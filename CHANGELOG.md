# YTDownloader V2 — Change Log

**Version:** V2 — Development/Stabilization Build  
**Log Date:** September 4, 2026  
**Status:** Core functionality stable and tested

## MainWindow.cpp

1. **Removed stray Markdown code fences**
   - Removed 34 leftover ` ``` ` lines accidentally inserted into the source.
   - Eliminated the resulting invalid-token and syntax errors.

2. **Fixed `CompletionWindow::Create()` argument**
   - Added the missing `m_lastIsPlaylist` argument.
   - Ensures the completion window correctly knows whether the completed download is a playlist.

3. **Fixed download-status text repaint/overlap**
   - Added `WS_CLIPCHILDREN` to the main window.
   - Prevents background repainting from drawing over child controls.

4. **Fixed progress-percentage label ghosting**
   - Updated `WM_CTLCOLORSTATIC` handling for `m_progressPercent`.
   - Prevents old percentage values from remaining visible underneath new values.

5. **Fixed playlist Open Folder behavior**
   - Playlists now explicitly resolve to the download folder instead of the last resolved media file.

## DownloadOutput.cpp

6. **Reduced progress-message flooding**
   - Added progress de-duplication so unchanged percentage updates do not flood the UI message queue.

7. **Implemented `__ITD_FILE__` output marker**
   - Added `__ITD_FILE__` as the preferred output-file identification mechanism.
   - Added validation against `NA` and malformed values.

## DownloadWorker.cpp

8. **Fixed output filename timing**
   - Changed the print hook from `before_dl` to `after_move`.
   - Ensures the application receives the post-processing/final filename.

9. **Fixed child-process encoding**
   - Added `PYTHONUTF8=1` and `PYTHONIOENCODING=utf-8`.
   - Improves handling of non-ASCII and special-character titles.

10. **Replaced unreliable console filename parsing**
    - Added the filesystem-based `FindNewestFileSince` fallback.
    - Generalized it across supported download types.

11. **Prevented the session manifest from being selected**
    - Skips dot-prefixed files and `.tmp` files.
    - Excludes `.itdownloader_session.tmp` by exact path.

## DownloadUtils.cpp / DownloadUtils.h

12. **Improved `FindNewestFileSince()`**
    - Added the exclusion and skip logic needed for safe newest-file detection.

## CompletionWindow.cpp

13. **Hardened playlist Open Folder**
    - Explicitly launches Explorer with the supplied folder path.
    - Removes reliance on assumptions about whether the supplied path is a file or directory.

## Overall Result

The build compiles cleanly, download-status rendering is stable, output-file detection is more reliable, and Completion Window Open/Open With/Open Folder behavior works correctly across supported download types, playlists, and special-character titles.
