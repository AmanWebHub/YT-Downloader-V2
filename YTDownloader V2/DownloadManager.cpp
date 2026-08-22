#include "DownloadManager.h"

#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <cwctype>
#include <shellapi.h>
#include <shlobj.h>

#pragma comment(lib, "shell32.lib")

namespace
{
    std::atomic<bool> g_downloadRunning{ false };
    std::atomic<bool> g_stopRequested{ false };
    std::atomic<bool> g_pauseRequested{ false };

    // The process handle and Job Object are owned by the worker thread.
    // The control functions only use them to signal/terminate the active
    // process tree.
    std::atomic<HANDLE> g_processHandle{ nullptr };
    std::atomic<HANDLE> g_jobHandle{ nullptr };

    std::wstring GetExeDirectory()
    {
        wchar_t exePath[MAX_PATH]{};
        DWORD length = GetModuleFileNameW(nullptr, exePath, MAX_PATH);

        if (length == 0)
        {
            return L".";
        }

        std::wstring path(exePath, length);
        const size_t lastSlash = path.find_last_of(L"\\/");

        return (lastSlash != std::wstring::npos)
            ? path.substr(0, lastSlash)
            : L".";
    }

    std::wstring GetYtDlpPath()
    {
        return GetExeDirectory() + L"\\bin\\yt-dlp.exe";
    }

    std::wstring GetDownloadsFolder(bool isMp3)
    {
        wchar_t* userProfile = nullptr;
        size_t len = 0;
        std::wstring folder;

        if (_wdupenv_s(&userProfile, &len, L"USERPROFILE") == 0 &&
            userProfile != nullptr)
        {
            folder = userProfile;
            free(userProfile);
        }

        if (folder.empty())
        {
            folder = L".";
        }

        return folder + (isMp3
            ? L"\\Downloads\\Music"
            : L"\\Downloads\\Video");
    }

    bool EnsureFolderExists(const std::wstring& folder)
    {
        const size_t slash = folder.find_last_of(L"\\/");

        if (slash != std::wstring::npos)
        {
            const std::wstring parent = folder.substr(0, slash);
            CreateDirectoryW(parent.c_str(), nullptr);
        }

        if (CreateDirectoryW(folder.c_str(), nullptr))
        {
            return true;
        }

        return GetLastError() == ERROR_ALREADY_EXISTS;
    }

    std::wstring Trim(const std::wstring& text)
    {
        size_t start = 0;

        while (start < text.size() && iswspace(text[start]))
        {
            ++start;
        }

        size_t end = text.size();

        while (end > start && iswspace(text[end - 1]))
        {
            --end;
        }

        return text.substr(start, end - start);
    }

    std::wstring FindNewestFileSince(
        const std::wstring& folder,
        const FILETIME& downloadStart)
    {
        std::wstring searchPattern = folder + L"\\*";
        WIN32_FIND_DATAW findData{};

        HANDLE findHandle =
            FindFirstFileW(searchPattern.c_str(), &findData);

        if (findHandle == INVALID_HANDLE_VALUE)
        {
            return L"";
        }

        std::wstring bestName;
        FILETIME bestTime{};

        static const wchar_t* skipExtensions[] = {
            L".part",
            L".ytdl",
            L".webp",
            L".description",
            L".json",
            L".temp"
        };

        do
        {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                continue;
            }

            std::wstring name = findData.cFileName;
            const size_t dotPos = name.find_last_of(L'.');

            if (dotPos != std::wstring::npos)
            {
                const std::wstring ext = name.substr(dotPos);
                bool skip = false;

                for (const wchar_t* skipExt : skipExtensions)
                {
                    if (_wcsicmp(ext.c_str(), skipExt) == 0)
                    {
                        skip = true;
                        break;
                    }
                }

                if (skip)
                {
                    continue;
                }
            }

            ULARGE_INTEGER fileTimeValue{};
            fileTimeValue.LowPart =
                findData.ftLastWriteTime.dwLowDateTime;
            fileTimeValue.HighPart =
                findData.ftLastWriteTime.dwHighDateTime;

            ULARGE_INTEGER startTimeValue{};
            startTimeValue.LowPart =
                downloadStart.dwLowDateTime;
            startTimeValue.HighPart =
                downloadStart.dwHighDateTime;

            // 2 seconds = 20,000,000 in 100-nanosecond FILETIME units.
            if (fileTimeValue.QuadPart + 20000000ULL <
                startTimeValue.QuadPart)
            {
                continue;
            }

            ULARGE_INTEGER bestTimeValue{};
            bestTimeValue.LowPart =
                bestTime.dwLowDateTime;
            bestTimeValue.HighPart =
                bestTime.dwHighDateTime;

            if (bestName.empty() ||
                fileTimeValue.QuadPart > bestTimeValue.QuadPart)
            {
                bestName = name;
                bestTime = findData.ftLastWriteTime;
            }

        } while (FindNextFileW(findHandle, &findData));

        FindClose(findHandle);

        return bestName.empty()
            ? L""
            : (folder + L"\\" + bestName);
    }

    bool TryParseProgress(
        const std::wstring& line,
        int& progress)
    {
        const size_t percentPos = line.find(L'%');

        if (percentPos == std::wstring::npos)
        {
            return false;
        }

        size_t end = percentPos;
        size_t start = end;

        while (start > 0)
        {
            const wchar_t c = line[start - 1];

            if ((c >= L'0' && c <= L'9') || c == L'.')
            {
                --start;
            }
            else
            {
                break;
            }
        }

        if (start == end)
        {
            return false;
        }

        try
        {
            const double value =
                std::stod(line.substr(start, end - start));

            if (value < 0.0 || value > 100.0)
            {
                return false;
            }

            progress = static_cast<int>(value);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void PostStatus(
        HWND ownerWindow,
        const std::wstring& text)
    {
        auto* message = new std::wstring(text);

        if (!PostMessageW(
            ownerWindow,
            WM_APP_DOWNLOAD_STATUS,
            0,
            reinterpret_cast<LPARAM>(message)))
        {
            delete message;
        }
    }

    void PostFinished(
        HWND ownerWindow,
        DWORD exitCode,
        bool isMp3,
        bool wasPaused,
        bool wasCancelled,
        const std::wstring& downloadsFolder,
        const std::wstring& filePath)
    {
        auto* info = new DownloadFinishedInfo;

        info->exitCode = exitCode;
        info->isMp3 = isMp3;
        info->wasPaused = wasPaused;
        info->wasCancelled = wasCancelled;
        info->downloadsFolder = downloadsFolder;
        info->filePath = filePath;

        if (!PostMessageW(
            ownerWindow,
            WM_APP_DOWNLOAD_FINISHED,
            0,
            reinterpret_cast<LPARAM>(info)))
        {
            delete info;
        }
    }

    // Creates a Job Object configured so that terminating the job also
    // terminates all child processes created by yt-dlp (for example ffmpeg).
    HANDLE CreateDownloadJob()
    {
        HANDLE job = CreateJobObjectW(nullptr, nullptr);

        if (job == nullptr)
        {
            return nullptr;
        }

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo{};

        jobInfo.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

        if (!SetInformationJobObject(
            job,
            JobObjectExtendedLimitInformation,
            &jobInfo,
            sizeof(jobInfo)))
        {
            CloseHandle(job);
            return nullptr;
        }

        return job;
    }

    void WorkerThread(
        HWND ownerWindow,
        std::wstring url,
        bool isMp3,
        bool isPlaylist,
        std::wstring ytDlpPath,
        std::wstring downloadsFolder)
    {
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
            PostStatus(
                ownerWindow,
                L"Failed to create the output pipe.");

            PostFinished(
                ownerWindow,
                PRE_LAUNCH_FAILURE_CODE,
                isMp3,
                false,
                false,
                downloadsFolder,
                L"");

            g_downloadRunning = false;
            return;
        }

        SetHandleInformation(
            readPipe,
            HANDLE_FLAG_INHERIT,
            0);

        std::wstring commandLine =
            L"\"" + ytDlpPath + L"\" "
            L"--newline "
            L"--continue ";

        if (isPlaylist)
        {
            commandLine +=
                L"--yes-playlist "
                L"--ignore-errors ";
        }
        else
        {
            commandLine +=
                L"--no-playlist ";
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
                L"-f \"bv*+ba/b\" ";
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

        std::vector<wchar_t> commandBuffer(
            commandLine.begin(),
            commandLine.end());

        commandBuffer.push_back(L'\0');

        STARTUPINFOW startupInfo{};

        startupInfo.cb =
            sizeof(startupInfo);

        startupInfo.dwFlags =
            STARTF_USESTDHANDLES;

        startupInfo.hStdInput =
            nullptr;

        startupInfo.hStdOutput =
            writePipe;

        startupInfo.hStdError =
            writePipe;

        PROCESS_INFORMATION processInfo{};

        FILETIME downloadStartTime{};
        GetSystemTimeAsFileTime(&downloadStartTime);

        const BOOL created = CreateProcessW(
            nullptr,
            commandBuffer.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startupInfo,
            &processInfo);

        CloseHandle(writePipe);
        writePipe = nullptr;

        if (!created)
        {
            const DWORD errorCode = GetLastError();

            CloseHandle(readPipe);

            std::wstring errorText =
                L"Failed to start yt-dlp.exe. Windows error code: " +
                std::to_wstring(errorCode) +
                L".";

            PostStatus(
                ownerWindow,
                errorText);

            PostFinished(
                ownerWindow,
                PRE_LAUNCH_FAILURE_CODE,
                isMp3,
                false,
                false,
                downloadsFolder,
                L"");

            g_downloadRunning = false;
            return;
        }

        CloseHandle(processInfo.hThread);

        // Create the Job Object after yt-dlp starts, then assign yt-dlp to it.
        // Once assigned, any ffmpeg process created by yt-dlp is part of the
        // same process tree controlled by the job.
        HANDLE jobHandle = CreateDownloadJob();

        if (jobHandle == nullptr)
        {
            TerminateProcess(processInfo.hProcess, 1);
            WaitForSingleObject(processInfo.hProcess, INFINITE);
            CloseHandle(processInfo.hProcess);
            CloseHandle(readPipe);

            PostStatus(
                ownerWindow,
                L"Failed to create the download process group.");

            PostFinished(
                ownerWindow,
                PRE_LAUNCH_FAILURE_CODE,
                isMp3,
                false,
                false,
                downloadsFolder,
                L"");

            g_downloadRunning = false;
            return;
        }

        if (!AssignProcessToJobObject(
            jobHandle,
            processInfo.hProcess))
        {
            const DWORD errorCode = GetLastError();

            CloseHandle(jobHandle);

            TerminateProcess(processInfo.hProcess, 1);
            WaitForSingleObject(processInfo.hProcess, INFINITE);
            CloseHandle(processInfo.hProcess);
            CloseHandle(readPipe);

            PostStatus(
                ownerWindow,
                L"Failed to attach yt-dlp to its process group. "
                L"Windows error code: " +
                std::to_wstring(errorCode) +
                L".");

            PostFinished(
                ownerWindow,
                PRE_LAUNCH_FAILURE_CODE,
                isMp3,
                false,
                false,
                downloadsFolder,
                L"");

            g_downloadRunning = false;
            return;
        }

        g_processHandle.store(
            processInfo.hProcess,
            std::memory_order_release);

        g_jobHandle.store(
            jobHandle,
            std::memory_order_release);

        PostStatus(
            ownerWindow,
            L"Starting download...");

        std::string buffer(4096, '\0');
        std::string pending;
        std::wstring finalFileName;

        bool cancellationSent = false;

        while (true)
        {
            if ((g_stopRequested || g_pauseRequested) &&
                !cancellationSent)
            {
                HANDLE activeJob =
                    g_jobHandle.load(
                        std::memory_order_acquire);

                if (activeJob != nullptr)
                {
                    // This terminates yt-dlp AND any child processes such
                    // as ffmpeg that it launched.
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

            DWORD bytesRead = 0;

            const BOOL readOk = ReadFile(
                readPipe,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytesRead,
                nullptr);

            if (!readOk || bytesRead == 0)
            {
                break;
            }

            pending.append(
                buffer.data(),
                bytesRead);

            size_t newlinePos =
                std::string::npos;

            while ((newlinePos =
                pending.find('\n')) !=
                std::string::npos)
            {
                std::string rawLine =
                    pending.substr(
                        0,
                        newlinePos);

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

                std::wstring line(
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

                line = Trim(line);

                int progress = 0;

                if (TryParseProgress(
                    line,
                    progress))
                {
                    PostMessageW(
                        ownerWindow,
                        WM_APP_DOWNLOAD_PROGRESS,
                        static_cast<WPARAM>(progress),
                        0);
                }

                if (line.find(
                    L"[download] Destination:") !=
                    std::wstring::npos)
                {
                    const size_t colon =
                        line.find(L':');

                    if (colon != std::wstring::npos)
                    {
                        const std::wstring destName =
                            Trim(line.substr(colon + 1));

                        PostStatus(
                            ownerWindow,
                            destName);

                        const size_t nameSlash =
                            destName.find_last_of(L"\\/");

                        finalFileName =
                            (nameSlash != std::wstring::npos)
                                ? destName.substr(nameSlash + 1)
                                : destName;
                    }
                }
                else if (line.find(
                    L"[ExtractAudio] Destination:") !=
                    std::wstring::npos)
                {
                    const size_t colon =
                        line.find(L':');

                    if (colon != std::wstring::npos)
                    {
                        const std::wstring destName =
                            Trim(line.substr(colon + 1));

                        const size_t nameSlash =
                            destName.find_last_of(L"\\/");

                        finalFileName =
                            (nameSlash != std::wstring::npos)
                                ? destName.substr(nameSlash + 1)
                                : destName;
                    }

                    PostStatus(
                        ownerWindow,
                        L"Converting audio...");
                }
                else if (line.find(
                    L"[Merger] Merging formats into") !=
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
                        const std::wstring destName =
                            line.substr(
                                firstQuote + 1,
                                lastQuote - firstQuote - 1);

                        const size_t nameSlash =
                            destName.find_last_of(L"\\/");

                        finalFileName =
                            (nameSlash != std::wstring::npos)
                                ? destName.substr(nameSlash + 1)
                                : destName;
                    }

                    PostStatus(
                        ownerWindow,
                        L"Merging video and audio...");
                }
                else if (line.find(L"ERROR:") !=
                         std::wstring::npos)
                {
                    PostStatus(
                        ownerWindow,
                        line);
                }
            }
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
            g_pauseRequested.load();

        const bool wasCancelled =
            g_stopRequested.load();

        g_processHandle.store(
            nullptr,
            std::memory_order_release);

        g_jobHandle.store(
            nullptr,
            std::memory_order_release);

        CloseHandle(
            processInfo.hProcess);

        // Closing the job handle also enforces the job's
        // KILL_ON_JOB_CLOSE behavior if anything unexpectedly remains.
        CloseHandle(jobHandle);

        std::wstring resolvedFilePath;

        if (wasPaused)
        {
            PostStatus(
                ownerWindow,
                L"Paused.");
        }
        else if (wasCancelled)
        {
            PostStatus(
                ownerWindow,
                L"Download cancelled.");
        }
        else if (exitCode == 0)
        {
            PostMessageW(
                ownerWindow,
                WM_APP_DOWNLOAD_PROGRESS,
                100,
                0);

            PostStatus(
                ownerWindow,
                L"Download complete.");

            if (!finalFileName.empty())
            {
                const std::wstring candidate =
                    downloadsFolder + L"\\" + finalFileName;

                if (GetFileAttributesW(candidate.c_str()) !=
                    INVALID_FILE_ATTRIBUTES)
                {
                    resolvedFilePath = candidate;
                }
            }

            if (resolvedFilePath.empty())
            {
                resolvedFilePath =
                    FindNewestFileSince(
                        downloadsFolder,
                        downloadStartTime);
            }
        }

        PostFinished(
            ownerWindow,
            exitCode,
            isMp3,
            wasPaused,
            wasCancelled,
            downloadsFolder,
            resolvedFilePath);

        g_stopRequested = false;
        g_pauseRequested = false;
        g_downloadRunning = false;
    }
}

namespace DownloadManager
{
    bool StartDownload(
        HWND ownerWindow,
        const std::wstring& url,
        bool isMp3,
        bool isPlaylist)
    {
        if (url.empty())
        {
            MessageBoxW(
                ownerWindow,
                L"Please enter a video URL.",
                L"IT Downloader V2",
                MB_OK | MB_ICONWARNING);

            return false;
        }

        if (g_downloadRunning.exchange(true))
        {
            MessageBoxW(
                ownerWindow,
                L"A download is already running.",
                L"IT Downloader V2",
                MB_OK | MB_ICONWARNING);

            return false;
        }

        g_stopRequested = false;
        g_pauseRequested = false;
        g_processHandle.store(nullptr);
        g_jobHandle.store(nullptr);

        const std::wstring ytDlpPath =
            GetYtDlpPath();

        if (GetFileAttributesW(
            ytDlpPath.c_str()) ==
            INVALID_FILE_ATTRIBUTES)
        {
            g_downloadRunning = false;

            std::wstring message =
                L"yt-dlp.exe was not found at:\n" +
                ytDlpPath +
                L"\n\nMake sure it's in a 'bin' folder next to this program.";

            MessageBoxW(
                ownerWindow,
                message.c_str(),
                L"IT Downloader V2",
                MB_OK | MB_ICONERROR);

            return false;
        }

        const std::wstring downloadsFolder =
            GetDownloadsFolder(isMp3);

        if (!EnsureFolderExists(downloadsFolder))
        {
            g_downloadRunning = false;

            std::wstring errorMessage =
                L"Unable to create the download folder:\n\n" +
                downloadsFolder;

            MessageBoxW(
                ownerWindow,
                errorMessage.c_str(),
                L"IT Downloader V2",
                MB_OK | MB_ICONERROR);

            return false;
        }

        std::thread(
            WorkerThread,
            ownerWindow,
            url,
            isMp3,
            isPlaylist,
            ytDlpPath,
            downloadsFolder)
            .detach();

        return true;
    }

    void CancelDownload()
    {
        if (!g_downloadRunning)
        {
            return;
        }

        g_stopRequested = true;

        HANDLE job =
            g_jobHandle.load(
                std::memory_order_acquire);

        if (job != nullptr)
        {
            TerminateJobObject(
                job,
                1);
            return;
        }

        HANDLE process =
            g_processHandle.load(
                std::memory_order_acquire);

        if (process != nullptr)
        {
            TerminateProcess(
                process,
                1);
        }
    }

    void PauseDownload()
    {
        if (!g_downloadRunning)
        {
            return;
        }

        g_pauseRequested = true;

        HANDLE job =
            g_jobHandle.load(
                std::memory_order_acquire);

        if (job != nullptr)
        {
            // IMPORTANT:
            // Pause is implemented as a clean stop of the entire yt-dlp
            // process tree. The partial .part file remains on disk.
            // Resume starts yt-dlp again with --continue.
            TerminateJobObject(
                job,
                1);
            return;
        }

        // Fallback for the very small window before the Job Object has
        // been created/assigned.
        HANDLE process =
            g_processHandle.load(
                std::memory_order_acquire);

        if (process != nullptr)
        {
            TerminateProcess(
                process,
                1);
        }
    }
}
