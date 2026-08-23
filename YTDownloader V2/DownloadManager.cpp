#include "DownloadManager.h"

#include <windows.h>

#include <atomic>
#include <cstdlib>
#include <cwctype>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "shell32.lib")

namespace
{
    constexpr ULONGLONG FILE_TIME_TOLERANCE = 20000000ULL;
    constexpr DWORD PROCESS_TERMINATION_CODE = 1;

    std::atomic<bool> g_downloadRunning{ false };
    std::atomic<bool> g_stopRequested{ false };
    std::atomic<bool> g_pauseRequested{ false };

    // These handles are published so the UI thread can request
    // cancellation or pause without owning the handles.
    std::atomic<HANDLE> g_processHandle{ nullptr };
    std::atomic<HANDLE> g_jobHandle{ nullptr };


    // ============================================================
    // Path / folder helpers
    // ============================================================

    std::wstring GetExeDirectory()
    {
        wchar_t exePath[MAX_PATH]{};

        const DWORD length =
            GetModuleFileNameW(
                nullptr,
                exePath,
                MAX_PATH);

        if (length == 0)
        {
            return L".";
        }

        const std::wstring path(
            exePath,
            length);

        const size_t lastSlash =
            path.find_last_of(L"\\/");

        return
            (lastSlash != std::wstring::npos)
            ? path.substr(0, lastSlash)
            : L".";
    }

    std::wstring GetYtDlpPath()
    {
        return GetExeDirectory() +
            L"\\bin\\yt-dlp.exe";
    }

    std::wstring GetDownloadsFolder(bool isMp3)
    {
        wchar_t* userProfile = nullptr;
        size_t length = 0;

        std::wstring folder;

        if (_wdupenv_s(
            &userProfile,
            &length,
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

        return folder +
            (isMp3
                ? L"\\Downloads\\Music"
                : L"\\Downloads\\Video");
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


    // ============================================================
    // String / output helpers
    // ============================================================

    std::wstring Trim(
        const std::wstring& text)
    {
        size_t start = 0;

        while (
            start < text.size() &&
            iswspace(text[start]))
        {
            ++start;
        }

        size_t end = text.size();

        while (
            end > start &&
            iswspace(text[end - 1]))
        {
            --end;
        }

        return text.substr(
            start,
            end - start);
    }

    std::wstring ConvertOutputLine(
        const std::string& rawLine)
    {
        if (rawLine.empty())
        {
            return L"";
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
            return L"";
        }

        std::wstring result(
            wideLength,
            L'\0');

        MultiByteToWideChar(
            codePage,
            codePage == CP_UTF8
            ? MB_ERR_INVALID_CHARS
            : 0,
            rawLine.data(),
            static_cast<int>(rawLine.size()),
            result.data(),
            wideLength);

        return Trim(result);
    }

    bool TryParseProgress(
        const std::wstring& line,
        int& progress)
    {
        const size_t percentPos =
            line.find(L'%');

        if (percentPos == std::wstring::npos)
        {
            return false;
        }

        size_t end = percentPos;
        size_t start = end;

        while (start > 0)
        {
            const wchar_t character =
                line[start - 1];

            if (
                (character >= L'0' &&
                    character <= L'9') ||
                character == L'.')
            {
                --start;
            }
            else
            {
                break;
            }
        }

        if (start == end)
        {
            return false;
        }

        try
        {
            const double value =
                std::stod(
                    line.substr(
                        start,
                        end - start));

            if (
                value < 0.0 ||
                value > 100.0)
            {
                return false;
            }

            progress =
                static_cast<int>(value);

            return true;
        }
        catch (...)
        {
            return false;
        }
    }


    // ============================================================
    // Window message helpers
    // ============================================================

    void PostStatus(
        HWND ownerWindow,
        const std::wstring& text)
    {
        auto* message =
            new std::wstring(text);

        if (!PostMessageW(
            ownerWindow,
            WM_APP_DOWNLOAD_STATUS,
            0,
            reinterpret_cast<LPARAM>(message)))
        {
            delete message;
        }
    }

    void PostFinished(
        HWND ownerWindow,
        DWORD exitCode,
        bool isMp3,
        bool wasPaused,
        bool wasCancelled,
        const std::wstring& downloadsFolder,
        const std::wstring& filePath)
    {
        auto* info =
            new DownloadFinishedInfo;

        info->exitCode = exitCode;
        info->isMp3 = isMp3;
        info->wasPaused = wasPaused;
        info->wasCancelled = wasCancelled;
        info->downloadsFolder = downloadsFolder;
        info->filePath = filePath;

        if (!PostMessageW(
            ownerWindow,
            WM_APP_DOWNLOAD_FINISHED,
            0,
            reinterpret_cast<LPARAM>(info)))
        {
            delete info;
        }
    }


    // ============================================================
    // File helpers
    // ============================================================

    bool FileExists(
        const std::wstring& path)
    {
        return
            GetFileAttributesW(
                path.c_str()) !=
            INVALID_FILE_ATTRIBUTES;
    }

    void DeletePartialFile(
        const std::wstring& path)
    {
        if (path.empty())
        {
            return;
        }

        if (!FileExists(path))
        {
            return;
        }

        DeleteFileW(
            path.c_str());
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

    void CleanupTrackedPartialFiles(
        const std::vector<std::wstring>& destinations)
    {
        for (const auto& destination :
            destinations)
        {
            if (destination.empty())
            {
                continue;
            }

            DeletePartialFile(
                destination + L".part");

            DeletePartialFile(
                destination + L".ytdl");
        }
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
            if (
                findData.dwFileAttributes &
                FILE_ATTRIBUTE_DIRECTORY)
            {
                continue;
            }

            const std::wstring name =
                findData.cFileName;

            const size_t dotPos =
                name.find_last_of(L'.');

            if (dotPos ==
                std::wstring::npos)
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

            if (
                fileTimeValue.QuadPart +
                FILE_TIME_TOLERANCE <
                startTimeValue.QuadPart)
            {
                continue;
            }

            DeletePartialFile(
                folder + L"\\" + name);

        } while (
            FindNextFileW(
                findHandle,
                &findData));

        FindClose(
            findHandle);
    }

    void CleanupCancelledDownload(
        const std::wstring& downloadsFolder,
        const FILETIME& downloadStart,
        const std::vector<std::wstring>& destinations)
    {
        // Remove destinations explicitly reported by yt-dlp.
        CleanupTrackedPartialFiles(
            destinations);

        // Also catch playlist/early-cancellation partial files
        // that were not reported before the process stopped.
        CleanupRecentPartialFiles(
            downloadsFolder,
            downloadStart);
    }

    std::wstring FindNewestFileSince(
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
            return L"";
        }

        std::wstring bestName;
        FILETIME bestTime{};

        static const wchar_t* ignoredExtensions[] =
        {
            L".part",
            L".ytdl",
            L".webp",
            L".description",
            L".json",
            L".temp"
        };

        ULARGE_INTEGER startTimeValue{};

        startTimeValue.LowPart =
            downloadStart.dwLowDateTime;

        startTimeValue.HighPart =
            downloadStart.dwHighDateTime;

        do
        {
            if (
                findData.dwFileAttributes &
                FILE_ATTRIBUTE_DIRECTORY)
            {
                continue;
            }

            const std::wstring name =
                findData.cFileName;

            const size_t dotPos =
                name.find_last_of(L'.');

            if (dotPos !=
                std::wstring::npos)
            {
                const std::wstring extension =
                    name.substr(dotPos);

                bool ignored = false;

                for (
                    const wchar_t* ignoredExtension :
                    ignoredExtensions)
                {
                    if (_wcsicmp(
                        extension.c_str(),
                        ignoredExtension) == 0)
                    {
                        ignored = true;
                        break;
                    }
                }

                if (ignored)
                {
                    continue;
                }
            }

            ULARGE_INTEGER fileTimeValue{};

            fileTimeValue.LowPart =
                findData.ftLastWriteTime.dwLowDateTime;

            fileTimeValue.HighPart =
                findData.ftLastWriteTime.dwHighDateTime;

            if (
                fileTimeValue.QuadPart +
                FILE_TIME_TOLERANCE <
                startTimeValue.QuadPart)
            {
                continue;
            }

            ULARGE_INTEGER bestTimeValue{};

            bestTimeValue.LowPart =
                bestTime.dwLowDateTime;

            bestTimeValue.HighPart =
                bestTime.dwHighDateTime;

            if (
                bestName.empty() ||
                fileTimeValue.QuadPart >
                bestTimeValue.QuadPart)
            {
                bestName = name;
                bestTime =
                    findData.ftLastWriteTime;
            }

        } while (
            FindNextFileW(
                findHandle,
                &findData));

        FindClose(
            findHandle);

        return bestName.empty()
            ? L""
            : folder + L"\\" + bestName;
    }


    // ============================================================
    // Process / Job Object helpers
    // ============================================================

    HANDLE CreateDownloadJob()
    {
        HANDLE job =
            CreateJobObjectW(
                nullptr,
                nullptr);

        if (job == nullptr)
        {
            return nullptr;
        }

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo{};

        jobInfo.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

        if (!SetInformationJobObject(
            job,
            JobObjectExtendedLimitInformation,
            &jobInfo,
            sizeof(jobInfo)))
        {
            CloseHandle(job);
            return nullptr;
        }

        return job;
    }

    void TerminateDownloadProcess(
        HANDLE processHandle)
    {
        HANDLE job =
            g_jobHandle.load(
                std::memory_order_acquire);

        if (job != nullptr)
        {
            TerminateJobObject(
                job,
                PROCESS_TERMINATION_CODE);

            return;
        }

        if (processHandle != nullptr)
        {
            TerminateProcess(
                processHandle,
                PROCESS_TERMINATION_CODE);
        }
    }

    void ClearPublishedProcessHandles()
    {
        g_processHandle.store(
            nullptr,
            std::memory_order_release);

        g_jobHandle.store(
            nullptr,
            std::memory_order_release);
    }


    // ============================================================
    // yt-dlp command construction
    // ============================================================

    std::wstring BuildCommandLine(
        const std::wstring& ytDlpPath,
        const std::wstring& downloadsFolder,
        const std::wstring& url,
        bool isMp3,
        bool isPlaylist)
    {
        std::wstring commandLine =
            L"\"" +
            ytDlpPath +
            L"\" "
            L"--newline "
            L"--continue ";

        if (isPlaylist)
        {
            commandLine +=
                L"--yes-playlist "
                L"--ignore-errors ";
        }
        else
        {
            commandLine +=
                L"--no-playlist ";
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
                L"-f \"bv*+ba/b\" ";
        }

        commandLine +=
            L"-o \"" +
            downloadsFolder +
            (
                isPlaylist
                ? L"\\%(playlist_index)s - %(title)s.%(ext)s\" "
                : L"\\%(title)s.%(ext)s\" "
                ) +
            L"\"" +
            url +
            L"\"";

        return commandLine;
    }


    // ============================================================
    // yt-dlp output processing
    // ============================================================

    void AddTrackedDestination(
        std::vector<std::wstring>& destinations,
        const std::wstring& destination)
    {
        if (destination.empty())
        {
            return;
        }

        if (
            std::find(
                destinations.begin(),
                destinations.end(),
                destination) ==
            destinations.end())
        {
            destinations.push_back(
                destination);
        }
    }

    std::wstring GetFileNameFromPath(
        const std::wstring& path)
    {
        const size_t slash =
            path.find_last_of(L"\\/");

        return
            slash != std::wstring::npos
            ? path.substr(slash + 1)
            : path;
    }

    void ProcessOutputLine(
        HWND ownerWindow,
        const std::wstring& line,
        std::vector<std::wstring>& trackedDestinations,
        std::wstring& finalFileName)
    {
        if (line.empty())
        {
            return;
        }

        int progress = 0;

        if (TryParseProgress(
            line,
            progress))
        {
            PostMessageW(
                ownerWindow,
                WM_APP_DOWNLOAD_PROGRESS,
                static_cast<WPARAM>(progress),
                0);
        }

        if (
            line.find(
                L"[download] Destination:") !=
            std::wstring::npos)
        {
            const size_t colon =
                line.find(L':');

            if (colon !=
                std::wstring::npos)
            {
                const std::wstring destination =
                    Trim(
                        line.substr(
                            colon + 1));

                PostStatus(
                    ownerWindow,
                    destination);

                AddTrackedDestination(
                    trackedDestinations,
                    destination);

                finalFileName =
                    GetFileNameFromPath(
                        destination);
            }

            return;
        }

        if (
            line.find(
                L"[ExtractAudio] Destination:") !=
            std::wstring::npos)
        {
            const size_t colon =
                line.find(L':');

            if (colon !=
                std::wstring::npos)
            {
                const std::wstring destination =
                    Trim(
                        line.substr(
                            colon + 1));

                AddTrackedDestination(
                    trackedDestinations,
                    destination);

                finalFileName =
                    GetFileNameFromPath(
                        destination);
            }

            PostStatus(
                ownerWindow,
                L"Converting audio.");

            return;
        }

        if (
            line.find(
                L"[Merger] Merging formats into") !=
            std::wstring::npos)
        {
            const size_t firstQuote =
                line.find(L'"');

            const size_t lastQuote =
                line.find_last_of(L'"');

            if (
                firstQuote !=
                std::wstring::npos &&
                lastQuote !=
                std::wstring::npos &&
                lastQuote > firstQuote)
            {
                const std::wstring destination =
                    line.substr(
                        firstQuote + 1,
                        lastQuote -
                        firstQuote -
                        1);

                finalFileName =
                    GetFileNameFromPath(
                        destination);
            }

            PostStatus(
                ownerWindow,
                L"Merging video and audio.");

            return;
        }

        if (
            line.find(L"ERROR:") !=
            std::wstring::npos)
        {
            PostStatus(
                ownerWindow,
                line);
        }
    }

    void ProcessOutputBuffer(
        HWND ownerWindow,
        std::string& pending,
        std::vector<std::wstring>& trackedDestinations,
        std::wstring& finalFileName)
    {
        while (true)
        {
            const size_t newlinePos =
                pending.find('\n');

            if (newlinePos ==
                std::string::npos)
            {
                break;
            }

            std::string rawLine =
                pending.substr(
                    0,
                    newlinePos);

            pending.erase(
                0,
                newlinePos + 1);

            if (
                !rawLine.empty() &&
                rawLine.back() == '\r')
            {
                rawLine.pop_back();
            }

            const std::wstring line =
                ConvertOutputLine(
                    rawLine);

            ProcessOutputLine(
                ownerWindow,
                line,
                trackedDestinations,
                finalFileName);
        }
    }


    // ============================================================
    // Process startup
    // ============================================================

    bool StartYtDlpProcess(
        const std::wstring& commandLine,
        HANDLE& readPipe,
        HANDLE& writePipe,
        PROCESS_INFORMATION& processInfo)
    {
        SECURITY_ATTRIBUTES securityAttributes{};

        securityAttributes.nLength =
            sizeof(securityAttributes);

        securityAttributes.bInheritHandle =
            TRUE;

        if (!CreatePipe(
            &readPipe,
            &writePipe,
            &securityAttributes,
            0))
        {
            return false;
        }

        SetHandleInformation(
            readPipe,
            HANDLE_FLAG_INHERIT,
            0);

        std::vector<wchar_t> commandBuffer(
            commandLine.begin(),
            commandLine.end());

        commandBuffer.push_back(
            L'\0');

        STARTUPINFOW startupInfo{};

        startupInfo.cb =
            sizeof(startupInfo);

        startupInfo.dwFlags =
            STARTF_USESTDHANDLES;

        startupInfo.hStdInput =
            nullptr;

        startupInfo.hStdOutput =
            writePipe;

        startupInfo.hStdError =
            writePipe;

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

        return created != FALSE;
    }

    bool AttachProcessToJob(
        PROCESS_INFORMATION& processInfo,
        HANDLE& jobHandle,
        DWORD& errorCode)
    {
        jobHandle =
            CreateDownloadJob();

        if (jobHandle == nullptr)
        {
            errorCode =
                GetLastError();

            return false;
        }

        if (!AssignProcessToJobObject(
            jobHandle,
            processInfo.hProcess))
        {
            errorCode =
                GetLastError();

            CloseHandle(jobHandle);
            jobHandle = nullptr;

            return false;
        }

        return true;
    }

    void FailWorkerStartup(
        HWND ownerWindow,
        const std::wstring& status,
        HANDLE readPipe,
        HANDLE processHandle)
    {
        if (processHandle != nullptr)
        {
            TerminateProcess(
                processHandle,
                PROCESS_TERMINATION_CODE);

            WaitForSingleObject(
                processHandle,
                INFINITE);

            CloseHandle(
                processHandle);
        }

        if (readPipe != nullptr)
        {
            CloseHandle(readPipe);
        }

        PostStatus(
            ownerWindow,
            status);

        PostFinished(
            ownerWindow,
            PRE_LAUNCH_FAILURE_CODE,
            false,
            false,
            false,
            L"",
            L"");
    }


    // ============================================================
    // Worker thread
    // ============================================================

    void WorkerThread(
        HWND ownerWindow,
        std::wstring url,
        bool isMp3,
        bool isPlaylist,
        std::wstring ytDlpPath,
        std::wstring downloadsFolder)
    {
        const std::wstring commandLine =
            BuildCommandLine(
                ytDlpPath,
                downloadsFolder,
                url,
                isMp3,
                isPlaylist);

        HANDLE readPipe = nullptr;
        HANDLE writePipe = nullptr;

        PROCESS_INFORMATION processInfo{};

        FILETIME downloadStartTime{};

        GetSystemTimeAsFileTime(
            &downloadStartTime);

        // --------------------------------------------------------
        // Start yt-dlp
        // --------------------------------------------------------

        if (!StartYtDlpProcess(
            commandLine,
            readPipe,
            writePipe,
            processInfo))
        {
            const DWORD errorCode =
                GetLastError();

            const std::wstring errorText =
                L"Failed to start yt-dlp.exe. "
                L"Windows error code: " +
                std::to_wstring(errorCode) +
                L".";

            PostStatus(
                ownerWindow,
                errorText);

            PostFinished(
                ownerWindow,
                PRE_LAUNCH_FAILURE_CODE,
                isMp3,
                false,
                false,
                downloadsFolder,
                L"");

            g_downloadRunning = false;
            return;
        }

        CloseHandle(
            processInfo.hThread);

        // --------------------------------------------------------
        // Create and attach Job Object
        // --------------------------------------------------------

        HANDLE jobHandle = nullptr;

        DWORD jobErrorCode = 0;

        if (!AttachProcessToJob(
            processInfo,
            jobHandle,
            jobErrorCode))
        {
            if (processInfo.hProcess != nullptr)
            {
                TerminateProcess(
                    processInfo.hProcess,
                    PROCESS_TERMINATION_CODE);

                WaitForSingleObject(
                    processInfo.hProcess,
                    INFINITE);

                CloseHandle(
                    processInfo.hProcess);
            }

            CloseHandle(
                readPipe);

            if (jobErrorCode == 0)
            {
                PostStatus(
                    ownerWindow,
                    L"Failed to create the download process group.");
            }
            else
            {
                PostStatus(
                    ownerWindow,
                    L"Failed to attach yt-dlp to its process group. "
                    L"Windows error code: " +
                    std::to_wstring(jobErrorCode) +
                    L".");
            }

            PostFinished(
                ownerWindow,
                PRE_LAUNCH_FAILURE_CODE,
                isMp3,
                false,
                false,
                downloadsFolder,
                L"");

            g_downloadRunning = false;
            return;
        }

        g_processHandle.store(
            processInfo.hProcess,
            std::memory_order_release);

        g_jobHandle.store(
            jobHandle,
            std::memory_order_release);

        PostStatus(
            ownerWindow,
            L"Starting download...");

        // --------------------------------------------------------
        // Read yt-dlp output
        // --------------------------------------------------------

        std::string buffer(
            4096,
            '\0');

        std::string pending;

        std::wstring finalFileName;

        std::vector<std::wstring>
            trackedDestinations;

        bool terminationRequested = false;

        while (true)
        {
            const bool stopRequested =
                g_stopRequested.load(
                    std::memory_order_acquire);

            const bool pauseRequested =
                g_pauseRequested.load(
                    std::memory_order_acquire);

            if (
                (stopRequested ||
                    pauseRequested) &&
                !terminationRequested)
            {
                TerminateDownloadProcess(
                    processInfo.hProcess);

                terminationRequested = true;
            }

            DWORD bytesRead = 0;

            const BOOL readOk =
                ReadFile(
                    readPipe,
                    buffer.data(),
                    static_cast<DWORD>(
                        buffer.size()),
                    &bytesRead,
                    nullptr);

            if (
                !readOk ||
                bytesRead == 0)
            {
                break;
            }

            pending.append(
                buffer.data(),
                bytesRead);

            ProcessOutputBuffer(
                ownerWindow,
                pending,
                trackedDestinations,
                finalFileName);
        }

        CloseHandle(
            readPipe);

        WaitForSingleObject(
            processInfo.hProcess,
            INFINITE);

        DWORD exitCode = 1;

        GetExitCodeProcess(
            processInfo.hProcess,
            &exitCode);

        const bool wasPaused =
            g_pauseRequested.load(
                std::memory_order_acquire);

        const bool wasCancelled =
            g_stopRequested.load(
                std::memory_order_acquire);

        ClearPublishedProcessHandles();

        CloseHandle(
            processInfo.hProcess);

        // Closing the Job Object handle enforces
        // JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE.
        CloseHandle(
            jobHandle);

        // --------------------------------------------------------
        // Determine final state
        // --------------------------------------------------------

        std::wstring resolvedFilePath;

        if (wasPaused)
        {
            // Pause deliberately preserves .part files.
            PostStatus(
                ownerWindow,
                L"Paused.");
        }
        else if (wasCancelled)
        {
            // Cancel permanently removes partial files.
            CleanupCancelledDownload(
                downloadsFolder,
                downloadStartTime,
                trackedDestinations);

            PostStatus(
                ownerWindow,
                L"Download cancelled.");
        }
        else if (exitCode == 0)
        {
            PostMessageW(
                ownerWindow,
                WM_APP_DOWNLOAD_PROGRESS,
                100,
                0);

            PostStatus(
                ownerWindow,
                L"Download complete.");

            if (!finalFileName.empty())
            {
                const std::wstring candidate =
                    downloadsFolder +
                    L"\\" +
                    finalFileName;

                if (
                    GetFileAttributesW(
                        candidate.c_str()) !=
                    INVALID_FILE_ATTRIBUTES)
                {
                    resolvedFilePath =
                        candidate;
                }
            }

            if (resolvedFilePath.empty())
            {
                resolvedFilePath =
                    FindNewestFileSince(
                        downloadsFolder,
                        downloadStartTime);
            }
        }

        // --------------------------------------------------------
        // Notify UI
        // --------------------------------------------------------

        PostFinished(
            ownerWindow,
            exitCode,
            isMp3,
            wasPaused,
            wasCancelled,
            downloadsFolder,
            resolvedFilePath);

        // --------------------------------------------------------
        // Reset global state
        // --------------------------------------------------------

        g_stopRequested = false;
        g_pauseRequested = false;
        g_downloadRunning = false;
    }
}


// =================================================================
// Public DownloadManager API
// =================================================================

namespace DownloadManager
{
    bool StartDownload(
        HWND ownerWindow,
        const std::wstring& url,
        bool isMp3,
        bool isPlaylist)
    {
        if (url.empty())
        {
            MessageBoxW(
                ownerWindow,
                L"Please enter a video URL.",
                L"IT Downloader V2",
                MB_OK | MB_ICONWARNING);

            return false;
        }

        if (g_downloadRunning.exchange(true))
        {
            MessageBoxW(
                ownerWindow,
                L"A download is already running.",
                L"IT Downloader V2",
                MB_OK | MB_ICONWARNING);

            return false;
        }

        g_stopRequested = false;
        g_pauseRequested = false;

        g_processHandle.store(
            nullptr,
            std::memory_order_release);

        g_jobHandle.store(
            nullptr,
            std::memory_order_release);

        const std::wstring ytDlpPath =
            GetYtDlpPath();

        if (
            GetFileAttributesW(
                ytDlpPath.c_str()) ==
            INVALID_FILE_ATTRIBUTES)
        {
            g_downloadRunning = false;

            const std::wstring message =
                L"yt-dlp.exe was not found at:\n" +
                ytDlpPath +
                L"\n\nMake sure it's in a 'bin' folder next to this program.";

            MessageBoxW(
                ownerWindow,
                message.c_str(),
                L"IT Downloader V2",
                MB_OK | MB_ICONERROR);

            return false;
        }

        const std::wstring downloadsFolder =
            GetDownloadsFolder(isMp3);

        if (!EnsureFolderExists(
            downloadsFolder))
        {
            g_downloadRunning = false;

            const std::wstring errorMessage =
                L"Unable to create the download folder:\n\n" +
                downloadsFolder;

            MessageBoxW(
                ownerWindow,
                errorMessage.c_str(),
                L"IT Downloader V2",
                MB_OK | MB_ICONERROR);

            return false;
        }

        std::thread(
            WorkerThread,
            ownerWindow,
            url,
            isMp3,
            isPlaylist,
            ytDlpPath,
            downloadsFolder)
            .detach();

        return true;
    }

    void CancelDownload()
    {
        if (!g_downloadRunning)
        {
            return;
        }

        // Permanent cancellation.
        g_stopRequested = true;

        HANDLE job =
            g_jobHandle.load(
                std::memory_order_acquire);

        if (job != nullptr)
        {
            TerminateJobObject(
                job,
                PROCESS_TERMINATION_CODE);

            return;
        }

        HANDLE process =
            g_processHandle.load(
                std::memory_order_acquire);

        if (process != nullptr)
        {
            TerminateProcess(
                process,
                PROCESS_TERMINATION_CODE);
        }
    }

    void PauseDownload()
    {
        if (!g_downloadRunning)
        {
            return;
        }

        // Pause is implemented as a process-tree stop.
        //
        // The partial files are deliberately preserved.
        // Resume starts yt-dlp again with --continue.
        g_pauseRequested = true;

        HANDLE job =
            g_jobHandle.load(
                std::memory_order_acquire);

        if (job != nullptr)
        {
            TerminateJobObject(
                job,
                PROCESS_TERMINATION_CODE);

            return;
        }

        HANDLE process =
            g_processHandle.load(
                std::memory_order_acquire);

        if (process != nullptr)
        {
            TerminateProcess(
                process,
                PROCESS_TERMINATION_CODE);
        }
    }
}