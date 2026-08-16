#pragma once

#include <windows.h>
#include <string>

// Owns all "what actually happens" logic - launching yt-dlp, tracking
// progress, etc. The UI layer (MainWindow) only ever calls into this;
// it never contains download logic itself.
namespace DownloadManager
{
    // ownerWindow is used only for parenting message boxes/dialogs.
    void StartDownload(HWND ownerWindow, const std::wstring& url, bool isMp3);
}
