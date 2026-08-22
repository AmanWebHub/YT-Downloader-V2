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
