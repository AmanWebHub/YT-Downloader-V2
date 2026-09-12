#include "DownloadUtils.h"

#include <cstdlib>
#include <cwctype>

#pragma comment(lib, "advapi32.lib")

namespace
{
    std::wstring GetExeDirectory()
    {
        wchar_t exePath[MAX_PATH]{};

        DWORD length =
            GetModuleFileNameW(
                nullptr,
                exePath,
                MAX_PATH);

        if (length == 0)
        {
            return L".";
        }

        std::wstring path(
            exePath,
            length);

        const size_t lastSlash =
            path.find_last_of(L"\\/");

        return
            (lastSlash != std::wstring::npos)
            ? path.substr(0, lastSlash)
            : L".";
    }

    bool IsSamePath(
        const std::wstring& first,
        const std::wstring& second)
    {
        if (first.empty() ||
            second.empty())
        {
            return false;
        }

        return _wcsicmp(
            first.c_str(),
            second.c_str()) == 0;
    }

    bool HasExtension(
        const std::wstring& path,
        const wchar_t* extension)
    {
        const size_t dotPos =
            path.find_last_of(L'.');

        if (dotPos == std::wstring::npos)
        {
            return false;
        }

        return _wcsicmp(
            path.substr(dotPos).c_str(),
            extension) == 0;
    }

    void DeleteFileIfExists(
        const std::wstring& path)
    {
        if (path.empty() ||
            !DownloadUtils::FileExists(path))
        {
            return;
        }

        DeleteFileW(
            path.c_str());
    }

    void CleanupTrackedPartialFiles(
        const std::vector<std::wstring>& destinations)
    {
        for (const std::wstring& destination :
            destinations)
        {
            if (destination.empty())
            {
                continue;
            }

            /*
                Standard yt-dlp partial files.
            */
            DeleteFileIfExists(
                destination + L".part");

            DeleteFileIfExists(
                destination + L".ytdl");

            DeleteFileIfExists(
                destination + L".temp");

            /*
                During MP3 extraction, WebM/WebP can be
                intermediate files.

                These paths are safe to remove here because
                they were explicitly reported by yt-dlp during
                this download.
            */
            if (HasExtension(
                destination,
                L".webm") ||
                HasExtension(
                    destination,
                    L".webp"))
            {
                DeleteFileIfExists(
                    destination);
            }
        }
    }

    void CleanupTrackedIntermediateFiles(
        const std::vector<std::wstring>& destinations,
        const std::wstring& finalFilePath)
    {
        for (const std::wstring& destination :
            destinations)
        {
            if (destination.empty())
            {
                continue;
            }

            /*
                NEVER delete the final output.
            */
            if (IsSamePath(
                destination,
                finalFilePath))
            {
                continue;
            }

            /*
                Remove standard temporary files.
            */
            DeleteFileIfExists(
                destination + L".part");

            DeleteFileIfExists(
                destination + L".ytdl");

            DeleteFileIfExists(
                destination + L".temp");

            /*
                WebM/WebP are intermediate files for the
                current MP3 extraction workflow.

                Only delete them when they were explicitly
                tracked as part of THIS download.
            */
            if (HasExtension(
                destination,
                L".webm") ||
                HasExtension(
                    destination,
                    L".webp"))
            {
                DeleteFileIfExists(
                    destination);
            }
        }
    }

    bool IsPartialExtension(
        const std::wstring& extension)
    {
        return
            _wcsicmp(
                extension.c_str(),
                L".part") == 0 ||
            _wcsicmp(
                extension.c_str(),
                L".ytdl") == 0 ||
            _wcsicmp(
                extension.c_str(),
                L".temp") == 0;
    }

    void CleanupRecentPartialFiles(
        const std::wstring& folder,
        const FILETIME& downloadStart)
    {
        const std::wstring searchPattern =
            folder + L"\\*";

        WIN32_FIND_DATAW findData{};

        HANDLE findHandle =
            FindFirstFileW(
                searchPattern.c_str(),
                &findData);

        if (findHandle ==
            INVALID_HANDLE_VALUE)
        {
            return;
        }

        ULARGE_INTEGER startTimeValue{};

        startTimeValue.LowPart =
            downloadStart.dwLowDateTime;

        startTimeValue.HighPart =
            downloadStart.dwHighDateTime;

        do
        {
            if (findData.dwFileAttributes &
                FILE_ATTRIBUTE_DIRECTORY)
            {
                continue;
            }

            const std::wstring name =
                findData.cFileName;

            const size_t dotPos =
                name.find_last_of(L'.');

            if (dotPos == std::wstring::npos)
            {
                continue;
            }

            const std::wstring extension =
                name.substr(dotPos);

            if (!IsPartialExtension(
                extension))
            {
                continue;
            }

            ULARGE_INTEGER fileTimeValue{};

            fileTimeValue.LowPart =
                findData.ftLastWriteTime.dwLowDateTime;

            fileTimeValue.HighPart =
                findData.ftLastWriteTime.dwHighDateTime;

            /*
                Allow a small two-second tolerance because
                Windows filesystem timestamps can differ slightly
                from the process start time.
            */
            if (fileTimeValue.QuadPart +
                20000000ULL <
                startTimeValue.QuadPart)
            {
                continue;
            }

            DeleteFileIfExists(
                folder + L"\\" + name);

        } while (FindNextFileW(
            findHandle,
            &findData));

        FindClose(findHandle);
    }
}

namespace DownloadUtils
{
    std::wstring GetYtDlpPath()
    {
        return GetExeDirectory() +
            L"\\bin\\yt-dlp.exe";
    }

    namespace
    {
        const wchar_t* const kSettingsKeyPath =
            L"Software\\YTDownloaderV2";

        const wchar_t* const kDownloadFolderValueName =
            L"DownloadFolder";
    }

    std::wstring GetDefaultDownloadBaseFolder()
    {
        wchar_t* userProfile = nullptr;
        size_t len = 0;
        std::wstring folder;

        if (_wdupenv_s(
            &userProfile,
            &len,
            L"USERPROFILE") == 0 &&
            userProfile != nullptr)
        {
            folder = userProfile;
            free(userProfile);
        }

        if (folder.empty())
        {
            folder = L".";
        }

        return folder + L"\\Downloads";
    }

    std::wstring GetCustomDownloadBaseFolder()
    {
        HKEY key = nullptr;

        if (RegOpenKeyExW(
            HKEY_CURRENT_USER,
            kSettingsKeyPath,
            0,
            KEY_READ,
            &key) != ERROR_SUCCESS)
        {
            return L"";
        }

        wchar_t buffer[MAX_PATH]{};
        DWORD bufferSize = sizeof(buffer);
        DWORD type = 0;

        const LONG result =
            RegQueryValueExW(
                key,
                kDownloadFolderValueName,
                nullptr,
                &type,
                reinterpret_cast<LPBYTE>(buffer),
                &bufferSize);

        RegCloseKey(key);

        if (result != ERROR_SUCCESS ||
            type != REG_SZ)
        {
            return L"";
        }

        return Trim(
            std::wstring(buffer));
    }

    bool SetCustomDownloadBaseFolder(
        const std::wstring& folder)
    {
        const std::wstring trimmed =
            Trim(folder);

        HKEY key = nullptr;

        if (trimmed.empty())
        {
            // Clear the override so GetDownloadsFolder() falls back
            // to the default. Missing key/value is not an error.
            if (RegOpenKeyExW(
                HKEY_CURRENT_USER,
                kSettingsKeyPath,
                0,
                KEY_SET_VALUE,
                &key) != ERROR_SUCCESS)
            {
                return true;
            }

            RegDeleteValueW(
                key,
                kDownloadFolderValueName);

            RegCloseKey(key);
            return true;
        }

        if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            kSettingsKeyPath,
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE,
            nullptr,
            &key,
            nullptr) != ERROR_SUCCESS)
        {
            return false;
        }

        const LONG result =
            RegSetValueExW(
                key,
                kDownloadFolderValueName,
                0,
                REG_SZ,
                reinterpret_cast<const BYTE*>(trimmed.c_str()),
                static_cast<DWORD>(
                    (trimmed.size() + 1) * sizeof(wchar_t)));

        RegCloseKey(key);

        return result == ERROR_SUCCESS;
    }

    std::wstring GetActiveDownloadBaseFolder()
    {
        const std::wstring custom =
            GetCustomDownloadBaseFolder();

        return custom.empty()
            ? GetDefaultDownloadBaseFolder()
            : custom;
    }

    std::wstring GetDownloadsFolder(
        bool isMp3)
    {
        return GetActiveDownloadBaseFolder() +
            (isMp3
                ? L"\\Music"
                : L"\\Video");
    }

    bool EnsureFolderExists(
        const std::wstring& folder)
    {
        const size_t slash =
            folder.find_last_of(L"\\/");

        if (slash != std::wstring::npos)
        {
            const std::wstring parent =
                folder.substr(0, slash);

            CreateDirectoryW(
                parent.c_str(),
                nullptr);
        }

        if (CreateDirectoryW(
            folder.c_str(),
            nullptr))
        {
            return true;
        }

        return GetLastError() ==
            ERROR_ALREADY_EXISTS;
    }

    std::wstring Trim(
        const std::wstring& text)
    {
        size_t start = 0;

        while (start < text.size() &&
            iswspace(text[start]))
        {
            ++start;
        }

        size_t end = text.size();

        while (end > start &&
            iswspace(text[end - 1]))
        {
            --end;
        }

        return text.substr(
            start,
            end - start);
    }

    std::wstring DecodeExternalUrl(
        const std::wstring& rawArgument)
    {
        // Defensive: some browser/OS combinations have been observed
        // to prepend a stray "/" onto the activated protocol string.
        // A legitimate "ytdlp:" or "ytdlp://" argument never starts
        // with "/", so it's always safe to strip leading slashes
        // before looking for our scheme prefix.
        std::wstring text =
            Trim(rawArgument);

        while (!text.empty() &&
            text.front() == L'/')
        {
            text.erase(
                text.begin());
        }

        // Recognize both the current opaque form ("ytdlp:...") and
        // the older hierarchical form ("ytdlp://...") for backwards
        // compatibility with any extension build still using it.
        // Checked longest-first so "ytdlp://" isn't misread as
        // "ytdlp:" plus a leftover "//".
        static const wchar_t* const kPrefixes[] =
        {
            L"ytdlp://",
            L"ytdlp:",
        };

        const wchar_t* matchedPrefix = nullptr;

        for (const wchar_t* candidate : kPrefixes)
        {
            const size_t candidateLen =
                wcslen(candidate);

            if (text.size() >= candidateLen &&
                _wcsnicmp(
                    text.c_str(),
                    candidate,
                    candidateLen) == 0)
            {
                matchedPrefix = candidate;
                break;
            }
        }

        if (!matchedPrefix)
        {
            // Not our custom protocol (e.g. a plain URL passed on
            // the command line, or dragged/typed input) - leave as-is.
            return text;
        }

        std::wstring encoded =
            text.substr(
                wcslen(matchedPrefix));

        // Defensive: strip a stray trailing "/" some browsers add
        // when canonicalizing the hierarchical "ytdlp://" form (see
        // the comment in the extension's content.js). Our own
        // encodeURIComponent() output never contains a raw "/", so
        // any trailing one here is browser-added noise, not data.
        while (!encoded.empty() &&
            encoded.back() == L'/')
        {
            encoded.pop_back();
        }

        auto hexValue = [](wchar_t c) -> int
        {
            if (c >= L'0' && c <= L'9') return c - L'0';
            if (c >= L'a' && c <= L'f') return 10 + (c - L'a');
            if (c >= L'A' && c <= L'F') return 10 + (c - L'A');
            return -1;
        };

        // encodeURIComponent() produces plain-ASCII, percent-encoded
        // UTF-8 bytes, so we can rebuild the raw UTF-8 byte sequence
        // directly from the wide characters here.
        std::string utf8Bytes;
        utf8Bytes.reserve(encoded.size());

        for (size_t i = 0; i < encoded.size(); ++i)
        {
            const wchar_t ch = encoded[i];

            if (ch == L'%' &&
                i + 2 < encoded.size())
            {
                const int high = hexValue(encoded[i + 1]);
                const int low = hexValue(encoded[i + 2]);

                if (high >= 0 && low >= 0)
                {
                    utf8Bytes.push_back(
                        static_cast<char>((high << 4) | low));

                    i += 2;
                    continue;
                }
            }

            if (ch <= 0x7F)
            {
                utf8Bytes.push_back(
                    static_cast<char>(ch));
            }
        }

        if (utf8Bytes.empty())
        {
            return L"";
        }

        const int wideLength =
            MultiByteToWideChar(
                CP_UTF8,
                0,
                utf8Bytes.c_str(),
                static_cast<int>(utf8Bytes.size()),
                nullptr,
                0);

        if (wideLength <= 0)
        {
            return L"";
        }

        std::wstring decoded(
            static_cast<size_t>(wideLength),
            L'\0');

        MultiByteToWideChar(
            CP_UTF8,
            0,
            utf8Bytes.c_str(),
            static_cast<int>(utf8Bytes.size()),
            decoded.data(),
            wideLength);

        return Trim(decoded);
    }

    std::wstring FindNewestFileSince(
        const std::wstring& folder,
        const FILETIME& downloadStart,
        const std::wstring& excludePath)
    {
        const std::wstring searchPattern =
            folder + L"\\*";

        WIN32_FIND_DATAW findData{};

        HANDLE findHandle =
            FindFirstFileW(
                searchPattern.c_str(),
                &findData);

        if (findHandle ==
            INVALID_HANDLE_VALUE)
        {
            return L"";
        }

        std::wstring bestName;
        FILETIME bestTime{};

        static const wchar_t* skipExtensions[] =
        {
            L".part",
            L".ytdl",
            L".webp",
            L".description",
            L".json",
            L".temp",
            L".tmp"
        };

        do
        {
            if (findData.dwFileAttributes &
                FILE_ATTRIBUTE_DIRECTORY)
            {
                continue;
            }

            const std::wstring name =
                findData.cFileName;

            // Skip dot-prefixed files outright. This app writes its
            // own internal bookkeeping (e.g. the session manifest)
            // as a hidden, dot-prefixed file specifically so it's
            // never mistaken for a downloaded artifact; it gets
            // rewritten throughout the download, so relying only on
            // "newest file" would otherwise pick it over the real
            // output.
            if (!name.empty() &&
                name.front() == L'.')
            {
                continue;
            }

            // Belt-and-suspenders: also exclude a specific known
            // path by exact match, in case a caller knows precisely
            // which file to rule out regardless of naming.
            if (!excludePath.empty() &&
                _wcsicmp(
                    (folder + L"\\" + name).c_str(),
                    excludePath.c_str()) == 0)
            {
                continue;
            }

            const size_t dotPos =
                name.find_last_of(L'.');

            if (dotPos != std::wstring::npos)
            {
                const std::wstring ext =
                    name.substr(dotPos);

                bool skip = false;

                for (const wchar_t* skipExt :
                    skipExtensions)
                {
                    if (_wcsicmp(
                        ext.c_str(),
                        skipExt) == 0)
                    {
                        skip = true;
                        break;
                    }
                }

                if (skip)
                {
                    continue;
                }
            }

            ULARGE_INTEGER fileTimeValue{};

            fileTimeValue.LowPart =
                findData.ftLastWriteTime.dwLowDateTime;

            fileTimeValue.HighPart =
                findData.ftLastWriteTime.dwHighDateTime;

            ULARGE_INTEGER startTimeValue{};

            startTimeValue.LowPart =
                downloadStart.dwLowDateTime;

            startTimeValue.HighPart =
                downloadStart.dwHighDateTime;

            if (fileTimeValue.QuadPart +
                20000000ULL <
                startTimeValue.QuadPart)
            {
                continue;
            }

            ULARGE_INTEGER bestTimeValue{};

            bestTimeValue.LowPart =
                bestTime.dwLowDateTime;

            bestTimeValue.HighPart =
                bestTime.dwHighDateTime;

            if (bestName.empty() ||
                fileTimeValue.QuadPart >
                bestTimeValue.QuadPart)
            {
                bestName = name;
                bestTime =
                    findData.ftLastWriteTime;
            }

        } while (FindNextFileW(
            findHandle,
            &findData));

        FindClose(findHandle);

        return bestName.empty()
            ? L""
            : folder + L"\\" + bestName;
    }

    bool FileExists(
        const std::wstring& path)
    {
        return GetFileAttributesW(
            path.c_str()) !=
            INVALID_FILE_ATTRIBUTES;
    }

    void CleanupCancelledDownload(
        const std::wstring& downloadsFolder,
        const FILETIME& downloadStart,
        const std::vector<std::wstring>& destinations)
    {
        /*
            First remove exact files reported by yt-dlp.
        */
        CleanupTrackedPartialFiles(
            destinations);

        /*
            Then perform the existing safety-net pass for
            partial files that may not have been reported before
            cancellation.
        */
        CleanupRecentPartialFiles(
            downloadsFolder,
            downloadStart);
    }

    void CleanupCompletedDownload(
        const std::vector<std::wstring>& destinations,
        const std::wstring& finalFilePath)
    {
        /*
            Example MP3 download:

                source.webm
                thumbnail.webp
                final.mp3

            Only final.mp3 should remain.
        */
        CleanupTrackedIntermediateFiles(
            destinations,
            finalFilePath);
    }
}