#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace DownloadUtils
{
    std::wstring GetYtDlpPath();

    std::wstring GetDownloadsFolder(
        bool isMp3);

    bool EnsureFolderExists(
        const std::wstring& folder);

    std::wstring Trim(
        const std::wstring& text);

    // Accepts either a plain URL or a URL wrapped in the app's
    // "ytdlp://" custom protocol handler (as sent by the browser
    // extension, e.g. "ytdlp://" + encodeURIComponent(videoUrl)).
    // Strips the scheme prefix and percent-decodes the remainder.
    // If the input does not use the ytdlp:// scheme, it is returned
    // trimmed and otherwise unchanged.
    std::wstring DecodeExternalUrl(
        const std::wstring& rawArgument);

    std::wstring FindNewestFileSince(
        const std::wstring& folder,
        const FILETIME& downloadStart,
        const std::wstring& excludePath = L"");

    bool FileExists(
        const std::wstring& path);

    void CleanupCancelledDownload(
        const std::wstring& downloadsFolder,
        const FILETIME& downloadStart,
        const std::vector<std::wstring>& destinations);

    void CleanupCompletedDownload(
        const std::vector<std::wstring>& destinations,
        const std::wstring& finalFilePath);
}