#include "DownloadWorker.h"

#include "DownloadJob.h"
#include "DownloadManager.h"
#include "DownloadOutput.h"
#include "DownloadState.h"
#include "DownloadUtils.h"

#include <windows.h>
#include <string>
#include <vector>

namespace
{
    bool ReadOutputLine(
        HANDLE readPipe,
        std::string& pending,
        std::wstring& line)
    {
        while (true)
        {
            const size_t newlinePos = pending.find('\n');

            if (newlinePos != std::string::npos)
            {
                std::string rawLine =
                    pending.substr(0, newlinePos);

                pending.erase(0, newlinePos + 1);

                if (!rawLine.empty() && rawLine.back() == '\r')
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

                line.assign(wideLength, L'\0');

                MultiByteToWideChar(
                    codePage,
                    (codePage == CP_UTF8)
                        ? MB_ERR_INVALID_CHARS
                        : 0,
                    rawLine.data(),
                    static_cast<int>(rawLine.size()),
                    line.data(),
                    wideLength);

                line = DownloadUtils::Trim(line);
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

            pending.append(buffer, bytesRead);
        }
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
            L"--continue ";

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
            commandLine += L"-f \"bv*+ba/b\" ";
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
        SECURITY_ATTRIBUTES securityAttributes{};
        securityAttributes.nLength = sizeof(securityAttributes);
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

        const BOOL created =
            CreateProcessW(
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

        HANDLE jobHandle = DownloadJob::CreateDownloadJob();

        if (jobHandle == nullptr)
        {
            TerminateProcess(processInfo.hProcess, 1);
            WaitForSingleObject(processInfo.hProcess, INFINITE);
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
            const DWORD errorCode = GetLastError();

            CloseHandle(jobHandle);

            TerminateProcess(processInfo.hProcess, 1);
            WaitForSingleObject(processInfo.hProcess, INFINITE);
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

        std::string pending;
        std::wstring finalFileName;
        std::vector<std::wstring> trackedDestinations;

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
                    TerminateJobObject(activeJob, 1);
                }
                else
                {
                    TerminateProcess(processInfo.hProcess, 1);
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

        // Closing the job handle also enforces KILL_ON_JOB_CLOSE if anything
        // unexpectedly remains.
        CloseHandle(jobHandle);

        std::wstring resolvedFilePath;

        if (wasPaused)
        {
            // Do NOT clean up .part files here. Resume depends on them.
            DownloadOutput::PostStatus(
                ownerWindow,
                L"Paused.");
        }
        else if (wasCancelled)
        {
            // Cancellation is permanent. Remove partial files belonging to
            // this download, including playlist partial files.
            DownloadUtils::CleanupCancelledDownload(
                downloadsFolder,
                downloadStartTime,
                trackedDestinations);

            DownloadOutput::PostStatus(
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

            DownloadOutput::PostStatus(
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
                    DownloadUtils::FindNewestFileSince(
                        downloadsFolder,
                        downloadStartTime);
            }
        }

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
