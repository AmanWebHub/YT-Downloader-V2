#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace DownloadUtils
{
    std::wstring GetYtDlpPath();

    std::wstring GetDownloadsFolder(
        bool isMp3);

    // The base folder downloads are saved under before the
    // \Video or \Music subfolder is appended. Defaults to
    // %USERPROFILE%\Downloads unless the user has set a custom
    // location (see Get/SetCustomDownloadBaseFolder below).
    std::wstring GetDefaultDownloadBaseFolder();

    // Returns the user's custom base download folder, or an empty
    // string if none has been set (meaning: use the default).
    std::wstring GetCustomDownloadBaseFolder();

    // Sets the custom base download folder. Pass an empty string to
    // clear the override and revert to the default. Returns false
    // only if the registry write itself failed.
    bool SetCustomDownloadBaseFolder(
        const std::wstring& folder);

    // Convenience: whichever folder GetDownloadsFolder() is
    // currently basing its \Video and \Music subfolders on - i.e.
    // the custom folder if one is set, otherwise the default.
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