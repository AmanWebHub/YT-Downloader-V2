#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace DownloadUtils
{
    std::wstring GetYtDlpPath();
    std::wstring GetDownloadsFolder(bool isMp3);
    bool EnsureFolderExists(const std::wstring& folder);
    std::wstring Trim(const std::wstring& text);

    std::wstring FindNewestFileSince(
        const std::wstring& folder,
        const FILETIME& downloadStart);

    bool FileExists(const std::wstring& path);

    void CleanupCancelledDownload(
        const std::wstring& downloadsFolder,
        const FILETIME& downloadStart,
        const std::vector<std::wstring>& destinations);
}
