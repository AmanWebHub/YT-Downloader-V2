#pragma once

#include <windows.h>
#include <string>

// Messages sent from the background download worker to the main window.
constexpr UINT WM_APP_DOWNLOAD_PROGRESS = WM_APP + 1;
constexpr UINT WM_APP_DOWNLOAD_STATUS   = WM_APP + 2;
constexpr UINT WM_APP_DOWNLOAD_FINISHED = WM_APP + 3;

// Used for failures that happen before yt-dlp even starts (pipe/process
// creation failure) - distinct from any real exit code yt-dlp itself
// could return, so the UI can tell "our own setup failed, already
// explained in the status label" apart from "yt-dlp ran and failed."
constexpr DWORD PRE_LAUNCH_FAILURE_CODE = 0xFFFFFFFF;

struct DownloadFinishedInfo
{
    DWORD exitCode = 1;
    bool isMp3 = false;
    bool wasPaused = false; // true if this "finish" was actually a pause
    std::wstring downloadsFolder;
    std::wstring filePath; // full path to the downloaded file, empty if unknown
};

namespace DownloadManager
{
    // Starts yt-dlp on a background thread. The UI thread is never blocked.
    // Returns false if another download is already running or validation fails.
    // isPlaylist controls whether the whole playlist is grabbed (with
    // numbered filenames) or just the single video the URL points at.
    bool StartDownload(HWND ownerWindow, const std::wstring& url, bool isMp3, bool isPlaylist);

    // Fully stops the current download - it will NOT resume from here.
    void CancelDownload();

    // Stops the current download but marks it as paused - calling
    // StartDownload again with the same URL will resume it, since
    // yt-dlp continues partial (.part) files by default.
    void PauseDownload();
}
