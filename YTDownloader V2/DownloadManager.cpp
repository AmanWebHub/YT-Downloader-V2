#include "DownloadManager.h"

#include "DownloadState.h"
#include "DownloadUtils.h"
#include "DownloadWorker.h"

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
        if (url.empty())
        {
            MessageBoxW(
                ownerWindow,
                L"Please enter a video URL.",
                L"IT Downloader V2",
                MB_OK | MB_ICONWARNING);

            return false;
        }

        if (DownloadState::downloadRunning.exchange(true))
        {
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

        const std::wstring ytDlpPath =
            DownloadUtils::GetYtDlpPath();

        if (GetFileAttributesW(ytDlpPath.c_str()) ==
            INVALID_FILE_ATTRIBUTES)
        {
            DownloadState::downloadRunning = false;

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

        if (!DownloadUtils::EnsureFolderExists(downloadsFolder))
        {
            DownloadState::downloadRunning = false;

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
            DownloadWorker::Run,
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
        if (!DownloadState::downloadRunning)
        {
            return;
        }

        // Mark this as a permanent cancellation.
        // The worker will terminate the Job Object and then remove the
        // associated .part/.ytdl/.temp files.
        DownloadState::stopRequested = true;

        HANDLE job =
            DownloadState::jobHandle.load(
                std::memory_order_acquire);

        if (job != nullptr)
        {
            TerminateJobObject(job, 1);
            return;
        }

        // Fallback for the small window before the Job Object exists.
        HANDLE process =
            DownloadState::processHandle.load(
                std::memory_order_acquire);

        if (process != nullptr)
        {
            TerminateProcess(process, 1);
        }
    }

    void PauseDownload()
    {
        if (!DownloadState::downloadRunning)
        {
            return;
        }

        // Mark this as a pause rather than a cancellation.
        // The worker terminates yt-dlp but deliberately keeps partial files.
        DownloadState::pauseRequested = true;

        HANDLE job =
            DownloadState::jobHandle.load(
                std::memory_order_acquire);

        if (job != nullptr)
        {
            TerminateJobObject(job, 1);
            return;
        }

        // Fallback for the small window before the Job Object exists.
        HANDLE process =
            DownloadState::processHandle.load(
                std::memory_order_acquire);

        if (process != nullptr)
        {
            TerminateProcess(process, 1);
        }
    }
}
