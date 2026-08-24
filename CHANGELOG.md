# Changelog

## [Stable Core Functionality] — August 22, 2026

### Fixed

* Fixed the **Pause** functionality so that the active `yt-dlp` process and its child processes can be stopped together.
* Fixed **Resume** so that a paused download can continue using yt-dlp's `--continue` behavior.
* Fixed the **Cancel** functionality so that cancelling a download no longer produces a misleading `yt-dlp exited with error code 1` error message.
* Added proper distinction between **Paused** and **Cancelled** download states.
* Fixed cancellation of downloads involving `ffmpeg` by controlling the yt-dlp process tree through a Windows Job Object.
* Fixed cancellation cleanup so that temporary `.part`, `.ytdl`, and related temporary files belonging to the cancelled download are removed.
* Added additional cleanup handling for playlist downloads, where multiple partial files may exist.
* Ensured that paused downloads retain their partial files so they can be resumed.
* Ensured that completed downloads are not removed by cancellation cleanup.

### Core Functionality Verified

The current implementation has been tested successfully for:

* Single video downloads
* MP4 downloads
* MP3 downloads
* Playlist downloads
* Pause
* Resume
* Cancel
* Partial-file preservation during Pause
* Partial-file cleanup during Cancel
* yt-dlp/FFmpeg process termination
* Cancellation without an erroneous error-code popup

### Development Note

This version is being treated as a **stable core-functionality checkpoint**.

UI improvements are intentionally being deferred. The next development step will be a code-cleanup/refactoring pass, particularly on `DownloadManager.cpp`, before additional functionality is introduced.




## [Stable Core Functionality] — August 24, 2026



# YT Downloader V2 — Changelog

This changelog is cumulative. Previous project history is retained and newer work is appended.

---

## V1.0 — CMD Version

- Initial command-line YouTube downloader.
- Integrated `yt-dlp` for media downloading.
- Used `ffmpeg` for media processing where required.
- Established the original download workflow.

## V1.1 — PowerShell GUI

- Migrated the downloader workflow to a Windows GUI.
- Added MP3/MP4 format selection.
- Added playlist detection and playlist-selection behavior.
- Added custom download-folder selection.
- Added progress reporting and completion handling.
- Added browser-extension integration.
- Added completion controls such as Open, Open With, and Open Folder.
- Fixed multiple pause, progress, completion-dialog, button, and browser-bridge issues during testing.
- V1.1 was treated as the completed predecessor before the native C++ V2 rewrite.

---

# V2 — Native C++ / Win32 Rewrite

## Early V2 Development

- Rebuilt the downloader as a native C++ Windows application using Win32 APIs.
- Moved away from the PowerShell/PS2EXE implementation.
- Established a dedicated `DownloadManager`.
- Added native process launching using `CreateProcessW`.
- Added background worker processing so downloads do not block the GUI.
- Added worker-to-GUI communication through Windows messages.
- Added command-line URL support for future browser-extension integration.
- Added native GUI controls for URL input, format selection, progress, status, and download controls.

## DownloadManager Refactoring

### Process handling

- Added explicit tracking of the active `yt-dlp` process.
- Added a Windows Job Object so `yt-dlp` and child processes such as `ffmpeg` can be terminated together.
- Added separate handling for process-start failures versus normal `yt-dlp` exit codes.
- Added `PRE_LAUNCH_FAILURE_CODE` for failures occurring before `yt-dlp` successfully starts.

### Progress handling

- Added parsing of `yt-dlp` progress output.
- Added validation so progress remains within 0–100%.
- Added handling for download destination messages.
- Added handling for audio extraction and video/audio merging.
- Added defensive file detection for completed downloads.

### Download state

- Added explicit download-running state.
- Added separate pause and cancellation requests.
- The UI changes from Pause to Resume only after the worker confirms the paused state.
- Cancellation is treated separately from ordinary download failure.
- Pause preserves partial `.part` files so `yt-dlp` can continue the download.
- Cancel removes partial files associated with the cancelled operation.

---

# Playlist Support

- Added support for individual video URLs.
- Added support for pure playlist URLs.
- Added handling for URLs containing both video and playlist identifiers.
- Added a playlist-choice workflow.
- Added MP3 playlist support.
- Added MP4 playlist support.

## Playlist Resume

Functional testing identified an edge case where resuming a playlist-related operation can cause `yt-dlp` to ask again whether to download the individual video or the entire playlist.

The underlying download can continue after the appropriate choice, so this is tracked separately from the core pause/resume functionality.

---

# Functional Testing Milestone — August 2026

The refactored DownloadManager reached a strong functional state.

Successfully tested:

- Single MP3 downloads.
- Single MP4 downloads.
- MP3 playlist downloads.
- MP4 playlist downloads.
- Pause.
- Resume.
- Cancel.
- Individual video URLs.
- Playlist URLs.
- Process-tree termination.
- `.part` handling.
- Progress reporting.

The core download workflow is working well. Remaining problems are being tracked separately to avoid disturbing working functionality.

---

# Current Bug Work

## BUG-001 — Fast Cancellation Temporary-File Cleanup

**Status:** Open

During cancellation of a very short MP3 download, the expected `.part` file is removed successfully, but some intermediate yt-dlp files can remain.

The suspected cause is a timing/race condition: yt-dlp can create and process WebM/WebP/thumbnail/intermediate files extremely quickly while cancellation cleanup is running.

Planned investigation:

- Perform cleanup after the process tree has definitely terminated.
- Consider a second directory scan.
- Account for yt-dlp intermediate WebM/WebP/thumbnail artifacts.
- Avoid deleting files belonging to unrelated downloads.

The fix must preserve existing download, pause, resume, cancel, playlist, and process-tree behavior.

## BUG-002 — Playlist Resume Selection Prompt

**Status:** Open

When resuming a playlist-related download, the playlist/video selection prompt can appear again.

The download continues after the appropriate choice, but Resume should ideally continue the already-selected operation without asking again.

The likely area for investigation is preserving the selected playlist mode as part of the active download state.

---

# Code Quality / Debugging History

- Fixed a Windows `MessageBoxW` string-type compile issue by constructing a `std::wstring` and passing `message.c_str()`.
- Added defensive checks around process creation and process-group setup.
- Added temporary-file filtering during completion-file detection.
- Added safer handling for invalid or unparseable progress values.
- Kept unrelated fixes separated where practical to make testing and commits easier to reason about.

---

# Current Development Direction

The project follows:

**Observe → Reproduce → Isolate → Fix → Retest → Regression-check → Record**

The goal is to preserve working functionality while resolving bugs independently and keeping the development history auditable.