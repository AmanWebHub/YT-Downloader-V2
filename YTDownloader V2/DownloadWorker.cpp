#include "DownloadWorker.h"

#include "DownloadJob.h"
#include "DownloadManager.h"
#include "DownloadOutput.h"
#include "DownloadState.h"
#include "DownloadUtils.h"
#include "DownloadLogger.h"

#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <sstream>
#include <iomanip>

namespace
{
    constexpr wchar_t kManifestFileName[] =
        L".itdownloader_session.tmp";

    constexpr wchar_t kManifestHeader[] =
        L"IT_DOWNLOADER_SESSION_V1";

    constexpr wchar_t kManifestUrlPrefix[] =
        L"URL=";

    constexpr wchar_t kManifestFormatPrefix[] =
        L"FORMAT=";

    constexpr wchar_t kManifestPlaylistPrefix[] =
        L"PLAYLIST=";

    constexpr wchar_t kManifestFilePrefix[] =
        L"FILE=";

    constexpr wchar_t kManifestStartTimePrefix[] =
        L"START_TIME=";

    // Helper to convert FILETIME to a hex string for storage
    std::wstring FileTimeToHex(const FILETIME& ft)
    {
        ULARGE_INTEGER ul;
        ul.LowPart = ft.dwLowDateTime;
        ul.HighPart = ft.dwHighDateTime;
        std::wostringstream oss;
        oss << std::hex << std::setw(16) << std::setfill(L'0') << ul.QuadPart;
        return oss.str();
    }

    // Helper to parse hex string back to FILETIME
    bool HexToFileTime(const std::wstring& hex, FILETIME& ft)
    {
        if (hex.size() != 16) return false;
        ULARGE_INTEGER ul;
        std::wistringstream iss(hex);
        iss >> std::hex >> ul.QuadPart;
        if (iss.fail()) return false;
        ft.dwLowDateTime = ul.LowPart;
        ft.dwHighDateTime = ul.HighPart;
        return true;
    }

    std::wstring BuildManifestPath(
        const std::wstring& downloadsFolder)
    {
        return downloadsFolder +
               L"\\" +
               kManifestFileName;
    }

    bool FileExists(
        const std::wstring& path)
    {
        const DWORD attributes =
            GetFileAttributesW(path.c_str());

        return attributes !=
               INVALID_FILE_ATTRIBUTES &&
               (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    std::string WideToUtf8(
        const std::wstring& text)
    {
        if (text.empty())
        {
            return {};
        }

        const int size =
            WideCharToMultiByte(
                CP_UTF8,
                0,
                text.data(),
                static_cast<int>(text.size()),
                nullptr,
                0,
                nullptr,
                nullptr);

        if (size <= 0)
        {
            return {};
        }

        std::string result(
            static_cast<size_t>(size),
            '\0');

        WideCharToMultiByte(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            result.data(),
            size,
            nullptr,
            nullptr);

        return result;
    }

    std::wstring Utf8ToWide(
        const std::string& text)
    {
        if (text.empty())
        {
            return {};
        }

        const int size =
            MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                text.data(),
                static_cast<int>(text.size()),
                nullptr,
                0);

        if (size <= 0)
        {
            return {};
        }

        std::wstring result(
            static_cast<size_t>(size),
            L'\0');

        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            result.data(),
            size);

        return result;
    }

    bool WriteUtf8Line(
        HANDLE file,
        const std::wstring& line)
    {
        std::string utf8 =
            WideToUtf8(line + L"\r\n");

        if (utf8.empty())
        {
            return false;
        }

        DWORD written = 0;

        return WriteFile(
                   file,
                   utf8.data(),
                   static_cast<DWORD>(utf8.size()),
                   &written,
                   nullptr) &&
               written == utf8.size();
    }

    bool ReadAllUtf8(
        const std::wstring& path,
        std::wstring& text)
    {
        text.clear();

        HANDLE file =
            CreateFileW(
                path.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);

        if (file == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        LARGE_INTEGER size{};

        if (!GetFileSizeEx(file, &size) ||
            size.QuadPart <= 0 ||
            size.QuadPart > 64LL * 1024LL * 1024LL)
        {
            CloseHandle(file);
            return false;
        }

        std::string data(
            static_cast<size_t>(size.QuadPart),
            '\0');

        DWORD totalRead = 0;

        while (totalRead < data.size())
        {
            DWORD bytesRead = 0;

            if (!ReadFile(
                    file,
                    data.data() + totalRead,
                    static_cast<DWORD>(data.size() - totalRead),
                    &bytesRead,
                    nullptr))
            {
                CloseHandle(file);
                return false;
            }

            if (bytesRead == 0)
            {
                break;
            }

            totalRead += bytesRead;
        }

        CloseHandle(file);

        data.resize(totalRead);

        text = Utf8ToWide(data);
        return !text.empty();
    }

    std::vector<std::wstring> SplitLines(
        const std::wstring& text)
    {
        std::vector<std::wstring> lines;
        size_t start = 0;

        while (start < text.size())
        {
            const size_t end =
                text.find(L'\n', start);

            std::wstring line =
                (end == std::wstring::npos)
                    ? text.substr(start)
                    : text.substr(start, end - start);

            if (!line.empty() &&
                line.back() == L'\r')
            {
                line.pop_back();
            }

            lines.push_back(line);

            if (end == std::wstring::npos)
            {
                break;
            }

            start = end + 1;
        }

        return lines;
    }

    // Structure returned by GetSessionInfo
    struct SessionInfo
    {
        bool matches = false;
        FILETIME startTime{};
    };

    SessionInfo GetSessionInfo(
        const std::wstring& manifestPath,
        const std::wstring& url,
        bool isMp3,
        bool isPlaylist)
    {
        SessionInfo info;
        if (!FileExists(manifestPath))
            return info;

        std::wstring content;
        if (!ReadAllUtf8(manifestPath, content))
            return info;

        const std::vector<std::wstring> lines = SplitLines(content);

        bool headerOk = false;
        bool urlOk = false;
        bool formatOk = false;
        bool playlistOk = false;
        bool startTimeOk = false;

        for (const std::wstring& line : lines)
        {
            if (line == kManifestHeader)
            {
                headerOk = true;
            }
            else if (line.rfind(kManifestUrlPrefix, 0) == 0)
            {
                urlOk = (line.substr(wcslen(kManifestUrlPrefix)) == url);
            }
            else if (line.rfind(kManifestFormatPrefix, 0) == 0)
            {
                const std::wstring value = line.substr(wcslen(kManifestFormatPrefix));
                formatOk = (value == (isMp3 ? L"MP3" : L"MP4"));
            }
            else if (line.rfind(kManifestPlaylistPrefix, 0) == 0)
            {
                const std::wstring value = line.substr(wcslen(kManifestPlaylistPrefix));
                playlistOk = (value == (isPlaylist ? L"1" : L"0"));
            }
            else if (line.rfind(kManifestStartTimePrefix, 0) == 0)
            {
                const std::wstring hex = line.substr(wcslen(kManifestStartTimePrefix));
                if (HexToFileTime(hex, info.startTime))
                    startTimeOk = true;
            }
        }

        info.matches = headerOk && urlOk && formatOk && playlistOk && startTimeOk;
        return info;
    }

    bool CreateNewManifest(
        const std::wstring& manifestPath,
        const std::wstring& url,
        bool isMp3,
        bool isPlaylist,
        const FILETIME& startTime)
    {
        DeleteFileW(manifestPath.c_str());

        HANDLE file =
            CreateFileW(
                manifestPath.c_str(),
                GENERIC_WRITE,
                FILE_SHARE_READ,
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY,
                nullptr);

        if (file == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        const bool ok =
            WriteUtf8Line(file, kManifestHeader) &&
            WriteUtf8Line(file, kManifestUrlPrefix + url) &&
            WriteUtf8Line(file, kManifestFormatPrefix + std::wstring(isMp3 ? L"MP3" : L"MP4")) &&
            WriteUtf8Line(file, kManifestPlaylistPrefix + std::wstring(isPlaylist ? L"1" : L"0")) &&
            WriteUtf8Line(file, kManifestStartTimePrefix + FileTimeToHex(startTime));

        FlushFileBuffers(file);
        CloseHandle(file);

        if (!ok)
        {
            DeleteFileW(manifestPath.c_str());
        }

        return ok;
    }

    void LoadManifestFiles(
        const std::wstring& manifestPath,
        std::vector<std::wstring>& files)
    {
        files.clear();

        std::wstring content;

        if (!ReadAllUtf8(
                manifestPath,
                content))
        {
            return;
        }

        for (const std::wstring& line :
             SplitLines(content))
        {
            if (line.rfind(
                    kManifestFilePrefix,
                    0) != 0)
            {
                continue;
            }

            const std::wstring path =
                line.substr(
                    wcslen(kManifestFilePrefix));

            if (path.empty())
            {
                continue;
            }

            if (std::find(
                    files.begin(),
                    files.end(),
                    path) == files.end())
            {
                files.push_back(path);
            }
        }
    }

    void RecordManifestFile(
        const std::wstring& manifestPath,
        const std::wstring& path,
        std::vector<std::wstring>& files)
    {
        if (path.empty())
        {
            return;
        }

        if (std::find(
                files.begin(),
                files.end(),
                path) != files.end())
        {
            return;
        }

        HANDLE file =
            CreateFileW(
                manifestPath.c_str(),
                FILE_APPEND_DATA,
                FILE_SHARE_READ,
                nullptr,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY,
                nullptr);

        if (file == INVALID_HANDLE_VALUE)
        {
            return;
        }

        if (WriteUtf8Line(
                file,
                kManifestFilePrefix + path))
        {
            FlushFileBuffers(file);
            files.push_back(path);
        }

        CloseHandle(file);
    }

    void RecordKnownArtifacts(
        const std::wstring& manifestPath,
        const std::wstring& destination,
        std::vector<std::wstring>& files)
    {
        if (destination.empty())
        {
            return;
        }

        RecordManifestFile(
            manifestPath,
            destination,
            files);

        RecordManifestFile(
            manifestPath,
            destination + L".part",
            files);

        RecordManifestFile(
            manifestPath,
            destination + L".ytdl",
            files);

        RecordManifestFile(
            manifestPath,
            destination + L".temp",
            files);
    }

    bool IsCleanupExtension(
        const std::wstring& name)
    {
        const size_t dot = name.find_last_of(L'.');

        if (dot == std::wstring::npos)
        {
            return false;
        }

        const std::wstring extension =
            name.substr(dot);

        return extension == L".part" ||
               extension == L".ytdl" ||
               extension == L".temp" ||
               extension == L".webp" ||
               extension == L".webm" ||
               extension == L".mp3" ||
               extension == L".mp4" ||
               extension == L".m4a" ||
               extension == L".m4v";
    }

    std::wstring NormalizeFileNameForMatch(
        const std::wstring& value)
    {
        std::wstring normalized;
        normalized.reserve(value.size());

        bool previousWasSpace = false;

        for (wchar_t ch : value)
        {
            const bool isAlphaNumeric =
                (ch >= L'a' && ch <= L'z') ||
                (ch >= L'A' && ch <= L'Z') ||
                (ch >= L'0' && ch <= L'9');

            if (isAlphaNumeric)
            {
                normalized.push_back(
                    static_cast<wchar_t>(towlower(ch)));
                previousWasSpace = false;
                continue;
            }

            if (!previousWasSpace)
            {
                normalized.push_back(L' ');
                previousWasSpace = true;
            }
        }

        while (!normalized.empty() &&
               normalized.back() == L' ')
        {
            normalized.pop_back();
        }

        return normalized;
    }

    std::wstring GetFileStem(
        const std::wstring& name)
    {
        const size_t slash =
            name.find_last_of(L"\\/");

        const size_t start =
            (slash == std::wstring::npos)
                ? 0
                : slash + 1;

        const size_t dot =
            name.find_last_of(L'.');

        const size_t end =
            (dot == std::wstring::npos || dot < start)
                ? name.size()
                : dot;

        return name.substr(
            start,
            end - start);
    }

    bool HasTrackedStem(
        const std::wstring& name,
        const std::vector<std::wstring>& trackedDestinations)
    {
        const std::wstring normalizedName =
            NormalizeFileNameForMatch(
                GetFileStem(name));

        if (normalizedName.empty())
        {
            return false;
        }

        for (const std::wstring& destination :
             trackedDestinations)
        {
            if (NormalizeFileNameForMatch(
                    GetFileStem(destination)) ==
                normalizedName)
            {
                return true;
            }
        }

        return false;
    }

    void DeleteManifestFiles(
        const std::vector<std::wstring>& files)
    {
        for (const std::wstring& path : files)
        {
            if (path.empty())
            {
                continue;
            }

            SetLastError(ERROR_SUCCESS);

            const BOOL deleted =
                DeleteFileW(path.c_str());

            if (deleted)
            {
                DownloadLogger::Write(
                    L"DownloadWorker",
                    L"Deleted manifest file: " + path);
                continue;
            }

            const DWORD errorCode = GetLastError();

            if (errorCode == ERROR_FILE_NOT_FOUND ||
                errorCode == ERROR_PATH_NOT_FOUND)
            {
                DownloadLogger::Write(
                    L"DownloadWorker",
                    L"Manifest file already absent: " + path);
            }
            else
            {
                DownloadLogger::Write(
                    L"DownloadWorker",
                    L"Failed to delete manifest file (error " +
                    std::to_wstring(errorCode) +
                    L"): " +
                    path);
            }
        }
    }

    void ScanAndDeleteRecentPartialArtifacts(
        const std::wstring& downloadsFolder,
        const FILETIME& sessionStartTime,
        const std::vector<std::wstring>& trackedDestinations)
    {
        const std::wstring searchPattern =
            downloadsFolder + L"\\*";

        WIN32_FIND_DATAW findData{};

        HANDLE findHandle =
            FindFirstFileW(
                searchPattern.c_str(),
                &findData);

        if (findHandle == INVALID_HANDLE_VALUE)
        {
            DownloadLogger::Write(
                L"DownloadWorker",
                L"ScanAndDeleteRecentPartialArtifacts: could not enumerate folder.");

            return;
        }

        ULARGE_INTEGER startTimeValue{};
        startTimeValue.LowPart = sessionStartTime.dwLowDateTime;
        startTimeValue.HighPart = sessionStartTime.dwHighDateTime;

        int deletedCount = 0;

        do
        {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                continue;
            }

            const std::wstring name = findData.cFileName;

            if (!IsCleanupExtension(name))
            {
                continue;
            }

            ULARGE_INTEGER fileTimeValue{};
            fileTimeValue.LowPart = findData.ftLastWriteTime.dwLowDateTime;
            fileTimeValue.HighPart = findData.ftLastWriteTime.dwHighDateTime;

            // 2 seconds = 20,000,000 in 100-nanosecond FILETIME units.
            if (fileTimeValue.QuadPart + 20000000ULL <
                startTimeValue.QuadPart)
            {
                continue; // predates this download - leave it alone
            }

            const bool isAlwaysSafeArtifact =
                name.size() >= 5 &&
                (name.rfind(L".part") == name.size() - 5 ||
                 name.rfind(L".ytdl") == name.size() - 5 ||
                 name.rfind(L".temp") == name.size() - 5 ||
                 name.rfind(L".webp") == name.size() - 5);

            if (!isAlwaysSafeArtifact &&
                !HasTrackedStem(name, trackedDestinations))
            {
                continue;
            }

            const std::wstring fullPath =
                downloadsFolder + L"\\" + name;

            SetLastError(ERROR_SUCCESS);

            const BOOL deleted =
                DeleteFileW(fullPath.c_str());

            DownloadLogger::Write(
                L"DownloadWorker",
                (deleted
                    ? L"Scan cleanup deleted: "
                    : L"Scan cleanup FAILED to delete (error " +
                      std::to_wstring(GetLastError()) +
                      L"): ") +
                fullPath);

            if (deleted)
            {
                ++deletedCount;
            }

        } while (FindNextFileW(findHandle, &findData));

        FindClose(findHandle);

        DownloadLogger::Write(
            L"DownloadWorker",
            L"ScanAndDeleteRecentPartialArtifacts finished. Files deleted: " +
            std::to_wstring(deletedCount));
    }

    void DeleteCompletedMp3WebmFile(
        const std::wstring& path)
    {
        if (path.empty())
        {
            return;
        }

        SetLastError(ERROR_SUCCESS);

        if (DeleteFileW(path.c_str()))
        {
            DownloadLogger::Write(
                L"DownloadWorker",
                L"Deleted completed MP3 intermediate file: " +
                path);
            return;
        }

        const DWORD errorCode = GetLastError();

        if (errorCode == ERROR_FILE_NOT_FOUND ||
            errorCode == ERROR_PATH_NOT_FOUND)
        {
            DownloadLogger::Write(
                L"DownloadWorker",
                L"Completed MP3 intermediate file already absent: " +
                path);
        }
        else
        {
            DownloadLogger::Write(
                L"DownloadWorker",
                L"Failed to delete completed MP3 intermediate file (error " +
                std::to_wstring(errorCode) +
                L"): " +
                path);
        }
    }

    void CleanupCompletedMp3IntermediateFiles(
        const std::vector<std::wstring>& trackedFiles,
        const std::vector<std::wstring>& trackedDestinations,
        const std::wstring& downloadsFolder,
        const FILETIME& sessionStartTime)
    {
        for (const std::wstring& path : trackedFiles)
        {
            if (path.empty())
            {
                continue;
            }

            const size_t dot =
                path.find_last_of(L'.');

            if (dot == std::wstring::npos)
            {
                continue;
            }

            const std::wstring extension =
                path.substr(dot);

            const bool isWebm =
                _wcsicmp(extension.c_str(), L".webm") == 0;

            const bool isWebmPart =
                path.size() >= 9 &&
                _wcsicmp(
                    path.c_str() + path.size() - 9,
                    L".webm.part") == 0;

            if (isWebm || isWebmPart)
            {
                DeleteCompletedMp3WebmFile(path);
            }
        }

        if (downloadsFolder.empty() || trackedDestinations.empty())
        {
            return;
        }

        const std::wstring searchPattern =
            downloadsFolder + L"\\*";

        WIN32_FIND_DATAW findData{};

        HANDLE findHandle =
            FindFirstFileW(
                searchPattern.c_str(),
                &findData);

        if (findHandle == INVALID_HANDLE_VALUE)
        {
            DownloadLogger::Write(
                L"DownloadWorker",
                L"CleanupCompletedMp3IntermediateFiles: could not enumerate folder.");
            return;
        }

        ULARGE_INTEGER startTimeValue{};
        startTimeValue.LowPart = sessionStartTime.dwLowDateTime;
        startTimeValue.HighPart = sessionStartTime.dwHighDateTime;

        int deletedCount = 0;

        do
        {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                continue;
            }

            const std::wstring name =
                findData.cFileName;

            const bool isWebm =
                name.size() >= 5 &&
                _wcsicmp(
                    name.c_str() + name.size() - 5,
                    L".webm") == 0;

            const bool isWebmPart =
                name.size() >= 9 &&
                _wcsicmp(
                    name.c_str() + name.size() - 9,
                    L".webm.part") == 0;

            if (!isWebm && !isWebmPart)
            {
                continue;
            }

            ULARGE_INTEGER fileTimeValue{};
            fileTimeValue.LowPart =
                findData.ftLastWriteTime.dwLowDateTime;
            fileTimeValue.HighPart =
                findData.ftLastWriteTime.dwHighDateTime;

            if (fileTimeValue.QuadPart + 20000000ULL <
                startTimeValue.QuadPart)
            {
                continue;
            }

            if (!HasTrackedStem(
                    name,
                    trackedDestinations))
            {
                continue;
            }

            const std::wstring fullPath =
                downloadsFolder + L"\\" + name;

            SetLastError(ERROR_SUCCESS);

            if (DeleteFileW(fullPath.c_str()))
            {
                DownloadLogger::Write(
                    L"DownloadWorker",
                    L"Completed MP3 scan deleted WebM intermediate: " +
                    fullPath);
                ++deletedCount;
            }
            else
            {
                const DWORD errorCode = GetLastError();

                if (errorCode != ERROR_FILE_NOT_FOUND &&
                    errorCode != ERROR_PATH_NOT_FOUND)
                {
                    DownloadLogger::Write(
                        L"DownloadWorker",
                        L"Completed MP3 scan FAILED to delete WebM (error " +
                        std::to_wstring(errorCode) +
                        L"): " +
                        fullPath);
                }
            }

        } while (FindNextFileW(findHandle, &findData));

        FindClose(findHandle);

        DownloadLogger::Write(
            L"DownloadWorker",
            L"Completed MP3 WebM scan finished. Files deleted: " +
            std::to_wstring(deletedCount));
    }

    // Third pass: delete any .webm or .webm.part with the same stem as the final MP3,
    // using the original session start time.
    void CleanupLeftoverWebmFiles(
        const std::wstring& downloadsFolder,
        const std::wstring& finalFilePath,
        const FILETIME& sessionStartTime)
    {
        if (finalFilePath.empty() || downloadsFolder.empty())
            return;

        std::wstring finalStem = GetFileStem(finalFilePath);
        if (finalStem.empty())
            return;

        std::wstring searchPattern = downloadsFolder + L"\\*";
        WIN32_FIND_DATAW findData{};
        HANDLE findHandle = FindFirstFileW(searchPattern.c_str(), &findData);
        if (findHandle == INVALID_HANDLE_VALUE)
        {
            DownloadLogger::Write(
                L"DownloadWorker",
                L"CleanupLeftoverWebmFiles: could not enumerate folder.");
            return;
        }

        ULARGE_INTEGER startTimeValue{};
        startTimeValue.LowPart = sessionStartTime.dwLowDateTime;
        startTimeValue.HighPart = sessionStartTime.dwHighDateTime;

        int deletedCount = 0;

        do
        {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;

            std::wstring name = findData.cFileName;

            bool isWebm = false;
            if (name.size() >= 5 &&
                _wcsicmp(name.c_str() + name.size() - 5, L".webm") == 0)
                isWebm = true;
            else if (name.size() >= 9 &&
                     _wcsicmp(name.c_str() + name.size() - 9, L".webm.part") == 0)
                isWebm = true;

            if (!isWebm)
                continue;

            ULARGE_INTEGER fileTimeValue{};
            fileTimeValue.LowPart = findData.ftLastWriteTime.dwLowDateTime;
            fileTimeValue.HighPart = findData.ftLastWriteTime.dwHighDateTime;
            if (fileTimeValue.QuadPart + 20000000ULL < startTimeValue.QuadPart)
                continue;

            std::wstring candidateStem = GetFileStem(name);
            if (NormalizeFileNameForMatch(candidateStem) != NormalizeFileNameForMatch(finalStem))
                continue;

            std::wstring fullPath = downloadsFolder + L"\\" + name;
            bool deleted = false;
            for (int attempt = 0; attempt < 3; ++attempt)
            {
                if (DeleteFileW(fullPath.c_str()))
                {
                    deleted = true;
                    break;
                }
                Sleep(50);
            }

            DownloadLogger::Write(
                L"DownloadWorker",
                deleted
                    ? L"Leftover WebM cleanup deleted: " + fullPath
                    : L"Leftover WebM cleanup FAILED (error " +
                      std::to_wstring(GetLastError()) + L"): " + fullPath);

            if (deleted)
                ++deletedCount;

        } while (FindNextFileW(findHandle, &findData));

        FindClose(findHandle);

        DownloadLogger::Write(
            L"DownloadWorker",
            L"CleanupLeftoverWebmFiles finished. Deleted: " +
            std::to_wstring(deletedCount));
    }

    void CleanupCancelledSession(
        const std::wstring& manifestPath,
        const std::vector<std::wstring>& trackedFiles,
        const std::vector<std::wstring>& trackedDestinations,
        const std::wstring& downloadsFolder,
        const FILETIME& sessionStartTime)
    {
        DownloadLogger::Write(
            L"DownloadWorker",
            L"CleanupCancelledSession() starting. Tracked files: " +
            std::to_wstring(trackedFiles.size()));

        std::vector<std::wstring> files =
            trackedFiles;

        std::vector<std::wstring> persistedFiles;

        LoadManifestFiles(
            manifestPath,
            persistedFiles);

        DownloadLogger::Write(
            L"DownloadWorker",
            L"Manifest reload found " +
            std::to_wstring(persistedFiles.size()) +
            L" persisted file(s).");

        for (const std::wstring& path : persistedFiles)
        {
            if (std::find(
                    files.begin(),
                    files.end(),
                    path) == files.end())
            {
                files.push_back(path);
            }
        }

        DeleteManifestFiles(files);

        Sleep(100);
        DeleteManifestFiles(files);

        DeleteFileW(
            manifestPath.c_str());

        ScanAndDeleteRecentPartialArtifacts(
            downloadsFolder,
            sessionStartTime,
            trackedDestinations);

        DownloadLogger::Write(
            L"DownloadWorker",
            L"CleanupCancelledSession() finished.");
    }

    bool ReadOutputLine(
        HANDLE readPipe,
        std::string& pending,
        std::wstring& line)
    {
        while (true)
        {
            const size_t newlinePos =
                pending.find('\n');

            if (newlinePos != std::string::npos)
            {
                std::string rawLine =
                    pending.substr(0, newlinePos);

                pending.erase(
                    0,
                    newlinePos + 1);

                if (!rawLine.empty() &&
                    rawLine.back() == '\r')
                {
                    rawLine.pop_back();
                }

                int wideLength =
                    MultiByteToWideChar(
                        CP_UTF8,
                        MB_ERR_INVALID_CHARS,
                        rawLine.data(),
                        static_cast<int>(rawLine.size()),
                        nullptr,
                        0);

                UINT codePage = CP_UTF8;

                if (wideLength <= 0)
                {
                    codePage = CP_ACP;

                    wideLength =
                        MultiByteToWideChar(
                            codePage,
                            0,
                            rawLine.data(),
                            static_cast<int>(rawLine.size()),
                            nullptr,
                            0);
                }

                if (wideLength <= 0)
                {
                    continue;
                }

                line.assign(
                    wideLength,
                    L'\0');

                MultiByteToWideChar(
                    codePage,
                    (codePage == CP_UTF8)
                        ? MB_ERR_INVALID_CHARS
                        : 0,
                    rawLine.data(),
                    static_cast<int>(rawLine.size()),
                    line.data(),
                    wideLength);

                line =
                    DownloadUtils::Trim(line);

                return true;
            }

            char buffer[4096]{};
            DWORD bytesRead = 0;

            const BOOL readOk =
                ReadFile(
                    readPipe,
                    buffer,
                    static_cast<DWORD>(sizeof(buffer)),
                    &bytesRead,
                    nullptr);

            if (!readOk || bytesRead == 0)
            {
                return false;
            }

            pending.append(
                buffer,
                bytesRead);
        }
    }

    std::wstring ExtractDestinationFromLine(
        const std::wstring& line)
    {
        const std::wstring markers[] =
        {
            L"[download] Destination:",
            L"[ExtractAudio] Destination:"
        };

        for (const std::wstring& marker : markers)
        {
            const size_t markerPos =
                line.find(marker);

            if (markerPos == std::wstring::npos)
            {
                continue;
            }

            return DownloadUtils::Trim(
                line.substr(markerPos + marker.size()));
        }

        const std::wstring printMarker =
            L"__ITD_FILE__:";

        const size_t printMarkerPos =
            line.find(printMarker);

        if (printMarkerPos != std::wstring::npos)
        {
            return DownloadUtils::Trim(
                line.substr(
                    printMarkerPos +
                    printMarker.size()));
        }

        if (line.find(L"[Merger] Merging formats into") !=
            std::wstring::npos)
        {
            const size_t firstQuote =
                line.find(L'"');

            const size_t lastQuote =
                line.find_last_of(L'"');

            if (firstQuote != std::wstring::npos &&
                lastQuote != std::wstring::npos &&
                lastQuote > firstQuote)
            {
                return line.substr(
                    firstQuote + 1,
                    lastQuote - firstQuote - 1);
            }
        }

        return L"";
    }

    std::wstring BuildCommandLine(
        const std::wstring& ytDlpPath,
        const std::wstring& url,
        bool isMp3,
        bool isPlaylist,
        const std::wstring& downloadsFolder)
    {
        std::wstring commandLine =
            L"\"" + ytDlpPath + L"\" "
            L"--newline "
            L"--print before_dl:__ITD_FILE__:%(filepath)s "
            L"--no-quiet ";

        if (isMp3)
        {
            commandLine += L"--continue ";
        }
        else
        {
            commandLine +=
                L"--no-continue "
                L"--force-overwrites ";
        }

        if (isPlaylist)
        {
            commandLine +=
                L"--yes-playlist "
                L"--ignore-errors ";
        }
        else
        {
            commandLine += L"--no-playlist ";
        }

        if (isMp3)
        {
            commandLine +=
                L"--extract-audio "
                L"--audio-format mp3 "
                L"--audio-quality 0 "
                L"--embed-thumbnail "
                L"--add-metadata ";
        }
        else
        {
            commandLine +=
                L"-f \"bv*[ext=mp4]+ba[ext=m4a]/b[ext=mp4]\" "
                L"--merge-output-format mp4 ";
        }

        commandLine +=
            L"-o \"" +
            downloadsFolder +
            (isPlaylist
                ? L"\\%(playlist_index)s - %(title)s.%(ext)s\" "
                : L"\\%(title)s.%(ext)s\" ") +
            L"\"" +
            url +
            L"\"";

        return commandLine;
    }

    void ResetState()
    {
        DownloadState::stopRequested = false;
        DownloadState::pauseRequested = false;
        DownloadState::downloadRunning = false;
    }
}

namespace DownloadWorker
{
    void Run(
        HWND ownerWindow,
        std::wstring url,
        bool isMp3,
        bool isPlaylist,
        std::wstring ytDlpPath,
        std::wstring downloadsFolder)
    {
        const std::wstring manifestPath =
            BuildManifestPath(downloadsFolder);

        std::vector<std::wstring> manifestFiles;

        FILETIME originalStartTime{};
        bool hasOriginalStartTime = false;

        // Check for existing manifest
        SessionInfo sessionInfo = GetSessionInfo(manifestPath, url, isMp3, isPlaylist);
        if (sessionInfo.matches)
        {
            LoadManifestFiles(manifestPath, manifestFiles);
            originalStartTime = sessionInfo.startTime;
            hasOriginalStartTime = true;
            DownloadLogger::Write(
                L"DownloadWorker",
                L"Resuming existing session with original start time.");
        }
        else
        {
            // New session: capture start time now
            GetSystemTimeAsFileTime(&originalStartTime);
            hasOriginalStartTime = true;
            if (!CreateNewManifest(manifestPath, url, isMp3, isPlaylist, originalStartTime))
            {
                DownloadOutput::PostStatus(
                    ownerWindow,
                    L"Failed to create the download session file.");

                DownloadOutput::PostFinished(
                    ownerWindow,
                    PRE_LAUNCH_FAILURE_CODE,
                    isMp3,
                    false,
                    false,
                    downloadsFolder,
                    L"");

                ResetState();
                return;
            }
        }

        SECURITY_ATTRIBUTES securityAttributes{};
        securityAttributes.nLength =
            sizeof(securityAttributes);
        securityAttributes.bInheritHandle = TRUE;

        HANDLE readPipe = nullptr;
        HANDLE writePipe = nullptr;

        if (!CreatePipe(
                &readPipe,
                &writePipe,
                &securityAttributes,
                0))
        {
            DownloadOutput::PostStatus(
                ownerWindow,
                L"Failed to create the output pipe.");

            DownloadOutput::PostFinished(
                ownerWindow,
                PRE_LAUNCH_FAILURE_CODE,
                isMp3,
                false,
                false,
                downloadsFolder,
                L"");

            ResetState();
            return;
        }

        SetHandleInformation(
            readPipe,
            HANDLE_FLAG_INHERIT,
            0);

        const std::wstring commandLine =
            BuildCommandLine(
                ytDlpPath,
                url,
                isMp3,
                isPlaylist,
                downloadsFolder);

        std::vector<wchar_t> commandBuffer(
            commandLine.begin(),
            commandLine.end());

        commandBuffer.push_back(L'\0');

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        startupInfo.dwFlags = STARTF_USESTDHANDLES;
        startupInfo.hStdInput = nullptr;
        startupInfo.hStdOutput = writePipe;
        startupInfo.hStdError = writePipe;

        PROCESS_INFORMATION processInfo{};
        FILETIME downloadStartTime{};
        GetSystemTimeAsFileTime(&downloadStartTime);

        const BOOL created =
            CreateProcessW(
                nullptr,
                commandBuffer.data(),
                nullptr,
                nullptr,
                TRUE,
                CREATE_NO_WINDOW,
                nullptr,
                nullptr,
                &startupInfo,
                &processInfo);

        CloseHandle(writePipe);
        writePipe = nullptr;

        if (!created)
        {
            const DWORD errorCode =
                GetLastError();

            CloseHandle(readPipe);

            DownloadOutput::PostStatus(
                ownerWindow,
                L"Failed to start yt-dlp.exe. Windows error code: " +
                std::to_wstring(errorCode) +
                L".");

            DownloadOutput::PostFinished(
                ownerWindow,
                PRE_LAUNCH_FAILURE_CODE,
                isMp3,
                false,
                false,
                downloadsFolder,
                L"");

            ResetState();
            return;
        }

        CloseHandle(processInfo.hThread);

        HANDLE jobHandle =
            DownloadJob::CreateDownloadJob();

        if (jobHandle == nullptr)
        {
            TerminateProcess(
                processInfo.hProcess,
                1);

            WaitForSingleObject(
                processInfo.hProcess,
                INFINITE);

            CloseHandle(processInfo.hProcess);
            CloseHandle(readPipe);

            DownloadOutput::PostStatus(
                ownerWindow,
                L"Failed to create the download process group.");

            DownloadOutput::PostFinished(
                ownerWindow,
                PRE_LAUNCH_FAILURE_CODE,
                isMp3,
                false,
                false,
                downloadsFolder,
                L"");

            ResetState();
            return;
        }

        if (!AssignProcessToJobObject(
                jobHandle,
                processInfo.hProcess))
        {
            const DWORD errorCode =
                GetLastError();

            CloseHandle(jobHandle);

            TerminateProcess(
                processInfo.hProcess,
                1);

            WaitForSingleObject(
                processInfo.hProcess,
                INFINITE);

            CloseHandle(processInfo.hProcess);
            CloseHandle(readPipe);

            DownloadOutput::PostStatus(
                ownerWindow,
                L"Failed to attach yt-dlp to its process group. "
                L"Windows error code: " +
                std::to_wstring(errorCode) +
                L".");

            DownloadOutput::PostFinished(
                ownerWindow,
                PRE_LAUNCH_FAILURE_CODE,
                isMp3,
                false,
                false,
                downloadsFolder,
                L"");

            ResetState();
            return;
        }

        DownloadState::processHandle.store(
            processInfo.hProcess,
            std::memory_order_release);

        DownloadState::jobHandle.store(
            jobHandle,
            std::memory_order_release);

        DownloadOutput::PostStatus(
            ownerWindow,
            L"Starting download...");

        std::string pending;
        std::wstring finalFileName;

        std::vector<std::wstring>
            trackedDestinations;

        bool cancellationSent = false;

        while (true)
        {
            if ((DownloadState::stopRequested ||
                 DownloadState::pauseRequested) &&
                !cancellationSent)
            {
                HANDLE activeJob =
                    DownloadState::jobHandle.load(
                        std::memory_order_acquire);

                if (activeJob != nullptr)
                {
                    TerminateJobObject(
                        activeJob,
                        1);
                }
                else
                {
                    TerminateProcess(
                        processInfo.hProcess,
                        1);
                }

                cancellationSent = true;
            }

            std::wstring line;

            if (!ReadOutputLine(
                    readPipe,
                    pending,
                    line))
            {
                break;
            }

            const std::wstring destination =
                ExtractDestinationFromLine(line);

            if (!destination.empty())
            {
                if (std::find(
                        trackedDestinations.begin(),
                        trackedDestinations.end(),
                        destination) ==
                    trackedDestinations.end())
                {
                    trackedDestinations.push_back(
                        destination);
                }

                RecordKnownArtifacts(
                    manifestPath,
                    destination,
                    manifestFiles);
            }

            DownloadOutput::ProcessLine(
                ownerWindow,
                line,
                trackedDestinations,
                finalFileName);
        }

        CloseHandle(readPipe);

        WaitForSingleObject(
            processInfo.hProcess,
            INFINITE);

        DWORD exitCode = 1;

        GetExitCodeProcess(
            processInfo.hProcess,
            &exitCode);

        const bool wasPaused =
            DownloadState::pauseRequested.load();

        const bool wasCancelled =
            DownloadState::stopRequested.load();

        DownloadState::processHandle.store(
            nullptr,
            std::memory_order_release);

        DownloadState::jobHandle.store(
            nullptr,
            std::memory_order_release);

        CloseHandle(processInfo.hProcess);

        CloseHandle(jobHandle);

        std::wstring resolvedFilePath;

        if (wasPaused)
        {
            DownloadOutput::PostStatus(
                ownerWindow,
                L"Paused.");
        }
        else if (wasCancelled)
        {
            CleanupCancelledSession(
                manifestPath,
                manifestFiles,
                trackedDestinations,
                downloadsFolder,
                originalStartTime);

            DownloadOutput::PostStatus(
                ownerWindow,
                L"Download cancelled.");
        }
        else
        {
            // Determine if we have a valid final file, even if exitCode != 0 (for MP3)
            bool hasValidFile = false;

            // First, try using finalFileName from parser
            if (!finalFileName.empty())
            {
                const std::wstring candidate =
                    downloadsFolder + L"\\" + finalFileName;
                if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES)
                {
                    resolvedFilePath = candidate;
                    hasValidFile = true;
                }
            }

            // If not found, scan for a .mp3 file created since the original start time
            if (!hasValidFile && isMp3)
            {
                // Use FindNewestFileSince but filter for .mp3 extension
                const std::wstring searchPattern = downloadsFolder + L"\\*";
                WIN32_FIND_DATAW findData{};
                HANDLE findHandle = FindFirstFileW(searchPattern.c_str(), &findData);
                if (findHandle != INVALID_HANDLE_VALUE)
                {
                    ULARGE_INTEGER startTimeValue{};
                    startTimeValue.LowPart = originalStartTime.dwLowDateTime;
                    startTimeValue.HighPart = originalStartTime.dwHighDateTime;

                    std::wstring bestPath;
                    ULARGE_INTEGER bestTime{};
                    bestTime.QuadPart = 0;

                    do
                    {
                        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                            continue;

                        const std::wstring name = findData.cFileName;
                        const size_t dot = name.find_last_of(L'.');
                        if (dot == std::wstring::npos)
                            continue;
                        if (_wcsicmp(name.substr(dot).c_str(), L".mp3") != 0)
                            continue;

                        ULARGE_INTEGER fileTime{};
                        fileTime.LowPart = findData.ftLastWriteTime.dwLowDateTime;
                        fileTime.HighPart = findData.ftLastWriteTime.dwHighDateTime;

                        // Must be created after the start (with 2s tolerance)
                        if (fileTime.QuadPart + 20000000ULL < startTimeValue.QuadPart)
                            continue;

                        if (fileTime.QuadPart > bestTime.QuadPart)
                        {
                            bestTime = fileTime;
                            bestPath = downloadsFolder + L"\\" + name;
                        }
                    } while (FindNextFileW(findHandle, &findData));
                    FindClose(findHandle);

                    if (!bestPath.empty() && DownloadUtils::FileExists(bestPath))
                    {
                        resolvedFilePath = bestPath;
                        hasValidFile = true;
                        // Also update finalFileName for cleanup
                        if (finalFileName.empty())
                        {
                            const size_t slash = bestPath.find_last_of(L"\\/");
                            finalFileName = (slash != std::wstring::npos) ? bestPath.substr(slash + 1) : bestPath;
                        }
                    }
                }
            }

            // Now decide what to do
            if (exitCode == 0 || (isMp3 && hasValidFile))
            {
                // Success path
                PostMessageW(
                    ownerWindow,
                    WM_APP_DOWNLOAD_PROGRESS,
                    100,
                    0);

                DownloadOutput::PostStatus(
                    ownerWindow,
                    L"Download complete.");

                // If we didn't have resolvedFilePath from earlier, we have it now
                // (but we already set it above)

                DeleteFileW(manifestPath.c_str());

                if (isMp3)
                {
                    // Pass 1: remove WebM files tracked in the manifest
                    CleanupCompletedMp3IntermediateFiles(
                        manifestFiles,
                        trackedDestinations,
                        downloadsFolder,
                        originalStartTime);

                    Sleep(100);

                    // Pass 2: scan with tracked destinations
                    CleanupCompletedMp3IntermediateFiles(
                        {},
                        trackedDestinations,
                        downloadsFolder,
                        originalStartTime);

                    // Pass 3: leftover .webm matching final stem
                    CleanupLeftoverWebmFiles(
                        downloadsFolder,
                        resolvedFilePath,
                        originalStartTime);
                }

                DeleteFileW(manifestPath.c_str());
            }
            else
            {
                // Real failure
                DownloadOutput::PostStatus(
                    ownerWindow,
                    L"Download failed.");

                // Clean up partial files
                CleanupCancelledSession(
                    manifestPath,
                    manifestFiles,
                    trackedDestinations,
                    downloadsFolder,
                    originalStartTime);

                // We'll still send a failure, but resolvedFilePath empty
                resolvedFilePath.clear();
            }
        }

        DownloadOutput::PostFinished(
            ownerWindow,
            exitCode,
            isMp3,
            wasPaused,
            wasCancelled,
            downloadsFolder,
            resolvedFilePath);

        ResetState();
    }
}