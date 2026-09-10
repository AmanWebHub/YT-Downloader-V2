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

---

# Current Bug Status — September 10, 2026

The September 10 development changes and functional testing indicate that some
historical issues in this log are no longer considered open.

## BUG-001 — Fast Download Cancellation Leaves Temporary yt-dlp Files

**Historical Status:** Open  
**Current Status:** **Partially addressed / superseded by BUG-003**

The earlier BUG-001 reproduction concerned cancelling extremely quickly at the
beginning of a very short download, where `.part` and other intermediate files
could remain.

The current build has improved cancellation cleanup and the normal cancellation
tests pass, including `.part` cleanup and process-tree termination.

However, a different timing window has now been observed: cancellation at the
**absolute end of a download**, while yt-dlp/FFmpeg is finishing its final
processing and renaming steps.

That new end-of-download condition is recorded separately as **BUG-003** below.

---

## BUG-002 — Playlist Resume Can Re-trigger Playlist/Video Selection

**Historical Status:** Open  
**Current Status:** **Fixed / verified**

The current implementation now resumes using the previously selected URL,
format, and playlist parameters rather than treating Resume as a completely new
download request.

The September 10 changelog specifically records that the Pause/Resume button
reuses `StartDownloadWithParams()` with the last-used URL/format/playlist flags
on Resume.

Manual pause/resume testing also passed for playlist-related downloads.

### Regression status

- Single-video pause/resume — **PASS**
- Pure playlist pause/resume — **PASS**
- MP3 playlist pause/resume — **PASS**
- MP4 playlist pause/resume — **PASS**
- Resume using existing playlist state — **PASS**

No repeat playlist/video-selection prompt was observed during the current test
cycle.

---

# BUG-003 — Cancelling at the Absolute End of a Download Leaves Final/Intermediate Files

**Status:** **Open**  
**Severity:** Medium  
**Priority:** Medium  
**Area:** Download cancellation / finalization / cleanup  
**Component:** DownloadWorker / cancellation cleanup / post-processing  
**Discovered:** September 10, 2026 during manual functional testing

## Description

A new cancellation race has been identified when the user presses **Cancel at
the absolute end of a download**.

The download is already very close to completion and yt-dlp/FFmpeg may be in the
process of moving, converting, merging, or finalizing the output.

If cancellation occurs during this narrow timing window, the application can
report the operation as **Download Failed** and display the failure message,
while files belonging to the cancelled operation are left behind.

The observed leftovers can include:

- An intermediate **WebM** file
- A final **MP4** file for an MP4 download
- A final **MP3** file for an MP3 download

This is different from the earlier fast-cancellation case because the new
failure occurs specifically at the **end of the download/finalization window**.

## Reproduction

1. Start a normal MP4 or MP3 download.
2. Allow the download to progress almost completely to the end.
3. Wait until the operation is in its final download/post-processing stage.
4. Press **Cancel at the absolute end**, just before the application reports
   completion.
5. Observe the application status/message.
6. Inspect the destination folder.

### Expected result

If the user cancels the operation, the application should consistently treat
the operation as **Cancelled**, not **Failed**.

All files belonging to the cancelled operation should be cleaned up where they
are temporary or incomplete, including applicable:

- `.part`
- `.ytdl`
- `.temp`
- intermediate WebM files
- temporary WebP/thumbnail files
- incomplete/final output files created by the cancelled operation

A cancelled operation should not leave behind a usable-looking MP4/MP3 output
unless the application explicitly determines that the download completed before
the cancellation request took effect.

### Actual result

When cancellation occurs at the absolute end of the operation:

1. The application can report **Download Failed**.
2. A failure message can be displayed to the user.
3. A WebM intermediate file can remain.
4. An MP4 file can remain for MP4 downloads, or an MP3 file can remain for MP3
   downloads.
5. The resulting folder can therefore contain output artifacts even though the
   UI reports failure.

## Impact

**Medium**

The core download system remains functional, but the result is misleading and
can leave unwanted or incomplete files in the user's download directory.

The most important user-facing problem is the mismatch between:

**Actual filesystem state:** output files exist  
**Application state:** Download Failed

This can also make it unclear whether the remaining MP4/MP3 file is safe to
use or whether it is a partially processed artifact.

## Initial Assessment

This appears to be another **cancellation/finalization race condition**, but
it should not automatically be treated as identical to BUG-001.

At this point, the exact ordering between:

- the user's Cancel request,
- yt-dlp process termination,
- FFmpeg/post-processing,
- file rename/move operations,
- output-path detection,
- final exit-code handling, and
- cancellation cleanup

needs to be confirmed from the current code.

The timing is particularly important because the cancellation request occurs
when the download is already transitioning from the download phase into final
processing/completion.

## Suggested Investigation

Do not change the working Pause/Resume or playlist-selection behavior as part
of this fix.

Investigate the cancellation path specifically around finalization:

- Determine whether Cancel can arrive after yt-dlp has effectively completed the
  media download but before the application receives/processes the final exit
  state.
- Confirm whether FFmpeg is still running when cancellation cleanup begins.
- Confirm which process actually owns the WebM and final MP4/MP3 files at the
  moment cleanup runs.
- Check whether cleanup happens before all child processes have terminated.
- Check whether output-path detection runs after cancellation and can race with
  cleanup.
- Perform a second cleanup scan after the complete process tree has terminated.
- Ensure that a cancellation occurring during finalization is classified
  consistently as **Cancelled**, unless completion was already definitively
  recorded before the cancel request.
- Make cleanup target only files belonging to the current download so unrelated
  files are not deleted.

## Important State-Handling Question

The fix should establish a clear rule for the narrow race where Cancel and
successful completion happen almost simultaneously.

Recommended behavior for testing:

**Cancel wins if the cancellation request is accepted before the application
records the download as completed.**

**Completion wins if the download was already definitively completed before the
cancellation request was processed.**

The application should not report **Failed** merely because cancellation caused
the underlying process to terminate.

## Regression Requirements

After fixing BUG-003, retest all of the following:

- Single MP4 normal completion
- Single MP3 normal completion
- Single MP4 normal cancellation
- Single MP3 normal cancellation
- Immediate MP4 cancellation
- Immediate MP3 cancellation
- Mid-download MP4 cancellation
- Mid-download MP3 cancellation
- Very short download cancellation
- Cancellation immediately before completion
- Cancellation during final FFmpeg processing
- Cancellation at the absolute end of the download
- MP4 cancellation cleanup
- MP3 cancellation cleanup
- WebM intermediate cleanup
- `.part` cleanup
- Process-tree termination
- Correct Cancelled status
- No incorrect Download Failed message
- Normal successful completion immediately after cancellation tests
- Playlist cancellation
- Playlist item cancellation

---

# Updated Current Bug Summary

| ID | Issue | Severity | Current Status |
|---|---|---:|---|
| BUG-001 | Fast cancellation can leave intermediate yt-dlp files | Medium | Partially addressed / superseded by BUG-003 |
| BUG-002 | Playlist resume can re-trigger playlist/video selection | Low/Medium | **Fixed / Verified** |
| BUG-003 | End-of-download cancellation can report failure and leave WebM + MP4/MP3 | Medium | **Open** |

---

# September 10 Testing Note

The broad September 10 manual test cycle passed the normal Cancel workflow,
including MP3/MP4 cancellation and temporary-file cleanup.

BUG-003 was identified as a separate edge-case race during additional testing
because it requires cancellation at a very specific point: the absolute end of
the download/finalization process.

Therefore, the normal Cancel test should remain marked **PASS**, while BUG-003
remains an **open edge-case defect** until its exact timing and cleanup behavior
are fixed and specifically retested.

---

# Next Bug-Fix Test Requirement

BUG-003 should be fixed and then tested independently before being marked
resolved.

The successful completion path must also be regression-tested because the bug
occurs at the boundary between **Cancelled**, **Failed**, and **Completed**
states.
