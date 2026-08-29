#include "DownloadManager.h"

#include "DownloadState.h"
#include "DownloadUtils.h"
#include "DownloadWorker.h"
#include "DownloadLogger.h"

#include <thread>

#pragma comment(lib, "shell32.lib")

namespace DownloadManager
{
    bool StartDownload(
        HWND ownerWindow,
        const std::wstring& url,
        bool isMp3,
        bool isPlaylist)
    {
        DownloadLogger::Write(
            L"DownloadManager",
            L"StartDownload() called.");

        if (url.empty())
        {
            DownloadLogger::Write(
                L"DownloadManager",
                L"StartDownload() rejected: URL is empty.");

            MessageBoxW(
                ownerWindow,
                L"Please enter a video URL.",
                L"IT Downloader V2",
                MB_OK | MB_ICONWARNING);

            return false;
        }

        if (DownloadState::downloadRunning.exchange(true))
        {
            DownloadLogger::Write(
                L"DownloadManager",
                L"StartDownload() rejected: another download is already running.");

            MessageBoxW(
                ownerWindow,
                L"A download is already running.",
                L"IT Downloader V2",
                MB_OK | MB_ICONWARNING);

            return false;
        }

        DownloadState::stopRequested = false;
        DownloadState::pauseRequested = false;
        DownloadState::processHandle.store(nullptr);
        DownloadState::jobHandle.store(nullptr);

        DownloadLogger::Write(
            L"DownloadManager",
            L"Download state initialized.");

        const std::wstring ytDlpPath =
            DownloadUtils::GetYtDlpPath();

        DownloadLogger::Write(
            L"DownloadManager",
            L"yt-dlp path: " + ytDlpPath);

        if (GetFileAttributesW(ytDlpPath.c_str()) ==
            INVALID_FILE_ATTRIBUTES)
        {
            DownloadState::downloadRunning = false;

            DownloadLogger::Write(
                L"DownloadManager",
                L"yt-dlp.exe was not found.");

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
            DownloadUtils::GetDownloadsFolder(isMp3);

        DownloadLogger::Write(
            L"DownloadManager",
            L"Download folder: " + downloadsFolder);

        if (!DownloadUtils::EnsureFolderExists(downloadsFolder))
        {
            DownloadState::downloadRunning = false;

            DownloadLogger::Write(
                L"DownloadManager",
                L"Unable to create download folder.");

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

        DownloadLogger::Write(
            L"DownloadManager",
            L"Starting DownloadWorker thread.");

        std::thread(
            DownloadWorker::Run,
            ownerWindow,
            url,
            isMp3,
            isPlaylist,
            ytDlpPath,
            downloadsFolder)
            .detach();

        DownloadLogger::Write(
            L"DownloadManager",
            L"DownloadWorker thread detached.");

        return true;
    }

    void CancelDownload()
    {
        DownloadLogger::Write(
            L"DownloadManager",
            L"CancelDownload() called.");

        if (!DownloadState::downloadRunning)
        {
            DownloadLogger::Write(
                L"DownloadManager",
                L"Cancel ignored: no download is running.");

            return;
        }

        DownloadState::stopRequested = true;

        DownloadLogger::Write(
            L"DownloadManager",
            L"stopRequested = TRUE.");

        HANDLE job =
            DownloadState::jobHandle.load(
                std::memory_order_acquire);

        if (job != nullptr)
        {
            DownloadLogger::Write(
                L"DownloadManager",
                L"Calling TerminateJobObject().");

            const BOOL result =
                TerminateJobObject(job, 1);

            DownloadLogger::Write(
                L"DownloadManager",
                result
                    ? L"TerminateJobObject() returned SUCCESS."
                    : L"TerminateJobObject() FAILED.");

            return;
        }

        DownloadLogger::Write(
            L"DownloadManager",
            L"Job Object not available; using process fallback.");

        HANDLE process =
            DownloadState::processHandle.load(
                std::memory_order_acquire);

        if (process != nullptr)
        {
            DownloadLogger::Write(
                L"DownloadManager",
                L"Calling TerminateProcess() fallback.");

            const BOOL result =
                TerminateProcess(process, 1);

            DownloadLogger::Write(
                L"DownloadManager",
                result
                    ? L"TerminateProcess() returned SUCCESS."
                    : L"TerminateProcess() FAILED.");
        }
        else
        {
            DownloadLogger::Write(
                L"DownloadManager",
                L"No process handle available for cancellation.");
        }
    }

    void PauseDownload()
    {
        DownloadLogger::Write(
            L"DownloadManager",
            L"PauseDownload() called.");

        if (!DownloadState::downloadRunning)
        {
            DownloadLogger::Write(
                L"DownloadManager",
                L"Pause ignored: no download is running.");

            return;
        }

        DownloadState::pauseRequested = true;

        DownloadLogger::Write(
            L"DownloadManager",
            L"pauseRequested = TRUE.");

        HANDLE job =
            DownloadState::jobHandle.load(
                std::memory_order_acquire);

        if (job != nullptr)
        {
            DownloadLogger::Write(
                L"DownloadManager",
                L"Calling TerminateJobObject() for pause.");

            const BOOL result =
                TerminateJobObject(job, 1);

            DownloadLogger::Write(
                L"DownloadManager",
                result
                    ? L"TerminateJobObject() for pause returned SUCCESS."
                    : L"TerminateJobObject() for pause FAILED.");

            return;
        }

        HANDLE process =
            DownloadState::processHandle.load(
                std::memory_order_acquire);

        if (process != nullptr)
        {
            DownloadLogger::Write(
                L"DownloadManager",
                L"Calling TerminateProcess() fallback for pause.");

            const BOOL result =
                TerminateProcess(process, 1);

            DownloadLogger::Write(
                L"DownloadManager",
                result
                    ? L"TerminateProcess() for pause returned SUCCESS."
                    : L"TerminateProcess() for pause FAILED.");
        }
        else
        {
            DownloadLogger::Write(
                L"DownloadManager",
                L"No process handle available for pause.");
        }
    }
}
