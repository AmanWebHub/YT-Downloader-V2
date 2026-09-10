# YT Downloader V2 — Change Log

**Log Date:** September 10, 2026
**Scope:** UI layer implementation review (MainWindow.cpp, CompletionWindow.cpp, DownloadWorker.cpp, PlaylistChoiceWindow.cpp)
**Status:** Playlist-choice UI feature confirmed implemented and wired end-to-end

---

## New Feature: Custom Playlist-Choice Window

The planned replacement for the old `MessageBoxW` playlist prompt is implemented and in use.

### PlaylistChoiceWindow.cpp / .h

- Added `PlaylistChoiceWindow`, a custom-styled modal window shown when a URL contains both a video and a playlist reference.
- Presents two owner-drawn "card" buttons:
  - **Download Entire Playlist** — styled as the primary action (filled red).
  - **Download This Video Only** — styled as the secondary action (outlined).
- Includes a **Cancel** button and supports `Esc` / window-close as cancel.
- Blocks the calling thread while pumping its own message loop, disabling the owner window for the duration (modal behavior) and re-enabling/refocusing it afterward.
- Result is written back to the caller via a pointer into the caller's stack frame (`m_resultOut`), set at `WM_NCDESTROY` just before the object deletes itself — avoids any use-after-free on the result value.
- Shares the same visual language (palette, rounded cards, typography) as `CompletionWindow`, so it reads as part of the same app.

### MainWindow.cpp — Integration

- Added `MainWindow::ResolvePlaylistChoice()`:
  - No `list=` in URL → proceeds as a single video (no prompt).
  - `list=` present but no `v=` → treated as a pure playlist (no prompt).
  - Both present → shows `PlaylistChoiceWindow` and returns the user's choice.
- `OnDownloadClicked()` now calls `ResolvePlaylistChoice()` before starting a download, and aborts cleanly if the user cancels.

---

## MainWindow.cpp — Other Confirmed UI Work

- `WS_CLIPCHILDREN` is set on window creation, consistent with the earlier fix to stop background repaint from drawing over child controls.
- Status label (`m_statusLabel`) and progress-percent label (`m_progressPercent`) are painted **opaque** (not transparent) in `WM_CTLCOLORSTATIC`, explicitly to prevent old text from showing underneath new text — matches the previously logged ghosting/overlap fix.
- Added a dedicated `WM_CTLCOLORSTATIC` branch for the disabled URL edit box, fixing a bug where it would fall through to the generic label branch and pick up an uninitialized (black) background while a download was active.
- Main window painting uses a double-buffered `WM_PAINT` (offscreen `CreateCompatibleBitmap` + `BitBlt`) to avoid flicker.
- `WM_APP_DOWNLOAD_FINISHED` handling distinguishes three end states — **paused**, **cancelled**, and **completed/failed** (by exit code) — each driving distinct status text and button states.
- Pause/Resume button reuses `StartDownloadWithParams()` with the last-used URL/format/playlist flags on Resume, rather than restarting from scratch.

## DownloadWorker.cpp — Confirmed Consistent With Backend Changelog

- Uses `--print after_move:__ITD_FILE__:%(filepath)s` to get the authoritative post-processing output path (confirmed present at line ~1274).
- Sets `PYTHONUTF8=1` and `PYTHONIOENCODING=utf-8` on the child process environment, explicitly overriding any pre-existing values of those variables so the app's settings are authoritative.
- Excludes the `.itdownloader_session.tmp` session manifest file by exact match.
- Falls back to `DownloadUtils::FindNewestFileSince()` when console-based filename parsing isn't sufficient.

## CompletionWindow.cpp — Confirmed Consistent With Backend Changelog

- `Create()` takes an explicit `isPlaylist` flag, stored as `m_isPlaylist`.
- `OnOpenFolderClicked()` branches on `m_isPlaylist` (or `m_isFolderOnly`) to open the destination **folder** directly via `ShellExecuteW`, rather than assuming the completion path is a single file.
- Playlist completions show a reduced action set (**Open Folder | Done**) vs. the full set for single downloads (**Open | Open With... | Open Folder | Done**).

---

## Summary

This pass confirms that the "Next Development Phase" item from the September 4 README — a dedicated playlist-choice interface with **This Video / Entire Playlist / Cancel** — has been fully implemented in `PlaylistChoiceWindow.cpp` and wired into `MainWindow.cpp`. No regressions were observed in the previously logged backend fixes (output detection, UTF-8 handling, session-file exclusion, playlist folder handling); their implementations in the newly reviewed files match what the August 30 / September 4 logs described.

**Not yet covered by this log:** no new automated/manual test cycle has been run against this specific playlist-choice UI. Recommend a targeted test pass (playlist-URL-with-video prompt trigger, both card choices, Cancel, Esc, window-close) before merging this UI work to the same "stable" status as the core engine.
