#include "DownloadManager.h"
#include <vector>
#include <string>

namespace
{
    // The exe is expected to sit next to a "bin" folder containing
    // yt-dlp.exe (and ffmpeg.exe), same layout as the V1.1 project.
    std::wstring GetYtDlpPath()
    {
        wchar_t exePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);

        std::wstring path(exePath);
        size_t lastSlash = path.find_last_of(L'\\');
        std::wstring exeDir = (lastSlash != std::wstring::npos)
            ? path.substr(0, lastSlash)
            : L".";

        return exeDir + L"\\bin\\yt-dlp.exe";
    }

    std::wstring GetDownloadsFolder(bool isMp3)
    {
        wchar_t* userProfile = nullptr;
        size_t len = 0;
        std::wstring folder;

        if (_wdupenv_s(&userProfile, &len, L"USERPROFILE") == 0 && userProfile != nullptr)
        {
            folder = userProfile;
            free(userProfile);
        }

        folder += isMp3 ? L"\\Downloads\\Music" : L"\\Downloads\\Video";
        return folder;
    }

    void EnsureFolderExists(const std::wstring& folder)
    {
        CreateDirectoryW(folder.c_str(), nullptr);
        // Ignores the "already exists" case - CreateDirectoryW just
        // returns false for that, which is fine here.
    }
}

namespace DownloadManager
{
    void StartDownload(HWND ownerWindow, const std::wstring& url, bool isMp3)
    {
        if (url.empty())
        {
            MessageBoxW(ownerWindow, L"Please enter a video URL.", L"IT Downloader V2", MB_OK | MB_ICONWARNING);
            return;
        }

        std::wstring ytDlpPath = GetYtDlpPath();
        if (GetFileAttributesW(ytDlpPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            std::wstring message = L"yt-dlp.exe was not found at:\n" + ytDlpPath +
                L"\n\nMake sure it's in a 'bin' folder next to this program.";
            MessageBoxW(ownerWindow, message.c_str(), L"IT Downloader V2", MB_OK | MB_ICONERROR);
            return;
        }

        std::wstring downloadsFolder = GetDownloadsFolder(isMp3);
        EnsureFolderExists(downloadsFolder);

        // Build the command line
        std::wstring commandLine = L"\"" + ytDlpPath + L"\" --no-playlist ";

        if (isMp3)
        {
            commandLine += L"--extract-audio --audio-format mp3 --audio-quality 0 "
                L"--embed-thumbnail --add-metadata ";
        }
        else
        {
            commandLine += L"-f \"bv*+ba/b\" ";
        }

        commandLine += L"-o \"" + downloadsFolder + L"\\%(title)s.%(ext)s\" ";
        commandLine += L"\"" + url + L"\"";

        // CreateProcessW requires a mutable buffer for the command line
        std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
        cmdBuffer.push_back(L'\0');

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        BOOL created = CreateProcessW(
            nullptr,
            cmdBuffer.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi
        );

        if (!created)
        {
            MessageBoxW(ownerWindow, L"Failed to start yt-dlp.", L"IT Downloader V2", MB_OK | MB_ICONERROR);
            return;
        }

        // Blocking for now - the window will freeze during the download.
        // This gets fixed in the next step when we add a progress window
        // that polls asynchronously instead of waiting like this.
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        std::wstring resultMessage = (exitCode == 0)
            ? L"Download complete!\n\nSaved to: " + downloadsFolder
            : L"yt-dlp exited with an error (code " + std::to_wstring(exitCode) + L").";

        MessageBoxW(
            ownerWindow,
            resultMessage.c_str(),
            L"IT Downloader V2",
            exitCode == 0 ? (MB_OK | MB_ICONINFORMATION) : (MB_OK | MB_ICONERROR)
        );
    }
}