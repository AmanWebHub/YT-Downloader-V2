# YTDownloader V2 — Changelog

**Date:** 2026-09-13
**Branch:** `clean-up`

---

## Overview

This update focuses primarily on code cleanup, function separation, maintainability, and regression safety.

The existing YTDownloader V2 functionality was tested after the internal restructuring and remains operational.

---

## Added / Separated Components

### DownloadResumeManifest

Resume/session manifest functionality was separated into its own component to reduce responsibility inside the download worker and improve maintainability.

### DownloadUtils

Utility functionality was expanded/separated into `DownloadUtils` to keep general-purpose operations out of the core download workflow.

### DownloadWorker Cleanup

`DownloadWorker` was reorganized and cleaned up by separating functionality into more focused components/functions.

The goal is to make the worker easier to understand, maintain, debug, and extend without changing the established download behavior.

---

## Browse / Download Location Behavior

The Browse/download-location behavior has been updated.

### Previous Behavior

Selecting a custom download location could cause that location to remain as the application's default location.

### Current Behavior

A custom location selected through Browse is now treated as **session-only**.

* The selected location is used during the current YTDownloader session.
* Closing YTDownloader ends the custom location selection.
* Starting a new session returns the application to the default download location.
* The previous custom location is not automatically retained.

This behavior was tested and confirmed working.

**Status: FIXED / PASS**

---

## Existing Functionality Preserved

The following functionality was regression-tested after the cleanup:

* Single MP3 downloads
* Single MP4 downloads
* Playlist MP3 downloads
* Playlist MP4 downloads
* Playlist selection
* Single-video-from-playlist downloads
* Pause/resume
* Download cancellation
* Browser extension integration
* Direct EXE downloads
* Completion windows
* Open/Open With/Open Folder
* Browse/download-location selection
* Temporary file cleanup
* Special/non-ASCII filenames
* Session manifest handling

All tested functionality passed.

---

## Browser Extension

The browser extension remains integrated with YTDownloader V2 and continues to support the established browser → `ytdlp:` protocol → application workflow.

No regression was observed during testing.

---

## Individual YouTube Short Failure

A specific YouTube Short was unable to download.

Testing confirmed that the same video failed both:

* As an independent download
* When downloaded through a playlist

Because the failure is reproducible independently of playlist handling, it is **not currently classified as a confirmed YTDownloader bug**.

This may be related to the individual video's availability, YouTube behavior, or yt-dlp handling.

---

## Testing

A comprehensive manual regression test was completed on the `clean-up` branch.

**Overall test result: PASS**

See:

`TEST_LOG_2026-09-13.md`

for the detailed test results.

---

## Development Status

YTDownloader V2 is currently in a stable development state.

The core download and playlist functionality has been validated, and the cleanup branch has not introduced any observed functional regression.

### Current Priorities

1. Targeted bug investigation
2. UI/UX polish
3. Additional edge-case testing
4. Release-build preparation
5. yt-dlp/FFmpeg packaging verification
6. Browser extension/protocol verification
7. Repository cleanup
8. Standalone application testing

---

## Summary

This update improves the internal organization of YTDownloader V2 while preserving the existing functionality.

The Browse location behavior was also corrected so custom download locations are session-specific rather than becoming persistent defaults.

**Current status: Stable / Tested / PASS**
