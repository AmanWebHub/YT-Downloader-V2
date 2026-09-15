#include "DownloadResumeManifest.h"
#include "DownloadLogger.h"

#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <sstream>
#include <iomanip>

// This whole file is the resume/session-manifest subsystem that used
// to live at the top of DownloadWorker.cpp (see that file's history
// if you need it). Moved out verbatim - no logic changes - so that
// DownloadWorker.cpp can focus on just running yt-dlp and reading its
// output, and this subsystem has a clearly-scoped home of its own.
namespace DownloadResumeManifest
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
}
