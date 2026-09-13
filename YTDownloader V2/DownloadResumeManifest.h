#pragma once

#include <windows.h>
#include <string>
#include <vector>

// Extracted from DownloadWorker.cpp: everything to do with the small
// per-download "session manifest" file written next to the output
// folder. It lets a paused/cancelled download be recognized on the
// next run (same URL/format/playlist), tracks which files belong to
// that session, and cleans up partial or intermediate artifacts
// without touching files from unrelated downloads.
//
// DownloadWorker.cpp owns actually running yt-dlp and reading its
// output; this module owns knowing what got downloaded and what to
// do about it if the session is cancelled or resumed.
namespace DownloadResumeManifest
{
    // Path to this session's manifest file, given the download's
    // destination folder.
    std::wstring BuildManifestPath(
        const std::wstring& downloadsFolder);

    struct SessionInfo
    {
        bool matches = false;
        FILETIME startTime{};
    };

    // Checks whether an existing manifest at manifestPath belongs to
    // this exact download (same URL, format, and playlist choice) -
    // used to decide whether a paused download can resume vs. needs
    // to start a fresh session.
    SessionInfo GetSessionInfo(
        const std::wstring& manifestPath,
        const std::wstring& url,
        bool isMp3,
        bool isPlaylist);

    // Loads the list of files this manifest has recorded so far.
    void LoadManifestFiles(
        const std::wstring& manifestPath,
        std::vector<std::wstring>& files);

    // Starts a brand new session manifest, overwriting any existing
    // one at that path.
    bool CreateNewManifest(
        const std::wstring& manifestPath,
        const std::wstring& url,
        bool isMp3,
        bool isPlaylist,
        const FILETIME& startTime);

    // Records a destination file - and its known in-progress sibling
    // extensions (.part/.ytdl/.temp) - into the manifest, so they can
    // all be cleaned up later if the download is cancelled.
    void RecordKnownArtifacts(
        const std::wstring& manifestPath,
        const std::wstring& destination,
        std::vector<std::wstring>& files);

    // MP3 downloads go through an intermediate .webm before audio
    // extraction; deletes those once the final MP3 is confirmed
    // complete.
    void CleanupCompletedMp3IntermediateFiles(
        const std::vector<std::wstring>& trackedFiles,
        const std::vector<std::wstring>& trackedDestinations,
        const std::wstring& downloadsFolder,
        const FILETIME& sessionStartTime);

    // Extra safety pass: catches any .webm/.webm.part sharing the
    // final file's stem that the pass above missed, bounded by the
    // session's start time so it can't touch unrelated files.
    void CleanupLeftoverWebmFiles(
        const std::wstring& downloadsFolder,
        const std::wstring& finalFilePath,
        const FILETIME& sessionStartTime);

    // Called when a download is cancelled: deletes every file the
    // manifest tracked (merging in-memory records with whatever was
    // persisted to disk), does a final folder scan for any partial
    // artifacts from this session that weren't explicitly tracked,
    // and removes the manifest file itself.
    void CleanupCancelledSession(
        const std::wstring& manifestPath,
        const std::vector<std::wstring>& trackedFiles,
        const std::vector<std::wstring>& trackedDestinations,
        const std::wstring& downloadsFolder,
        const FILETIME& sessionStartTime);
}
