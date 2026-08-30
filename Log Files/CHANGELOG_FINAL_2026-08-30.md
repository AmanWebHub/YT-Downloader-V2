# YT Downloader V2 — Changelog

This changelog is cumulative. Previous project history is retained.

---

## Final Core Stability Milestone — August 30, 2026

### Fixed / Verified

* Verified single MP3 downloads.
* Verified single MP4 downloads.
* Verified complete MP3 playlist downloads.
* Verified complete MP4 playlist downloads.
* Verified single-item selection from playlists for MP3 and MP4.
* Verified multiple-item selection from playlists for MP3 and MP4.
* Verified pause behavior during active downloads.
* Verified resume behavior after pausing.
* Verified immediate pause behavior.
* Verified delayed/post-delay pause behavior.
* Verified immediate cancellation behavior.
* Verified delayed/post-delay cancellation behavior.
* Verified cancellation cleanup.
* Verified MP3 intermediate WebM cleanup.
* Verified regression behavior after the latest cleanup and download-control fixes.

### BUG-001 — Fast Cancellation Temporary-File Cleanup

**Final status: FIXED / VERIFIED**

The previously identified fast-cancellation cleanup issue could leave intermediate yt-dlp artifacts during very short MP3 downloads.

The current implementation includes session manifest tracking, recent-artifact scanning, and dedicated WebM/WebM-part cleanup handling.

Final manual regression testing did not reproduce the previously observed leftover-artifact behavior.

### BUG-002 — Playlist Resume Selection Prompt

**Final status: FIXED / VERIFIED**

The previously identified playlist resume workflow issue was retested during the final regression cycle.

Playlist-related resume behavior worked according to the intended workflow during final testing.

---

## Final Core Functionality Status

The following functionality has been verified:

- Single MP3 download
- Single MP4 download
- Full MP3 playlist download
- Full MP4 playlist download
- Single playlist-item download
- Multiple playlist-item download
- Pause
- Resume
- Immediate pause
- Delayed pause
- Immediate cancellation
- Delayed cancellation
- Cancellation cleanup
- MP3 intermediate-file cleanup
- Playlist workflows
- Core regression scenarios

**Current status: STABLE CORE FUNCTIONALITY**

**Known core bugs reproduced during final testing: NONE**

**Merge status: READY FOR MAIN-BRANCH MERGE**

UI improvements and future enhancements remain separate from this core-stability milestone.
