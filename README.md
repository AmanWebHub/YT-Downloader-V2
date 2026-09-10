# YTDownloader V2

> Windows desktop YouTube/media downloader built around yt-dlp and FFmpeg.

**Project Status:** Core functionality stable / UI refinement phase  
**Documentation Date:** September 4, 2026

## Current Status

The core downloader functionality has been completed and tested successfully.

### Supported / Tested Functionality

- Single MP3 downloads
- Single MP4/video downloads
- Single-video selection from playlists
- Entire playlist downloads
- MP3 playlists
- MP4/video playlists
- Pause and resume
- Playlist resume
- Immediate cancellation
- Delayed/progressed cancellation
- Completion window
- Open downloaded file
- Open With
- Open Folder
- Special-character and non-ASCII titles
- Post-processing output detection
- Temporary/session-file handling

## Major Stabilization Fixes

### Reliable Output Detection

YTDownloader V2 now prioritizes the `__ITD_FILE__` marker for identifying the final output file and uses a filesystem-based `FindNewestFileSince()` fallback when necessary.

This is more reliable than attempting to determine the final filename from ordinary yt-dlp console output, particularly when merging or extracting media.

### UTF-8 Handling

The yt-dlp child process is configured with:

- `PYTHONUTF8=1`
- `PYTHONIOENCODING=utf-8`

This improves handling of titles containing non-ASCII characters.

### Download UI Rendering

The main window now uses `WS_CLIPCHILDREN` to prevent background repainting from interfering with child controls.

Progress-percentage rendering was also corrected to prevent old percentage values from being left behind.

### Playlist Folder Handling

Playlist completion now explicitly resolves to the destination folder for **Open Folder**, rather than relying on the last downloaded file.

### Progress Update Optimization

Progress messages are de-duplicated so unchanged percentage values do not unnecessarily flood the UI message queue.

## Testing

The completed test cycle resulted in a **PASS** for the core functionality.

Testing covered:

- MP3 and MP4 downloads
- Playlists
- Pause/resume
- Cancellation
- Playlist resume
- Completion-window actions
- Special-character filenames
- Post-processing
- Temporary-file handling
- Download UI rendering

See [`TEST_LOG.md`](TEST_LOG.md) for the detailed test record.

See [`CHANGELOG.md`](CHANGELOG.md) for the detailed change history.

## Next Development Phase

The next planned improvement is UI/UX refinement of playlist detection.

### Planned Playlist Choice Interface

When a playlist URL is entered, the application will present a dedicated interface with:

1. **This Video** — download only the selected video.
2. **Entire Playlist** — download all videos in the playlist.
3. **Cancel** — return without starting the download.

The goal is to replace the current basic prompt with a polished custom interface consistent with the YTDownloader V2 design.

## Project Direction

The project is currently moving from **core functionality development** into **UI refinement and polish**.

The download engine should remain stable while UI improvements are developed and tested independently.
