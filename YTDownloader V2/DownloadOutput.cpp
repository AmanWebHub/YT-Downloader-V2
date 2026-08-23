#include "DownloadOutput.h"

#include "DownloadUtils.h"

#include <algorithm>

namespace DownloadOutput
{
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
        bool wasPaused,
        bool wasCancelled,
        const std::wstring& downloadsFolder,
        const std::wstring& filePath)
    {
        auto* info = new DownloadFinishedInfo;

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

    void ProcessLine(
        HWND ownerWindow,
        const std::wstring& line,
        std::vector<std::wstring>& trackedDestinations,
        std::wstring& finalFileName)
    {
        int progress = 0;

        if (TryParseProgress(line, progress))
        {
            PostMessageW(
                ownerWindow,
                WM_APP_DOWNLOAD_PROGRESS,
                static_cast<WPARAM>(progress),
                0);
        }

        if (line.find(L"[download] Destination:") !=
            std::wstring::npos)
        {
            const size_t colon = line.find(L':');

            if (colon != std::wstring::npos)
            {
                const std::wstring destName =
                    DownloadUtils::Trim(line.substr(colon + 1));

                PostStatus(ownerWindow, destName);

                if (std::find(
                    trackedDestinations.begin(),
                    trackedDestinations.end(),
                    destName) == trackedDestinations.end())
                {
                    trackedDestinations.push_back(destName);
                }

                const size_t nameSlash =
                    destName.find_last_of(L"\\/");

                finalFileName =
                    (nameSlash != std::wstring::npos)
                    ? destName.substr(nameSlash + 1)
                    : destName;
            }
        }
        else if (line.find(L"[ExtractAudio] Destination:") !=
                 std::wstring::npos)
        {
            const size_t colon = line.find(L':');

            if (colon != std::wstring::npos)
            {
                const std::wstring destName =
                    DownloadUtils::Trim(line.substr(colon + 1));

                if (std::find(
                    trackedDestinations.begin(),
                    trackedDestinations.end(),
                    destName) == trackedDestinations.end())
                {
                    trackedDestinations.push_back(destName);
                }

                const size_t nameSlash =
                    destName.find_last_of(L"\\/");

                finalFileName =
                    (nameSlash != std::wstring::npos)
                    ? destName.substr(nameSlash + 1)
                    : destName;
            }

            PostStatus(ownerWindow, L"Converting audio...");
        }
        else if (line.find(L"[Merger] Merging formats into") !=
                 std::wstring::npos)
        {
            const size_t firstQuote = line.find(L'"');
            const size_t lastQuote = line.find_last_of(L'"');

            if (firstQuote != std::wstring::npos &&
                lastQuote != std::wstring::npos &&
                lastQuote > firstQuote)
            {
                const std::wstring destName =
                    line.substr(
                        firstQuote + 1,
                        lastQuote - firstQuote - 1);

                const size_t nameSlash =
                    destName.find_last_of(L"\\/");

                finalFileName =
                    (nameSlash != std::wstring::npos)
                    ? destName.substr(nameSlash + 1)
                    : destName;
            }

            PostStatus(ownerWindow, L"Merging video and audio...");
        }
        else if (line.find(L"ERROR:") != std::wstring::npos)
        {
            PostStatus(ownerWindow, line);
        }
    }
}
