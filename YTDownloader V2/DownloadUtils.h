#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace DownloadUtils
{
    std::wstring GetYtDlpPath();

    // Where a download actually gets saved. If the user has picked a
    // custom folder via Browse, that exact folder is used as-is (no
    // subfolder appended - they chose that specific location on
    // purpose). Otherwise falls back to the default
    // (%USERPROFILE%\Downloads\Video or \Music, split by format).
    std::wstring GetDownloadsFolder(
        bool isMp3);

    // The default base folder used when no custom folder is set:
    // %USERPROFILE%\Downloads. Only relevant to the default path -
    // see GetDownloadsFolder()'s comment for how a custom folder
    // differs (no \Video / \Music split).
    std::wstring GetDefaultDownloadBaseFolder();

    // Returns the user's custom base download folder, or an empty
    // string if none has been set (meaning: use the default).
    std::wstring GetCustomDownloadBaseFolder();

    // Sets the custom base download folder. Pass an empty string to
    // clear the override and revert to the default. Returns false
    // only if the registry write itself failed.
    bool SetCustomDownloadBaseFolder(
        const std::wstring& folder);

    // Convenience for UI display: the custom folder if one is set,
    // otherwise the default. NOTE: unlike GetDownloadsFolder(), this
    // never appends \Video or \Music - it's meant for showing/seeding
    // "what folder is currently active" in the Save To UI, not for
    // resolving where a specific download actually gets written.
    std::wstring GetActiveDownloadBaseFolder();

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