#pragma once

#include <windows.h>
#include <string>
#include <fstream>
#include <mutex>
#include <cstdlib>

namespace DownloadLogger
{
    inline std::mutex& GetMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    // Logs live under the user's own per-app data folder
    // (%LOCALAPPDATA%\YTDownloaderV2\logs\) rather than next to the
    // exe - the conventional place for a Windows app's own logs, and
    // one that doesn't depend on the exe's install location being
    // writable. Creates the folder on first use if it doesn't exist.
    inline std::wstring GetLogPath()
    {
        wchar_t* localAppData = nullptr;
        size_t len = 0;

        std::wstring folder;

        if (_wdupenv_s(
            &localAppData,
            &len,
            L"LOCALAPPDATA") == 0 &&
            localAppData != nullptr)
        {
            folder = localAppData;
            free(localAppData);
        }

        if (folder.empty())
        {
            // Fall back to next to the exe if LOCALAPPDATA is
            // somehow unavailable, rather than losing logging
            // entirely.
            wchar_t modulePath[MAX_PATH]{};

            const DWORD length =
                GetModuleFileNameW(
                    nullptr,
                    modulePath,
                    MAX_PATH);

            if (length == 0)
            {
                return L"download_debug.log";
            }

            std::wstring path(
                modulePath,
                length);

            const size_t slash =
                path.find_last_of(L"\\/");

            return (slash == std::wstring::npos)
                ? L"download_debug.log"
                : path.substr(0, slash + 1) +
                    L"download_debug.log";
        }

        const std::wstring appFolder =
            folder + L"\\YTDownloaderV2";

        const std::wstring logsFolder =
            appFolder + L"\\logs";

        CreateDirectoryW(
            appFolder.c_str(),
            nullptr);

        CreateDirectoryW(
            logsFolder.c_str(),
            nullptr);

        return logsFolder + L"\\download_debug.log";
    }

    inline std::string WideToUtf8(
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

    inline std::wstring GetTimestamp()
    {
        SYSTEMTIME time{};
        GetLocalTime(&time);

        wchar_t buffer[64]{};

        swprintf_s(
            buffer,
            L"%04u-%02u-%02u %02u:%02u:%02u.%03u",
            time.wYear,
            time.wMonth,
            time.wDay,
            time.wHour,
            time.wMinute,
            time.wSecond,
            time.wMilliseconds);

        return buffer;
    }

    // Keeps the log from growing without bound over long-term use -
    // if it's already past this size when we're about to write,
    // start a fresh file instead of appending forever. Simple
    // truncate-and-restart rather than rotating/keeping an archive:
    // this log is for in-the-moment troubleshooting of the current
    // or most recent run, not a long-term history.
    constexpr long long kMaxLogSizeBytes =
        5LL * 1024 * 1024;

    inline void TrimIfTooLarge(
        const std::wstring& path)
    {
        WIN32_FILE_ATTRIBUTE_DATA info{};

        if (!GetFileAttributesExW(
            path.c_str(),
            GetFileExInfoStandard,
            &info))
        {
            // Doesn't exist yet (or can't be queried) - nothing to
            // trim.
            return;
        }

        const long long size =
            (static_cast<long long>(info.nFileSizeHigh) << 32) |
            info.nFileSizeLow;

        if (size > kMaxLogSizeBytes)
        {
            DeleteFileW(
                path.c_str());
        }
    }

    inline void Write(
        const std::wstring& component,
        const std::wstring& message)
    {
        std::lock_guard<std::mutex> lock(GetMutex());

        const std::wstring path =
            GetLogPath();

        TrimIfTooLarge(path);

        std::ofstream file(
            path,
            std::ios::binary |
            std::ios::app);

        if (!file.is_open())
        {
            return;
        }

        const std::wstring line =
            L"[" +
            GetTimestamp() +
            L"] [" +
            component +
            L"] " +
            message +
            L"\r\n";

        const std::string utf8 =
            WideToUtf8(line);

        file.write(
            utf8.data(),
            static_cast<std::streamsize>(utf8.size()));

        file.flush();
    }
}
