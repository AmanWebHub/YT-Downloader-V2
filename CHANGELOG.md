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
