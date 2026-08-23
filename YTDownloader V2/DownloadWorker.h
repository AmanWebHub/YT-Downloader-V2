#pragma once

#include <windows.h>
#include <string>

namespace DownloadWorker
{
    void Run(
        HWND ownerWindow,
        std::wstring url,
        bool isMp3,
        bool isPlaylist,
        std::wstring ytDlpPath,
        std::wstring downloadsFolder);
}
