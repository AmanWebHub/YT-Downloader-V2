# Test Log

## August 22, 2026 — Core Download Control Testing

### Test Environment

* Project: **IT Downloader V2**
* Component tested: `DownloadManager`
* Downloader engine: `yt-dlp`
* Process management: Windows Job Object
* Test focus: Core download control functionality
* UI changes: Not part of this test cycle

### Results

| Test                      | Expected Result                                      | Result |
| ------------------------- | ---------------------------------------------------- | ------ |
| MP4 single-video download | Download completes successfully                      | PASS   |
| MP3 single-video download | Audio download/conversion completes successfully     | PASS   |
| Playlist download         | Playlist items download successfully                 | PASS   |
| Pause                     | yt-dlp process stops and `.part` file remains        | PASS   |
| Resume                    | Paused download continues from existing partial file | PASS   |
| Cancel                    | yt-dlp/FFmpeg processes terminate                    | PASS   |
| Cancel cleanup            | Cancelled download's temporary files are removed     | PASS   |
| Cancel error handling     | No misleading error-code `1` popup is displayed      | PASS   |
| Pause state               | Download remains resumable                           | PASS   |
| Playlist cancellation     | Partial playlist files are cleaned up                | PASS   |

### Pause/Resume Verification

A download was paused during an active transfer.

**Observed:**

* yt-dlp stopped.
* The partial `.part` file remained.
* The application entered the paused state.
* Resume restarted the download using the existing partial file.
* The download continued successfully.

**Result: PASS**

### Cancel Verification

A separate download was cancelled during an active transfer.

**Observed:**

* yt-dlp and its child process were terminated.
* The partial `.part` file was removed.
* The application reported `Download cancelled.`
* No erroneous yt-dlp error-code popup appeared.
* The application returned to its normal ready state.

**Result: PASS**

### Playlist Verification

Playlist downloads were tested with the updated process-control and cleanup implementation.

**Observed:**

* Playlist downloads completed successfully.
* Multiple partial files can be handled during cancellation.
* Cancellation cleanup does not intentionally remove completed files.

**Result: PASS**

### Current Status

**STABLE CORE FUNCTIONALITY**

The tested download-control functionality is considered stable enough to commit to the repository.

The next planned work is **code cleanup/refactoring**, with particular attention to `DownloadManager.cpp`. No additional feature development should be performed until the refactoring pass has been completed and retested.


## [Stable Core Functionality] — August 24, 2026

# YT Downloader V2 — Test Log

This is the cumulative functional and regression test record. Previous testing information is retained.

---

# 1. Test Scope

Testing covers:

- Application startup.
- URL handling.
- MP4 downloads.
- MP3 downloads.
- Single-video downloads.
- Playlist downloads.
- Playlist/video URL ambiguity.
- Progress reporting.
- Pause.
- Resume.
- Cancel.
- Temporary-file handling.
- Process and child-process termination.
- Completion handling.
- Error handling.
- GUI state synchronization.
- Regression testing after DownloadManager changes.

---

# 2. Historical V1 Testing

V1 testing included:

- Pause-button behavior.
- Progress-bar behavior.
- Download completion dialog.
- Open/Open With/Open Folder controls.
- Browser-extension integration.
- Browser-bridge reload behavior.
- Playlist handling.
- General download workflow.

Testing followed an iterative reproduce → fix → retest process.

---

# 3. V2 Test Environment

**Application:** YT Downloader V2  
**Platform:** Windows  
**Implementation:** Native C++ / Win32  
**Download engine:** yt-dlp  
**Media processing:** ffmpeg where required

---

# 4. Core Functional Test Results

| Test ID | Test | Result | Notes |
|---|---|---|---|
| FT-001 | Start application | PASS | Application starts and accepts the normal workflow. |
| FT-002 | Single MP4 download | PASS | Successfully tested. |
| FT-003 | Single MP3 download | PASS | Successfully tested. |
| FT-004 | MP4 playlist download | PASS | Successfully tested. |
| FT-005 | MP3 playlist download | PASS | Successfully tested. |
| FT-006 | Pause download | PASS | Download pauses and the paused state is confirmed. |
| FT-007 | Resume download | PASS | Partial download continues using yt-dlp resume behavior. |
| FT-008 | Cancel download | PASS | Download terminates and cancellation is reported separately from failure. |
| FT-009 | Individual video URL | PASS | Successfully tested. |
| FT-010 | Playlist URL | PASS | Successfully tested. |
| FT-011 | Progress reporting | PASS | Progress is parsed and limited to 0–100%. |
| FT-012 | yt-dlp process termination | PASS | Job Object handling terminates yt-dlp and child processes such as ffmpeg. |
| FT-013 | `.part` handling during pause | PASS | Partial file is preserved for resume. |
| FT-014 | `.part` cleanup during cancellation | PASS | Expected `.part` cleanup works in normal cancellation cases. |

---

# 5. Playlist Workflow Tests

## Pure playlist URL

**Result:** PASS

A playlist URL is handled as a playlist download.

## Video + playlist URL

**Result:** PASS

A URL containing both a video and playlist identifier is treated as an ambiguous case and the user can choose the intended mode.

## Playlist MP3

**Result:** PASS

Multiple playlist items can be downloaded and converted to MP3.

## Playlist MP4

**Result:** PASS

Multiple playlist items can be downloaded as video.

## Playlist resume edge case

**Result:** BUG FOUND / REQUIRES FIX

When resuming a playlist-related download, yt-dlp can prompt again for whether the operation should apply to the individual video or the entire playlist.

The download can continue after the appropriate choice, but the repeated prompt is undesirable.

---

# 6. Pause / Resume Tests

## Pause

**Result:** PASS

The Pause control requests a pause and the worker confirms the paused state before the UI changes to Resume.

## Resume

**Result:** PASS

The resumed operation continues from the existing partial download rather than intentionally restarting the entire transfer.

## Pause regression

**Result:** PASS

Testing after the DownloadManager refactor confirmed that pause/resume remained functional.

---

# 7. Cancellation Tests

## Normal cancellation

**Result:** PASS

Cancellation terminates the active download process and its child processes through the Job Object.

## `.part` cleanup

**Result:** PASS in normal cases

The expected partial `.part` file is removed during cancellation.

## Very fast cancellation

**Result:** BUG FOUND

For an extremely short MP3 download, cancellation can leave some intermediate yt-dlp artifacts even though the `.part` file is removed.

Observed possible artifacts include:

- WebM media files.
- WebP/thumbnail-related files.
- Other short-lived intermediate processing files.

This is recorded as BUG-001.

---

# 8. Progress and Output Parsing

The DownloadManager parses yt-dlp output for:

- Percentage progress.
- Download destination.
- Audio extraction destination.
- Video/audio merging.
- Error messages.

Progress values outside 0–100% are rejected.

**Result:** PASS during current functional testing.

---

# 9. Process Handling

The refactored DownloadManager uses a Windows Job Object so the active yt-dlp process and child processes such as ffmpeg can be controlled together.

Tested behavior:

- Start yt-dlp.
- Track active process.
- Assign process to Job Object.
- Terminate process tree during cancellation.
- Terminate process tree during pause.
- Clean up process and Job Object handles.

**Result:** PASS during current functional testing.

---

# 10. Regression Requirements

After any DownloadManager change, retest:

- Single MP4.
- Single MP3.
- MP4 playlist.
- MP3 playlist.
- Pause.
- Resume.
- Cancel.
- `.part` handling.
- Progress reporting.
- Process-tree termination.
- Playlist/video URL selection.

A fix is not considered safe if it resolves one scenario while breaking another previously working scenario.

---

# 11. Current Test Status — 24 August 2026

### Working

- Core MP4 download.
- Core MP3 download.
- MP4 playlists.
- MP3 playlists.
- Pause.
- Resume.
- Cancel.
- Progress reporting.
- Process-tree termination.
- Normal `.part` cleanup.

### Known issues

- **BUG-001:** Very fast cancellation can leave intermediate yt-dlp files.
- **BUG-002:** Playlist resume can show the playlist/video selection again.

---

# 12. Testing Method

The project follows:

1. Observe the user-visible behavior.
2. Reproduce the problem.
3. Identify the affected component.
4. Make the smallest appropriate change.
5. Rebuild.
6. Retest the original failure.
7. Run regression tests against related functionality.
8. Record the result.

This log is intended to provide an auditable development/testing history. 

# Functional Testing Milestone — August 2026

# YT Downloader V2 — Test Log

This is the cumulative functional and regression test record. Previous testing information is retained.

---

# 1. Test Scope

Testing covers:

- Application startup.
- URL handling.
- MP4 downloads.
- MP3 downloads.
- Single-video downloads.
- Playlist downloads.
- Playlist/video URL ambiguity.
- Progress reporting.
- Pause.
- Resume.
- Cancel.
- Temporary-file handling.
- Process and child-process termination.
- Completion handling.
- Error handling.
- GUI state synchronization.
- Regression testing after DownloadManager changes.

---

# 2. Historical V1 Testing

V1 testing included:

- Pause-button behavior.
- Progress-bar behavior.
- Download completion dialog.
- Open/Open With/Open Folder controls.
- Browser-extension integration.
- Browser-bridge reload behavior.
- Playlist handling.
- General download workflow.

Testing followed an iterative reproduce → fix → retest process.

---

# 3. V2 Test Environment

**Application:** YT Downloader V2  
**Platform:** Windows  
**Implementation:** Native C++ / Win32  
**Download engine:** yt-dlp  
**Media processing:** ffmpeg where required

---

# 4. Core Functional Test Results

| Test ID | Test | Result | Notes |
|---|---|---|---|
| FT-001 | Start application | PASS | Application starts and accepts the normal workflow. |
| FT-002 | Single MP4 download | PASS | Successfully tested. |
| FT-003 | Single MP3 download | PASS | Successfully tested. |
| FT-004 | MP4 playlist download | PASS | Successfully tested. |
| FT-005 | MP3 playlist download | PASS | Successfully tested. |
| FT-006 | Pause download | PASS | Download pauses and the paused state is confirmed. |
| FT-007 | Resume download | PASS | Partial download continues using yt-dlp resume behavior. |
| FT-008 | Cancel download | PASS | Download terminates and cancellation is reported separately from failure. |
| FT-009 | Individual video URL | PASS | Successfully tested. |
| FT-010 | Playlist URL | PASS | Successfully tested. |
| FT-011 | Progress reporting | PASS | Progress is parsed and limited to 0–100%. |
| FT-012 | yt-dlp process termination | PASS | Job Object handling terminates yt-dlp and child processes such as ffmpeg. |
| FT-013 | `.part` handling during pause | PASS | Partial file is preserved for resume. |
| FT-014 | `.part` cleanup during cancellation | PASS | Expected `.part` cleanup works in normal cancellation cases. |

---

# 5. Playlist Workflow Tests

## Pure playlist URL

**Result:** PASS

A playlist URL is handled as a playlist download.

## Video + playlist URL

**Result:** PASS

A URL containing both a video and playlist identifier is treated as an ambiguous case and the user can choose the intended mode.

## Playlist MP3

**Result:** PASS

Multiple playlist items can be downloaded and converted to MP3.

## Playlist MP4

**Result:** PASS

Multiple playlist items can be downloaded as video.

## Playlist resume edge case

**Result:** BUG FOUND / REQUIRES FIX

When resuming a playlist-related download, yt-dlp can prompt again for whether the operation should apply to the individual video or the entire playlist.

The download can continue after the appropriate choice, but the repeated prompt is undesirable.

---

# 6. Pause / Resume Tests

## Pause

**Result:** PASS

The Pause control requests a pause and the worker confirms the paused state before the UI changes to Resume.

## Resume

**Result:** PASS

The resumed operation continues from the existing partial download rather than intentionally restarting the entire transfer.

## Pause regression

**Result:** PASS

Testing after the DownloadManager refactor confirmed that pause/resume remained functional.

---

# 7. Cancellation Tests

## Normal cancellation

**Result:** PASS

Cancellation terminates the active download process and its child processes through the Job Object.

## `.part` cleanup

**Result:** PASS in normal cases

The expected partial `.part` file is removed during cancellation.

## Very fast cancellation

**Result:** BUG FOUND

For an extremely short MP3 download, cancellation can leave some intermediate yt-dlp artifacts even though the `.part` file is removed.

Observed possible artifacts include:

- WebM media files.
- WebP/thumbnail-related files.
- Other short-lived intermediate processing files.

This is recorded as BUG-001.

---

# 8. Progress and Output Parsing

The DownloadManager parses yt-dlp output for:

- Percentage progress.
- Download destination.
- Audio extraction destination.
- Video/audio merging.
- Error messages.

Progress values outside 0–100% are rejected.

**Result:** PASS during current functional testing.

---

# 9. Process Handling

The refactored DownloadManager uses a Windows Job Object so the active yt-dlp process and child processes such as ffmpeg can be controlled together.

Tested behavior:

- Start yt-dlp.
- Track active process.
- Assign process to Job Object.
- Terminate process tree during cancellation.
- Terminate process tree during pause.
- Clean up process and Job Object handles.

**Result:** PASS during current functional testing.

---

# 10. Regression Requirements

After any DownloadManager change, retest:

- Single MP4.
- Single MP3.
- MP4 playlist.
- MP3 playlist.
- Pause.
- Resume.
- Cancel.
- `.part` handling.
- Progress reporting.
- Process-tree termination.
- Playlist/video URL selection.

A fix is not considered safe if it resolves one scenario while breaking another previously working scenario.

---

# 11. Current Test Status — 24 August 2026

### Working

- Core MP4 download.
- Core MP3 download.
- MP4 playlists.
- MP3 playlists.
- Pause.
- Resume.
- Cancel.
- Progress reporting.
- Process-tree termination.
- Normal `.part` cleanup.

### Known issues

- **BUG-001:** Very fast cancellation can leave intermediate yt-dlp files.
- **BUG-002:** Playlist resume can show the playlist/video selection again.

---

# 12. Testing Method

The project follows:

1. Observe the user-visible behavior.
2. Reproduce the problem.
3. Identify the affected component.
4. Make the smallest appropriate change.
5. Rebuild.
6. Retest the original failure.
7. Run regression tests against related functionality.
8. Record the result.

This log is intended to provide an auditable development/testing history. 

