#include "DownloadOutput.h"

#include "DownloadUtils.h"

#include <algorithm>

namespace
{
    bool TryGetDestination(
        const std::wstring& line,
        const std::wstring& marker,
        std::wstring& destination)
    {
        const size_t markerPos =
            line.find(marker);

        if (markerPos == std::wstring::npos)
        {
            return false;
        }

        const size_t destinationStart =
            markerPos + marker.length();

        if (destinationStart >= line.length())
        {
            return false;
        }

        destination =
            DownloadUtils::Trim(
                line.substr(destinationStart));

        return !destination.empty();
    }

    void TrackDestination(
        const std::wstring& destination,
        std::vector<std::wstring>& trackedDestinations,
        std::wstring& finalFileName)
    {
        if (destination.empty())
        {
            return;
        }

        if (std::find(
            trackedDestinations.begin(),
            trackedDestinations.end(),
            destination) ==
            trackedDestinations.end())
        {
            trackedDestinations.push_back(
                destination);
        }

        const size_t nameSlash =
            destination.find_last_of(
                L"\\/");

        finalFileName =
            (nameSlash != std::wstring::npos)
            ? destination.substr(
                nameSlash + 1)
            : destination;
    }
}

namespace DownloadOutput
{
    bool TryParseProgress(
        const std::wstring& line,
        int& progress)
    {
        const size_t percentPos =
            line.find(L'%');

        if (percentPos ==
            std::wstring::npos)
        {
            return false;
        }

        size_t end =
            percentPos;

        size_t start =
            end;

        while (start > 0)
        {
            const wchar_t c =
                line[start - 1];

            if ((c >= L'0' &&
                 c <= L'9') ||
                c == L'.')
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

            if (value < 0.0 ||
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
            reinterpret_cast<LPARAM>(
                message)))
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

        info->exitCode =
            exitCode;

        info->isMp3 =
            isMp3;

        info->wasPaused =
            wasPaused;

        info->wasCancelled =
            wasCancelled;

        info->downloadsFolder =
            downloadsFolder;

        info->filePath =
            filePath;

        if (!PostMessageW(
            ownerWindow,
            WM_APP_DOWNLOAD_FINISHED,
            0,
            reinterpret_cast<LPARAM>(
                info)))
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

        // ---------------------------------------------------------
        // Normal yt-dlp download destination.
        //
        // Example:
        // [download] Destination:
        // C:\...\video.f399.mp4
        // ---------------------------------------------------------
        std::wstring destination;

        if (TryGetDestination(
            line,
            L"[download] Destination:",
            destination))
        {
            PostStatus(
                ownerWindow,
                destination);

            TrackDestination(
                destination,
                trackedDestinations,
                finalFileName);

            return;
        }

        // ---------------------------------------------------------
        // Audio extraction destination.
        //
        // Example:
        // [ExtractAudio] Destination:
        // C:\...\song.mp3
        // ---------------------------------------------------------
        if (TryGetDestination(
            line,
            L"[ExtractAudio] Destination:",
            destination))
        {
            TrackDestination(
                destination,
                trackedDestinations,
                finalFileName);

            PostStatus(
                ownerWindow,
                L"Converting audio...");

            return;
        }

        // ---------------------------------------------------------
        // Video merge destination.
        //
        // Example:
        // [Merger] Merging formats into
        // "C:\...\video.mp4"
        // ---------------------------------------------------------
        if (line.find(
            L"[Merger] Merging formats into") !=
            std::wstring::npos)
        {
            const size_t firstQuote =
                line.find(L'"');

            const size_t lastQuote =
                line.find_last_of(L'"');

            if (firstQuote !=
                    std::wstring::npos &&
                lastQuote !=
                    std::wstring::npos &&
                lastQuote > firstQuote)
            {
                destination =
                    line.substr(
                        firstQuote + 1,
                        lastQuote -
                            firstQuote -
                            1);

                destination =
                    DownloadUtils::Trim(
                        destination);

                const size_t nameSlash =
                    destination.find_last_of(
                        L"\\/");

                finalFileName =
                    (nameSlash !=
                        std::wstring::npos)
                    ? destination.substr(
                        nameSlash + 1)
                    : destination;
            }

            PostStatus(
                ownerWindow,
                L"Merging video and audio...");

            return;
        }

        // ---------------------------------------------------------
        // Error reporting.
        // ---------------------------------------------------------
        if (line.find(L"ERROR:") !=
            std::wstring::npos)
        {
            PostStatus(
                ownerWindow,
                line);
        }
    }
}