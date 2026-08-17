#include "DownloadManager.h"

#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <cwctype>

namespace
{
    std::atomic<bool> g_downloadRunning{ false };
    std::atomic<bool> g_stopRequested{ false };
    HANDLE g_processHandle = nullptr;

    std::wstring GetExeDirectory()
    {
        wchar_t exePath[MAX_PATH]{};
        DWORD length = GetModuleFileNameW(nullptr, exePath, MAX_PATH);

        if (length == 0)
        {
            return L".";
        }

        std::wstring path(exePath, length);
        const size_t lastSlash = path.find_last_of(L"\\/");

        return (lastSlash != std::wstring::npos)
            ? path.substr(0, lastSlash)
            : L".";
    }

    std::wstring GetYtDlpPath()
    {
        return GetExeDirectory() + L"\\bin\\yt-dlp.exe";
    }

    std::wstring GetDownloadsFolder(bool isMp3)
    {
        wchar_t* userProfile = nullptr;
        size_t len = 0;
        std::wstring folder;

        if (_wdupenv_s(&userProfile, &len, L"USERPROFILE") == 0 &&
            userProfile != nullptr)
        {
            folder = userProfile;
            free(userProfile);
        }

        if (folder.empty())
        {
            folder = L".";
        }

        return folder + (isMp3
            ? L"\\Downloads\\Music"
            : L"\\Downloads\\Video");
    }

    bool EnsureFolderExists(const std::wstring& folder)
    {
        // Create the normal Downloads folder first, then the V2 subfolder.
        const size_t slash = folder.find_last_of(L"\\/");

        if (slash != std::wstring::npos)
        {
            const std::wstring parent = folder.substr(0, slash);
            CreateDirectoryW(parent.c_str(), nullptr);
        }

        if (CreateDirectoryW(folder.c_str(), nullptr))
        {
            return true;
        }

        return GetLastError() == ERROR_ALREADY_EXISTS;
    }

    std::wstring Trim(const std::wstring& text)
    {
        size_t start = 0;

        while (start < text.size() && iswspace(text[start]))
        {
            ++start;
        }

        size_t end = text.size();

        while (end > start && iswspace(text[end - 1]))
        {
            --end;
        }

        return text.substr(start, end - start);
    }

    bool TryParseProgress(
        const std::wstring& line,
        int& progress)
    {
        const size_t percentPos = line.find(L'%');

        if (percentPos == std::wstring::npos)
        {
            return false;
        }

        size_t end = percentPos;
        size_t start = end;

        while (start > 0)
        {
            const wchar_t c = line[start - 1];

            if ((c >= L'0' && c <= L'9') || c == L'.')
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
                std::stod(line.substr(start, end - start));

            if (value < 0.0 || value > 100.0)
            {
                return false;
            }

            progress = static_cast<int>(value);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void PostStatus(
        HWND ownerWindow,
        const std::wstring& text)
    {
        auto* message = new std::wstring(text);

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
        const std::wstring& downloadsFolder)
    {
        auto* info = new DownloadFinishedInfo;

        info->exitCode = exitCode;
        info->isMp3 = isMp3;
        info->downloadsFolder = downloadsFolder;

        if (!PostMessageW(
            ownerWindow,
            WM_APP_DOWNLOAD_FINISHED,
            0,
            reinterpret_cast<LPARAM>(info)))
        {
            delete info;
        }
    }

    void WorkerThread(
        HWND ownerWindow,
        std::wstring url,
        bool isMp3,
        std::wstring ytDlpPath,
        std::wstring downloadsFolder)
    {
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
            PostStatus(
                ownerWindow,
                L"Failed to create the output pipe.");

            PostFinished(
                ownerWindow,
                1,
                isMp3,
                downloadsFolder);

            g_downloadRunning = false;
            return;
        }

        // The parent must keep the read side private.
        SetHandleInformation(
            readPipe,
            HANDLE_FLAG_INHERIT,
            0);

        std::wstring commandLine =
            L"\"" + ytDlpPath + L"\" "
            L"--newline "
            L"--no-playlist ";

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
            L"\\%(title)s.%(ext)s\" "
            L"\"" +
            url +
            L"\"";

        std::vector<wchar_t> commandBuffer(
            commandLine.begin(),
            commandLine.end());

        commandBuffer.push_back(L'\0');

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

        PROCESS_INFORMATION processInfo{};

        const BOOL created = CreateProcessW(
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

        // The child now owns its inherited copy.
        // The parent closes its write handle so ReadFile
        // can reach EOF when yt-dlp exits.
        CloseHandle(writePipe);
        writePipe = nullptr;

        if (!created)
        {
            const DWORD errorCode = GetLastError();

            CloseHandle(readPipe);

            PostStatus(
                ownerWindow,
                L"Failed to start yt-dlp.exe.");

            PostFinished(
                ownerWindow,
                errorCode,
                isMp3,
                downloadsFolder);

            g_downloadRunning = false;
            return;
        }

        CloseHandle(processInfo.hThread);

        g_processHandle =
            processInfo.hProcess;

        PostStatus(
            ownerWindow,
            L"Starting download...");

        std::string buffer(4096, '\0');
        std::string pending;

        bool cancellationSent = false;

        while (true)
        {
            if (g_stopRequested &&
                !cancellationSent)
            {
                TerminateProcess(
                    processInfo.hProcess,
                    1);

                cancellationSent = true;
            }

            DWORD bytesRead = 0;

            const BOOL readOk = ReadFile(
                readPipe,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytesRead,
                nullptr);

            if (!readOk || bytesRead == 0)
            {
                break;
            }

            pending.append(
                buffer.data(),
                bytesRead);

            size_t newlinePos =
                std::string::npos;

            while ((newlinePos =
                pending.find('\n')) !=
                std::string::npos)
            {
                std::string rawLine =
                    pending.substr(
                        0,
                        newlinePos);

                pending.erase(
                    0,
                    newlinePos + 1);

                if (!rawLine.empty() &&
                    rawLine.back() == '\r')
                {
                    rawLine.pop_back();
                }

                // yt-dlp normally emits UTF-8.
                // Convert it to UTF-16 for Windows.
                int wideLength =
                    MultiByteToWideChar(
                        CP_UTF8,
                        MB_ERR_INVALID_CHARS,
                        rawLine.data(),
                        static_cast<int>(
                            rawLine.size()),
                        nullptr,
                        0);

                if (wideLength <= 0)
                {
                    wideLength =
                        MultiByteToWideChar(
                            CP_ACP,
                            0,
                            rawLine.data(),
                            static_cast<int>(
                                rawLine.size()),
                            nullptr,
                            0);
                }

                if (wideLength <= 0)
                {
                    continue;
                }

                std::wstring line(
                    wideLength,
                    L'\0');

                if (MultiByteToWideChar(
                    CP_UTF8,
                    MB_ERR_INVALID_CHARS,
                    rawLine.data(),
                    static_cast<int>(
                        rawLine.size()),
                    line.data(),
                    wideLength) <= 0)
                {
                    MultiByteToWideChar(
                        CP_ACP,
                        0,
                        rawLine.data(),
                        static_cast<int>(
                            rawLine.size()),
                        line.data(),
                        wideLength);
                }

                line = Trim(line);

                int progress = 0;

                if (TryParseProgress(
                    line,
                    progress))
                {
                    PostMessageW(
                        ownerWindow,
                        WM_APP_DOWNLOAD_PROGRESS,
                        static_cast<WPARAM>(
                            progress),
                        0);
                }

                if (line.find(
                    L"[download] Destination:") !=
                    std::wstring::npos)
                {
                    const size_t colon =
                        line.find(L':');

                    if (colon !=
                        std::wstring::npos)
                    {
                        PostStatus(
                            ownerWindow,
                            Trim(
                                line.substr(
                                    colon + 1)));
                    }
                }
                else if (line.find(
                    L"[ExtractAudio]") !=
                    std::wstring::npos)
                {
                    PostStatus(
                        ownerWindow,
                        L"Converting audio...");
                }
                else if (line.find(
                    L"[Merger]") !=
                    std::wstring::npos)
                {
                    PostStatus(
                        ownerWindow,
                        L"Merging video and audio...");
                }
                else if (line.find(
                    L"ERROR:") !=
                    std::wstring::npos)
                {
                    PostStatus(
                        ownerWindow,
                        line);
                }
            }
        }

        CloseHandle(readPipe);

        WaitForSingleObject(
            processInfo.hProcess,
            INFINITE);

        DWORD exitCode = 1;

        GetExitCodeProcess(
            processInfo.hProcess,
            &exitCode);

        CloseHandle(
            processInfo.hProcess);

        g_processHandle = nullptr;

        if (g_stopRequested)
        {
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
        }

        PostFinished(
            ownerWindow,
            exitCode,
            isMp3,
            downloadsFolder);

        g_stopRequested = false;
        g_downloadRunning = false;
    }
}

namespace DownloadManager
{
    bool StartDownload(
        HWND ownerWindow,
        const std::wstring& url,
        bool isMp3)
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

        const std::wstring ytDlpPath =
            GetYtDlpPath();

        if (GetFileAttributesW(
            ytDlpPath.c_str()) ==
            INVALID_FILE_ATTRIBUTES)
        {
            g_downloadRunning = false;

            std::wstring message =
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

            // FIX:
            // MessageBoxW requires LPCWSTR.
            // Build the message as std::wstring first,
            // then pass message.c_str().
            std::wstring errorMessage =
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
            ytDlpPath,
            downloadsFolder)
            .detach();

        return true;
    }

    void StopDownload()
    {
        if (!g_downloadRunning)
        {
            return;
        }

        g_stopRequested = true;

        HANDLE process =
            g_processHandle;

        if (process != nullptr)
        {
            TerminateProcess(
                process,
                1);
        }
    }
}