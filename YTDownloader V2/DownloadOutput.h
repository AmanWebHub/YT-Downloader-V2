#pragma once

#include "DownloadManager.h"

#include <windows.h>
#include <string>
#include <vector>

namespace DownloadOutput
{
    bool TryParseProgress(
        const std::wstring& line,
        int& progress);

    void PostStatus(
        HWND ownerWindow,
        const std::wstring& text);

    void PostFinished(
        HWND ownerWindow,
        DWORD exitCode,
        bool isMp3,
        bool wasPaused,
        bool wasCancelled,
        const std::wstring& downloadsFolder,
        const std::wstring& filePath);

    void ProcessLine(
        HWND ownerWindow,
        const std::wstring& line,
        std::vector<std::wstring>& trackedDestinations,
        std::wstring& finalFileName);
}
