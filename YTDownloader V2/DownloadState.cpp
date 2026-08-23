#include "DownloadState.h"

namespace DownloadState
{
    std::atomic<bool> downloadRunning{ false };
    std::atomic<bool> stopRequested{ false };
    std::atomic<bool> pauseRequested{ false };

    std::atomic<HANDLE> processHandle{ nullptr };
    std::atomic<HANDLE> jobHandle{ nullptr };
}
