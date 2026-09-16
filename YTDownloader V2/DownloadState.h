#pragma once

#include <windows.h>
#include <atomic>

namespace DownloadState
{
    extern std::atomic<bool> downloadRunning;
    extern std::atomic<bool> stopRequested;
    extern std::atomic<bool> pauseRequested;

    extern std::atomic<HANDLE> processHandle;
    extern std::atomic<HANDLE> jobHandle;

    // Millisecond tick (GetTickCount64) of the last time yt-dlp
    // produced any output line, used by the stall watchdog in
    // DownloadWorker::Run to detect a hung download - yt-dlp's own
    // --socket-timeout doesn't cover every internal operation it can
    // block on (e.g. a format-testing step that shells out to an
    // external helper), so this is a backstop independent of that.
    extern std::atomic<long long> lastOutputTick;

    // True when the current download was auto-cancelled by the
    // stall watchdog rather than by the user clicking Cancel/Pause -
    // lets the post-download status message say "stalled" instead of
    // the misleading "cancelled".
    extern std::atomic<bool> stalledByWatchdog;
}
