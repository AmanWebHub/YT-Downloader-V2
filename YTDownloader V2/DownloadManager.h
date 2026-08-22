#pragma once

#include <windows.h>
#include <string>

// Messages sent from the background download worker to the main window.
constexpr UINT WM_APP_DOWNLOAD_PROGRESS = WM_APP + 1;
constexpr UINT WM_APP_DOWNLOAD_STATUS = WM_APP + 2;
constexpr UINT WM_APP_DOWNLOAD_FINISHED = WM_APP + 3;

// Used for failures that happen before yt-dlp even starts
// (pipe/process creation failure).
// This is distinct from any real yt-dlp exit code.
constexpr DWORD PRE_LAUNCH_FAILURE_CODE = 0xFFFFFFFF;

struct DownloadFinishedInfo
{
    DWORD exitCode = 1;

    bool isMp3 = false;
    bool wasPaused = false;
    bool wasCancelled = false;

    std::wstring downloadsFolder;
    std::wstring filePath;
};

namespace DownloadManager
{
    // Starts yt-dlp on a background thread.
    // The UI thread is never blocked.
    bool StartDownload(
        HWND ownerWindow,
        const std::wstring& url,
        bool isMp3,
        bool isPlaylist);

    // Fully stops the current download.
    //
    // The partial .part files belonging to the current download
    // are removed.
    //
    // This is a permanent cancellation and cannot be resumed.
    void CancelDownload();

    // Stops the current download but keeps the partial .part files.
    //
    // A later StartDownload() using the same URL will use yt-dlp's
    // --continue option to resume the download.
    void PauseDownload();
}