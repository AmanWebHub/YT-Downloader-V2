#pragma once

#include <windows.h>
#include <string>

// Messages sent from the background download worker to the main window.
constexpr UINT WM_APP_DOWNLOAD_PROGRESS = WM_APP + 1;
constexpr UINT WM_APP_DOWNLOAD_STATUS   = WM_APP + 2;
constexpr UINT WM_APP_DOWNLOAD_FINISHED = WM_APP + 3;

struct DownloadFinishedInfo
{
    DWORD exitCode = 1;
    bool isMp3 = false;
    std::wstring downloadsFolder;
};

namespace DownloadManager
{
    // Starts yt-dlp on a background thread. The UI thread is never blocked.
    // Returns false if another download is already running or validation fails.
    bool StartDownload(HWND ownerWindow, const std::wstring& url, bool isMp3);

    // Stops the current download if the main window is being closed.
    void StopDownload();
}
