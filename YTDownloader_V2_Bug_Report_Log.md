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
