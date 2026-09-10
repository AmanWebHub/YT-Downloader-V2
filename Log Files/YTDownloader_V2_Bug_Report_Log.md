# YT Downloader V2 — Bug Report Log

## Purpose

This document records bugs discovered during functional testing of the current refactored DownloadManager implementation.

The current testing indicates that the core download functionality is working well. The issues recorded here are bugs observed during testing and are not currently treated as failures of the overall download workflow.

---

## BUG-001 — Fast Download Cancellation Leaves Temporary yt-dlp Files

**Status:** Open  
**Severity:** Medium  
**Priority:** Medium  
**Area:** Download cancellation / temporary-file cleanup  
**Component:** DownloadManager / cancellation cleanup  
**Discovered during:** Functional testing after DownloadManager refactor

### Description

When cancelling a download, the expected `.part` files are successfully removed.

However, when a download is extremely short and completes its intermediate yt-dlp processing very quickly, some temporary files created during the download process can remain after cancellation.

The issue was observed with a small MP3 download that completed in less than approximately three seconds.

### Observed yt-dlp workflow

During the download process, yt-dlp may create intermediate files such as:

1. Thumbnail / WebP-related files
2. WebM media files
3. The final converted/combined output

For a very short download, these intermediate files can be created and processed so quickly that the cancellation cleanup does not remove every temporary artifact.

### Reproduction

1. Start an MP3 download for a very short/small video.
2. Wait for yt-dlp to begin downloading/processing the media.
3. Press **Cancel** very quickly, before the entire operation finishes.
4. Inspect the download directory.

### Expected result

Cancellation should remove all temporary files created for the cancelled download, including:

- `.part`
- `.ytdl`
- `.temp`
- intermediate WebM files
- temporary WebP/thumbnail-related files, where applicable

No temporary artifacts belonging to the cancelled download should remain.

### Actual result

The `.part` file is successfully removed.

However, some intermediate files can remain when cancellation happens during a very fast download.

### Impact

This does **not** currently prevent normal downloading, pausing, resuming, or cancellation from functioning.

The main impact is leftover temporary files in the download directory.

### Initial assessment

This appears to be a **race/timing issue in cancellation cleanup**.

The cleanup can run before yt-dlp has finished creating or renaming all of its intermediate files. Because the MP3 download was extremely short, the intermediate files were created and processed within a very small time window.

### Suggested investigation

Do not modify the working Pause/Resume implementation yet.

Investigate whether cancellation cleanup should:

- perform a short delayed cleanup pass after the process tree has definitely terminated;
- perform a second directory scan after termination;
- track all files created during the current download;
- account for yt-dlp's intermediate WebM/WebP/thumbnail files;
- avoid deleting files belonging to an unrelated download.

### Regression requirements

Any fix must continue to preserve the currently working behavior:

- Single MP4 download
- Single MP3 download
- MP4 playlist download
- MP3 playlist download
- Pause
- Resume
- Cancel
- `.part` cleanup
- Process-tree termination

---

## Testing Notes

At the time this bug was recorded, functional testing showed that the core DownloadManager functionality was working correctly.

Tested successfully:

- MP3 single-item downloads
- MP3 playlist downloads
- MP4 single-item downloads
- MP4 playlist downloads
- Pause
- Resume
- Cancel
- Single individual links
- Playlist links

No functionality-breaking errors were found during this testing pass.

Some UI-related bugs are known, but UI issues are intentionally outside the scope of the current core-functionality/refactoring work.

---

## Development Policy for This Log

Bugs should be recorded here when discovered during testing, even if the core functionality otherwise works correctly.

A bug report should document:

- What happened
- How it was reproduced
- What was expected
- What actually happened
- Severity/priority
- Suspected cause, when known
- Suggested investigation
- Regression requirements

Fixes should be made separately from unrelated refactoring whenever practical, so each change can be tested and committed independently.


## [Stable Core Functionality] — August 24, 2026

## --------------------------------------

# YT Downloader V2 — Bug Report Log

This document is cumulative. Previous issues are retained and new issues are added without deleting the historical record.

---

# BUG-001 — Fast Download Cancellation Leaves Temporary yt-dlp Files

**Status:** Open  
**Severity:** Medium  
**Priority:** Medium  
**Area:** Download cancellation / temporary-file cleanup  
**Component:** DownloadManager / cancellation cleanup

## Description

When cancelling a download, the expected `.part` files are successfully removed.

However, when a download is extremely short and yt-dlp processes its intermediate files very quickly, some temporary files can remain after cancellation.

The issue was observed with a small MP3 download that completed in less than approximately three seconds.

## Observed workflow

yt-dlp may create intermediate files such as:

1. Thumbnail / WebP-related files.
2. WebM media files.
3. The final converted or combined output.

For a very short download, these files can be created and processed within a very small time window.

## Reproduction

1. Start an MP3 download for a very short/small video.
2. Allow yt-dlp to begin downloading/processing.
3. Press Cancel very quickly.
4. Inspect the download directory.

## Expected result

Cancellation should remove temporary files created for the cancelled download, including applicable:

- `.part`
- `.ytdl`
- `.temp`
- intermediate WebM files
- temporary WebP/thumbnail-related files

## Actual result

The `.part` file is removed successfully.

Some intermediate files can remain when cancellation happens during a very fast download.

## Impact

This does not currently prevent normal downloading, pausing, resuming, or cancellation.

The main impact is leftover temporary files in the download directory.

## Initial assessment

This appears to be a timing/race issue in cancellation cleanup.

Cleanup may run before yt-dlp has finished creating or renaming every intermediate file.

## Suggested investigation

- Perform a cleanup pass after the process tree has definitely terminated.
- Consider a second directory scan.
- Track files created by the current operation where practical.
- Account for yt-dlp intermediate WebM/WebP/thumbnail files.
- Avoid deleting files belonging to unrelated downloads.

## Regression requirements

Any fix must preserve:

- Single MP4 download.
- Single MP3 download.
- MP4 playlist download.
- MP3 playlist download.
- Pause.
- Resume.
- Cancel.
- `.part` cleanup.
- Process-tree termination.

---

# BUG-002 — Playlist Resume Can Re-trigger Playlist/Video Selection

**Status:** Open  
**Severity:** Low / Medium  
**Priority:** Medium  
**Area:** Playlist resume workflow  
**Component:** MainWindow / DownloadManager / playlist-selection state

## Description

Playlist downloads function correctly during normal operation.

However, when a playlist-related download is paused and then resumed, yt-dlp can cause the application to ask again whether the user wants to download the individual video or the entire playlist.

The user can choose the appropriate option and the download continues.

## Expected result

Once the user selects:

- Single video, or
- Entire playlist,

that selection should remain associated with the paused/resumed operation.

Resume should continue the same operation without requiring the selection again.

## Actual result

The playlist/video selection prompt can appear again on resume.

## Impact

- Does not prevent the download from completing.
- Creates unnecessary user interaction.
- Can be confusing because Resume should continue the existing operation.
- Particularly undesirable for playlist downloads.

## Initial assessment

The resumed operation may be starting as a fresh `StartDownload()` request and re-evaluating playlist context from the original URL.

The application remembers enough state to resume the partial transfer, but the original playlist-selection decision is not yet persistent operation state.

## Suggested investigation

Consider preserving the selected playlist mode as part of the active download state:

- Store the selected playlist/single-video mode.
- Resume using that established state.
- Do not reopen the playlist picker for an already-selected paused operation.
- Keep the playlist picker for brand-new URLs.

## Regression requirements

After a fix, retest:

- Single video download.
- Single video pause/resume.
- Pure playlist download.
- Playlist pause/resume.
- MP3 playlist pause/resume.
- MP4 playlist pause/resume.
- Cancel after resume.
- New playlist download after a previous paused/cancelled operation.

---

# Historical V1/V2 Defect Categories

Earlier project testing identified and resolved issues in areas including:

- Pause-button state.
- Progress-bar behavior.
- Completion-dialog behavior.
- Open/Open With/Open Folder controls.
- Browser-extension integration and reload behavior.
- Playlist URL handling.
- Windows API string conversion during C++ compilation.

These historical issues remain part of the project's development history even though they are no longer open.

---

# Current Bug Summary

| ID | Issue | Severity | Status |
|---|---|---|---|
| BUG-001 | Fast cancellation can leave intermediate yt-dlp files | Medium | Open |
| BUG-002 | Playlist resume can re-trigger playlist/video selection | Low/Medium | Open |

---

# Bug Handling Policy

For each bug:

1. Record the observed behavior.
2. Record exact reproduction steps.
3. Define expected behavior.
4. Define actual behavior.
5. Assess impact.
6. Identify the likely cause when supported by evidence.
7. Implement a targeted fix.
8. Retest the original reproduction.
9. Run regression tests against related functionality.
10. Update the changelog and test log after the fix is verified.

Fixes should be separated from unrelated refactoring whenever practical so individual changes can be tested and committed independently.