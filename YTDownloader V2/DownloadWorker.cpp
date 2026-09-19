#include "DownloadWorker.h"

#include "DownloadJob.h"
#include "DownloadManager.h"
#include "DownloadOutput.h"
#include "DownloadResumeManifest.h"
#include "DownloadState.h"
#include "DownloadUtils.h"
#include "DownloadLogger.h"

#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <sstream>
#include <iomanip>
#include <thread>

// The resume/session-manifest subsystem (tracking what's been
// downloaded so pause/cancel/resume can clean up correctly) used to
// live in this file. It's now in DownloadResumeManifest.h/.cpp -
// this file focuses on actually launching yt-dlp, reading its
// output, and reporting progress/completion.
namespace
{

    bool ReadOutputLine(
        HANDLE readPipe,
        std::string& pending,
        std::wstring& line)
    {
        while (true)
        {
            const size_t newlinePos =
                pending.find('\n');

            if (newlinePos != std::string::npos)
            {
                std::string rawLine =
                    pending.substr(0, newlinePos);

                pending.erase(
                    0,
                    newlinePos + 1);

                if (!rawLine.empty() &&
                    rawLine.back() == '\r')
                {
                    rawLine.pop_back();
                }

                int wideLength =
                    MultiByteToWideChar(
                        CP_UTF8,
                        MB_ERR_INVALID_CHARS,
                        rawLine.data(),
                        static_cast<int>(rawLine.size()),
                        nullptr,
                        0);

                UINT codePage = CP_UTF8;

                if (wideLength <= 0)
                {
                    codePage = CP_ACP;

                    wideLength =
                        MultiByteToWideChar(
                            codePage,
                            0,
                            rawLine.data(),
                            static_cast<int>(rawLine.size()),
                            nullptr,
                            0);
                }

                if (wideLength <= 0)
                {
                    continue;
                }

                line.assign(
                    wideLength,
                    L'\0');

                MultiByteToWideChar(
                    codePage,
                    (codePage == CP_UTF8)
                        ? MB_ERR_INVALID_CHARS
                        : 0,
                    rawLine.data(),
                    static_cast<int>(rawLine.size()),
                    line.data(),
                    wideLength);

                line =
                    DownloadUtils::Trim(line);

                return true;
            }

            char buffer[4096]{};
            DWORD bytesRead = 0;

            const BOOL readOk =
                ReadFile(
                    readPipe,
                    buffer,
                    static_cast<DWORD>(sizeof(buffer)),
                    &bytesRead,
                    nullptr);

            if (!readOk || bytesRead == 0)
            {
                return false;
            }

            pending.append(
                buffer,
                bytesRead);
        }
    }

    std::wstring ExtractDestinationFromLine(
        const std::wstring& line)
    {
        const std::wstring markers[] =
        {
            L"[download] Destination:",
            L"[ExtractAudio] Destination:"
        };

        for (const std::wstring& marker : markers)
        {
            const size_t markerPos =
                line.find(marker);

            if (markerPos == std::wstring::npos)
            {
                continue;
            }

            return DownloadUtils::Trim(
                line.substr(markerPos + marker.size()));
        }

        const std::wstring printMarker =
            L"__ITD_FILE__:";

        const size_t printMarkerPos =
            line.find(printMarker);

        if (printMarkerPos != std::wstring::npos)
        {
            return DownloadUtils::Trim(
                line.substr(
                    printMarkerPos +
                    printMarker.size()));
        }

        if (line.find(L"[Merger] Merging formats into") !=
            std::wstring::npos)
        {
            const size_t firstQuote =
                line.find(L'"');

            const size_t lastQuote =
                line.find_last_of(L'"');

            if (firstQuote != std::wstring::npos &&
                lastQuote != std::wstring::npos &&
                lastQuote > firstQuote)
            {
                return line.substr(
                    firstQuote + 1,
                    lastQuote - firstQuote - 1);
            }
        }

        return L"";
    }

    std::wstring BuildCommandLine(
        const std::wstring& ytDlpPath,
        const std::wstring& url,
        bool isMp3,
        bool isPlaylist,
        const std::wstring& downloadsFolder)
    {
        std::wstring commandLine =
            L"\"" + ytDlpPath + L"\" "
            L"--newline "
            L"--print after_move:__ITD_FILE__:%(filepath)s "
            L"--no-quiet "
            // Bounds any single network operation (including format
            // resolution/testing) so a stalled request fails with a
            // clear error instead of hanging the download indefinitely
            // - e.g. when a format needs a JS-runtime-dependent check
            // (see yt-dlp's EJS requirement) and none is installed.
            L"--socket-timeout 30 "
            L"--retries 5 "
            L"--extractor-retries 3 ";

        if (isMp3)
        {
            commandLine += L"--continue ";
        }
        else
        {
            commandLine +=
                L"--no-continue "
                L"--force-overwrites ";
        }

        if (isPlaylist)
        {
            commandLine +=
                L"--yes-playlist "
                L"--ignore-errors ";
        }
        else
        {
            commandLine += L"--no-playlist ";
        }

        if (isMp3)
        {
            commandLine +=
                L"--extract-audio "
                L"--audio-format mp3 "
                L"--audio-quality 0 "
                L"--embed-thumbnail "
                L"--add-metadata ";
        }
        else
        {
            commandLine +=
                L"-f \"bv*[ext=mp4]+ba[ext=m4a]/b[ext=mp4]\" "
                L"--merge-output-format mp4 ";
        }

        commandLine +=
            L"-o \"" +
            downloadsFolder +
            (isPlaylist
                ? L"\\%(playlist_index)s - %(title)s.%(ext)s\" "
                : L"\\%(title)s.%(ext)s\" ") +
            L"\"" +
            url +
            L"\"";

        return commandLine;
    }

    void ResetState()
    {
        DownloadState::stopRequested = false;
        DownloadState::pauseRequested = false;
        DownloadState::downloadRunning = false;
        DownloadState::stalledByWatchdog = false;
    }

    // yt-dlp's bundled Python runtime picks its stdout/stderr text
    // encoding based on the environment it inherits. When stdout is
    // redirected to a pipe (as it is here, not a real console), that
    // choice is locale-dependent and on many Windows setups is NOT
    // UTF-8 - even though everything downstream in this app
    // (ReadOutputLine's MultiByteToWideChar(CP_UTF8, ...)) assumes it
    // is. Any video title containing a character yt-dlp has to
    // substitute for filename-safety then gets misread here, and the
    // wide filename we reconstruct silently diverges - byte for byte
    // - from the real file on disk, even though it looks identical or
    // near-identical when printed back out. That's what makes
    // GetFileAttributesW report "doesn't exist" for a file that does.
    //
    // Setting PYTHONUTF8=1 forces Python's UTF-8 mode (PEP 540)
    // regardless of the system locale/codepage, so the bytes we
    // receive are guaranteed to actually be UTF-8, matching what we
    // already assume when decoding.
    std::vector<wchar_t> BuildChildEnvironmentWithUtf8()
    {
        LPWCH parentEnv =
            GetEnvironmentStringsW();

        std::vector<wchar_t> block;

        if (parentEnv)
        {
            const wchar_t* p = parentEnv;

            while (*p)
            {
                const size_t len =
                    wcslen(p);

                // Skip any pre-existing PYTHONUTF8 /
                // PYTHONIOENCODING entries so ours are authoritative
                // and we don't end up with duplicate keys.
                if (_wcsnicmp(p, L"PYTHONUTF8=", 11) != 0 &&
                    _wcsnicmp(p, L"PYTHONIOENCODING=", 17) != 0)
                {
                    block.insert(
                        block.end(),
                        p,
                        p + len + 1);
                }

                p += len + 1;
            }

            FreeEnvironmentStringsW(parentEnv);
        }

        const std::wstring utf8Mode =
            L"PYTHONUTF8=1";

        block.insert(
            block.end(),
            utf8Mode.begin(),
            utf8Mode.end());

        block.push_back(L'\0');

        const std::wstring ioEncoding =
            L"PYTHONIOENCODING=utf-8";

        block.insert(
            block.end(),
            ioEncoding.begin(),
            ioEncoding.end());

        block.push_back(L'\0');

        // Double null terminator marks the end of the block.
        block.push_back(L'\0');

        return block;
    }
}

namespace DownloadWorker
{
    void Run(
        HWND ownerWindow,
        std::wstring url,
        bool isMp3,
        bool isPlaylist,
        std::wstring ytDlpPath,
        std::wstring downloadsFolder)
    {
        const std::wstring manifestPath =
            DownloadResumeManifest::BuildManifestPath(downloadsFolder);

        std::vector<std::wstring> manifestFiles;

        FILETIME originalStartTime{};
        bool hasOriginalStartTime = false;

        // Check for existing manifest
        DownloadResumeManifest::SessionInfo sessionInfo =
            DownloadResumeManifest::GetSessionInfo(
                manifestPath, url, isMp3, isPlaylist);
        if (sessionInfo.matches)
        {
            DownloadResumeManifest::LoadManifestFiles(manifestPath, manifestFiles);
            originalStartTime = sessionInfo.startTime;
            hasOriginalStartTime = true;
            DownloadLogger::Write(
                L"DownloadWorker",
                L"Resuming existing session with original start time.");
        }
        else
        {
            // New session: capture start time now
            GetSystemTimeAsFileTime(&originalStartTime);
            hasOriginalStartTime = true;
            if (!DownloadResumeManifest::CreateNewManifest(manifestPath, url, isMp3, isPlaylist, originalStartTime))
            {
                DownloadOutput::PostStatus(
                    ownerWindow,
                    L"Failed to create the download session file.");

                DownloadOutput::PostFinished(
                    ownerWindow,
                    PRE_LAUNCH_FAILURE_CODE,
                    isMp3,
                    false,
                    false,
                    downloadsFolder,
                    L"");

                ResetState();
                return;
            }
        }

        SECURITY_ATTRIBUTES securityAttributes{};
        securityAttributes.nLength =
            sizeof(securityAttributes);
        securityAttributes.bInheritHandle = TRUE;

        HANDLE readPipe = nullptr;
        HANDLE writePipe = nullptr;

        if (!CreatePipe(
                &readPipe,
                &writePipe,
                &securityAttributes,
                0))
        {
            DownloadOutput::PostStatus(
                ownerWindow,
                L"Failed to create the output pipe.");

            DownloadOutput::PostFinished(
                ownerWindow,
                PRE_LAUNCH_FAILURE_CODE,
                isMp3,
                false,
                false,
                downloadsFolder,
                L"");

            ResetState();
            return;
        }

        SetHandleInformation(
            readPipe,
            HANDLE_FLAG_INHERIT,
            0);

        const std::wstring commandLine =
            BuildCommandLine(
                ytDlpPath,
                url,
                isMp3,
                isPlaylist,
                downloadsFolder);

        DownloadLogger::Write(
            L"DownloadWorker",
            L"Launching yt-dlp with command line: " +
            commandLine);

        std::vector<wchar_t> commandBuffer(
            commandLine.begin(),
            commandLine.end());

        commandBuffer.push_back(L'\0');

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        startupInfo.dwFlags = STARTF_USESTDHANDLES;
        startupInfo.hStdInput = nullptr;
        startupInfo.hStdOutput = writePipe;
        startupInfo.hStdError = writePipe;

        PROCESS_INFORMATION processInfo{};
        FILETIME downloadStartTime{};
        GetSystemTimeAsFileTime(&downloadStartTime);

        std::vector<wchar_t> environmentBlock =
            BuildChildEnvironmentWithUtf8();

        // yt-dlp (or a library it uses internally - this traced back
        // to right after some crypto/TLS provider registry lookups)
        // can open a file using a bare filename with no directory
        // component, which Windows then resolves relative to the
        // process's current working directory. We never set one
        // explicitly here, so yt-dlp was just inheriting whatever
        // OUR OWN process's working directory happened to be - which
        // varies depending on how this app itself was launched, and
        // could land on a drive root (where a standard, non-admin
        // user can't create new files - hence the PermissionError
        // that "Run as administrator" appeared to "fix"). Pointing
        // it at our own bin folder instead - guaranteed to exist and
        // be writable, since yt-dlp.exe itself lives there - removes
        // that uncertainty entirely.
        const std::wstring ytDlpBinFolder =
            DownloadUtils::GetYtDlpBinFolder();

        const BOOL created =
            CreateProcessW(
                nullptr,
                commandBuffer.data(),
                nullptr,
                nullptr,
                TRUE,
                CREATE_NO_WINDOW |
                CREATE_UNICODE_ENVIRONMENT,
                environmentBlock.data(),
                ytDlpBinFolder.c_str(),
                &startupInfo,
                &processInfo);

        CloseHandle(writePipe);
        writePipe = nullptr;

        if (!created)
        {
            const DWORD errorCode =
                GetLastError();

            CloseHandle(readPipe);

            DownloadOutput::PostStatus(
                ownerWindow,
                L"Failed to start yt-dlp.exe. Windows error code: " +
                std::to_wstring(errorCode) +
                L".");

            DownloadOutput::PostFinished(
                ownerWindow,
                PRE_LAUNCH_FAILURE_CODE,
                isMp3,
                false,
                false,
                downloadsFolder,
                L"");

            ResetState();
            return;
        }

        CloseHandle(processInfo.hThread);

        HANDLE jobHandle =
            DownloadJob::CreateDownloadJob();

        if (jobHandle == nullptr)
        {
            TerminateProcess(
                processInfo.hProcess,
                1);

            WaitForSingleObject(
                processInfo.hProcess,
                INFINITE);

            CloseHandle(processInfo.hProcess);
            CloseHandle(readPipe);

            DownloadOutput::PostStatus(
                ownerWindow,
                L"Failed to create the download process group.");

            DownloadOutput::PostFinished(
                ownerWindow,
                PRE_LAUNCH_FAILURE_CODE,
                isMp3,
                false,
                false,
                downloadsFolder,
                L"");

            ResetState();
            return;
        }

        if (!AssignProcessToJobObject(
                jobHandle,
                processInfo.hProcess))
        {
            const DWORD errorCode =
                GetLastError();

            CloseHandle(jobHandle);

            TerminateProcess(
                processInfo.hProcess,
                1);

            WaitForSingleObject(
                processInfo.hProcess,
                INFINITE);

            CloseHandle(processInfo.hProcess);
            CloseHandle(readPipe);

            DownloadOutput::PostStatus(
                ownerWindow,
                L"Failed to attach yt-dlp to its process group. "
                L"Windows error code: " +
                std::to_wstring(errorCode) +
                L".");

            DownloadOutput::PostFinished(
                ownerWindow,
                PRE_LAUNCH_FAILURE_CODE,
                isMp3,
                false,
                false,
                downloadsFolder,
                L"");

            ResetState();
            return;
        }

        DownloadState::processHandle.store(
            processInfo.hProcess,
            std::memory_order_release);

        DownloadState::jobHandle.store(
            jobHandle,
            std::memory_order_release);

        DownloadOutput::PostStatus(
            ownerWindow,
            L"Starting download...");

        // Stall watchdog: yt-dlp's own --socket-timeout only bounds
        // its plain network reads - it does NOT cover every
        // operation it can block on (e.g. a format-testing step that
        // shells out to an external helper like a JS runtime). If
        // yt-dlp goes silent for too long, this treats it the same
        // as a user-initiated cancel: it sets stopRequested, which
        // the main loop below already knows how to act on
        // (TerminateJobObject, then the normal cleanup path). Runs
        // only while this download is active and stops itself the
        // moment that's no longer true.
        DownloadState::lastOutputTick.store(
            static_cast<long long>(GetTickCount64()));

        std::thread([]()
        {
            constexpr long long kStallThresholdMs = 25000;
            constexpr DWORD kPollIntervalMs = 2000;

            while (DownloadState::downloadRunning.load())
            {
                Sleep(kPollIntervalMs);

                if (!DownloadState::downloadRunning.load())
                {
                    break;
                }

                if (DownloadState::stopRequested.load() ||
                    DownloadState::pauseRequested.load())
                {
                    // Already being cancelled/paused for some other
                    // reason - nothing for the watchdog to do.
                    break;
                }

                const long long elapsed =
                    static_cast<long long>(GetTickCount64()) -
                    DownloadState::lastOutputTick.load();

                if (elapsed > kStallThresholdMs)
                {
                    DownloadLogger::Write(
                        L"DownloadWorker",
                        L"Watchdog: no output for over "
                        L"25s - treating as stalled, "
                        L"auto-cancelling.");

                    DownloadState::stalledByWatchdog =
                        true;

                    // Setting stopRequested alone isn't enough: the
                    // worker thread's own loop is what normally acts
                    // on that flag, but it's permanently blocked
                    // inside a blocking ReadFile() waiting for
                    // output that will never come while yt-dlp is
                    // stalled - so it never gets back around to
                    // check the flag. CancelDownload() is what a
                    // manual Cancel click calls, and it terminates
                    // the job/process directly from whichever thread
                    // calls it, which is exactly what's needed here
                    // to actually unblock that read.
                    DownloadManager::CancelDownload();

                    break;
                }
            }
        }).detach();

        std::string pending;
        std::wstring finalFileName;

        std::vector<std::wstring>
            trackedDestinations;

        bool cancellationSent = false;

        while (true)
        {
            if ((DownloadState::stopRequested ||
                 DownloadState::pauseRequested) &&
                !cancellationSent)
            {
                HANDLE activeJob =
                    DownloadState::jobHandle.load(
                        std::memory_order_acquire);

                if (activeJob != nullptr)
                {
                    TerminateJobObject(
                        activeJob,
                        1);
                }
                else
                {
                    TerminateProcess(
                        processInfo.hProcess,
                        1);
                }

                cancellationSent = true;
            }

            std::wstring line;

            if (!ReadOutputLine(
                    readPipe,
                    pending,
                    line))
            {
                break;
            }

            // Any output at all means yt-dlp is still alive and
            // working - reset the stall watchdog's clock.
            DownloadState::lastOutputTick.store(
                static_cast<long long>(GetTickCount64()));

            // Diagnostic: log every raw line from yt-dlp so we can see
            // exactly what it's printing (including the __ITD_FILE__
            // marker) and how that maps to the file the app resolves.
            DownloadLogger::Write(
                L"DownloadWorker",
                L"RAW OUTPUT: " + line);

            const std::wstring destination =
                ExtractDestinationFromLine(line);

            if (!destination.empty())
            {
                if (std::find(
                        trackedDestinations.begin(),
                        trackedDestinations.end(),
                        destination) ==
                    trackedDestinations.end())
                {
                    trackedDestinations.push_back(
                        destination);
                }

                DownloadResumeManifest::RecordKnownArtifacts(
                    manifestPath,
                    destination,
                    manifestFiles);
            }

            DownloadOutput::ProcessLine(
                ownerWindow,
                line,
                trackedDestinations,
                finalFileName);
        }

        CloseHandle(readPipe);

        WaitForSingleObject(
            processInfo.hProcess,
            INFINITE);

        DWORD exitCode = 1;

        GetExitCodeProcess(
            processInfo.hProcess,
            &exitCode);

        const bool wasPaused =
            DownloadState::pauseRequested.load();

        const bool wasCancelled =
            DownloadState::stopRequested.load();

        DownloadState::processHandle.store(
            nullptr,
            std::memory_order_release);

        DownloadState::jobHandle.store(
            nullptr,
            std::memory_order_release);

        CloseHandle(processInfo.hProcess);

        CloseHandle(jobHandle);

        std::wstring resolvedFilePath;

        if (wasPaused)
        {
            DownloadOutput::PostStatus(
                ownerWindow,
                L"Paused.");
        }
        else if (wasCancelled)
        {
            DownloadResumeManifest::CleanupCancelledSession(
                manifestPath,
                manifestFiles,
                trackedDestinations,
                downloadsFolder,
                originalStartTime);

            const bool wasStalled =
                DownloadState::stalledByWatchdog.load();

            DownloadOutput::PostStatus(
                ownerWindow,
                wasStalled
                    ? L"Download stalled and was cancelled "
                      L"automatically. Please try again."
                    : L"Download cancelled.");
        }
        else
        {
            // Determine if we have a valid final file, even if exitCode != 0 (for MP3)
            bool hasValidFile = false;

            // First, try using finalFileName from parser
            if (!finalFileName.empty())
            {
                const std::wstring candidate =
                    downloadsFolder + L"\\" + finalFileName;

                const bool candidateExists =
                    GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES;

                DownloadLogger::Write(
                    L"DownloadWorker",
                    L"Resolving final file: finalFileName=\"" +
                    finalFileName +
                    L"\" candidate=\"" +
                    candidate +
                    L"\" exists=" +
                    (candidateExists ? L"YES" : L"NO"));

                if (candidateExists)
                {
                    resolvedFilePath = candidate;
                    hasValidFile = true;
                }
            }
            else
            {
                DownloadLogger::Write(
                    L"DownloadWorker",
                    L"Resolving final file: finalFileName is EMPTY.");
            }

            // Fall back to scanning the filesystem directly for the
            // newest real output file created since this download
            // started. This applies regardless of format (mp3 or
            // video/mp4): trying to reconstruct the exact filename
            // from yt-dlp's console text is unreliable whenever the
            // title contains a character yt-dlp had to substitute
            // for filename-safety (observed: yt-dlp prints a
            // fullwidth "｜" in that case, but by the time it comes
            // through our pipe and gets decoded, that character is
            // gone). FindFirstFileW/FindNextFileW read the actual
            // NTFS filename directly as UTF-16, so this sidesteps
            // that problem entirely rather than trying to out-guess
            // it - the same approach already proven to work for the
            // MP3 case below.
            if (!hasValidFile)
            {
                const std::wstring newestFile =
                    DownloadUtils::FindNewestFileSince(
                        downloadsFolder,
                        originalStartTime,
                        manifestPath);

                DownloadLogger::Write(
                    L"DownloadWorker",
                    L"Filesystem fallback scan: newestFile=\"" +
                    newestFile +
                    L"\"");

                if (!newestFile.empty())
                {
                    resolvedFilePath = newestFile;
                    hasValidFile = true;

                    const size_t slash =
                        newestFile.find_last_of(L"\\/");

                    finalFileName =
                        (slash != std::wstring::npos)
                        ? newestFile.substr(slash + 1)
                        : newestFile;
                }
            }

            // Now decide what to do
            if (exitCode == 0 || hasValidFile)
            {
                // Success path
                PostMessageW(
                    ownerWindow,
                    WM_APP_DOWNLOAD_PROGRESS,
                    100,
                    0);

                DownloadOutput::PostStatus(
                    ownerWindow,
                    L"Download complete.");

                // If we didn't have resolvedFilePath from earlier, we have it now
                // (but we already set it above)

                DeleteFileW(manifestPath.c_str());

                if (isMp3)
                {
                    // Pass 1: remove WebM files tracked in the manifest
                    DownloadResumeManifest::CleanupCompletedMp3IntermediateFiles(
                        manifestFiles,
                        trackedDestinations,
                        downloadsFolder,
                        originalStartTime);

                    Sleep(100);

                    // Pass 2: scan with tracked destinations
                    DownloadResumeManifest::CleanupCompletedMp3IntermediateFiles(
                        {},
                        trackedDestinations,
                        downloadsFolder,
                        originalStartTime);

                    // Pass 3: leftover .webm matching final stem
                    DownloadResumeManifest::CleanupLeftoverWebmFiles(
                        downloadsFolder,
                        resolvedFilePath,
                        originalStartTime);
                }

                DeleteFileW(manifestPath.c_str());
            }
            else
            {
                // Real failure
                DownloadOutput::PostStatus(
                    ownerWindow,
                    L"Download failed.");

                // Clean up partial files
                DownloadResumeManifest::CleanupCancelledSession(
                    manifestPath,
                    manifestFiles,
                    trackedDestinations,
                    downloadsFolder,
                    originalStartTime);

                // We'll still send a failure, but resolvedFilePath empty
                resolvedFilePath.clear();
            }
        }

        DownloadLogger::Write(
            L"DownloadWorker",
            L"FINAL DECISION: resolvedFilePath=\"" +
            resolvedFilePath +
            L"\" downloadsFolder=\"" +
            downloadsFolder +
            L"\" (CompletionWindow will use resolvedFilePath if " +
            L"non-empty, otherwise it falls back to downloadsFolder)");

        DownloadOutput::PostFinished(
            ownerWindow,
            exitCode,
            isMp3,
            wasPaused,
            wasCancelled,
            downloadsFolder,
            resolvedFilePath);

        ResetState();
    }
}