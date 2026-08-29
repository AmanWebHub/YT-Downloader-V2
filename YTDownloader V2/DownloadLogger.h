#pragma once

#include <windows.h>
#include <string>
#include <fstream>
#include <mutex>

namespace DownloadLogger
{
    inline std::mutex& GetMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    inline std::wstring GetLogPath()
    {
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

        if (slash == std::wstring::npos)
        {
            return L"download_debug.log";
        }

        return path.substr(0, slash + 1) +
               L"download_debug.log";
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

    inline void Write(
        const std::wstring& component,
        const std::wstring& message)
    {
        std::lock_guard<std::mutex> lock(GetMutex());

        std::ofstream file(
            GetLogPath(),
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
