#include "DownloadManager.h"

namespace DownloadManager
{
    void StartDownload(HWND ownerWindow, const std::wstring& url, bool isMp3)
    {
        // Placeholder for now - this is where CreateProcess() will launch
        // yt-dlp with the right arguments once we wire that up next.
        std::wstring message = L"URL: ";
        message += url;
        message += L"\nFormat: ";
        message += isMp3 ? L"MP3 Audio" : L"MP4 Video";
        message += L"\n\n(Actual download logic comes next)";

        MessageBoxW(
            ownerWindow,
            message.c_str(),
            L"IT Downloader V2",
            MB_OK | MB_ICONINFORMATION
        );
    }
}
