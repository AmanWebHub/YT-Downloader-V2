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
}
