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
    bool StartDownload(
        HWND ownerWindow,
        const std::wstring& url,
        bool isMp3,
        bool isPlaylist);

    void CancelDownload();

    void PauseDownload();
}
