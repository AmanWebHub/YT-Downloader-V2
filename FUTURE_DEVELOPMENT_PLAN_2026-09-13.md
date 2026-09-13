# YTDownloader V2 — Future Development Plan
**Date:** 2026-09-13
**Branch:** Extension

## Current Position

YTDownloader V2 has reached a strong functional milestone.

The current download system has been tested through both:
- the YTDownloader V2 EXE
- the YTDownloader V2 browser extension

Single downloads and playlist-related functionality are currently working in the tested scenarios.

The project should now move into a **bug-discovery → bug-fix → regression → release-hardening** cycle rather than immediately entering a large refactor.

## Priority 1 — Investigate Known Bugs

### Short Inside Playlist
A Short currently appears not to download when it is contained inside a playlist.

This is **not yet confirmed as a playlist bug**.

Required comparison:

| Test | Expected Purpose |
|---|---|
| Same Short by itself | Determine whether the video itself works |
| Same Short inside the original playlist | Confirm playlist-specific behavior |
| Another Short inside another playlist | Determine reproducibility |
| Same scenarios through EXE | Separate application issue from extension issue |
| Same scenarios through Extension | Validate URL/protocol path |

### Decision Rule

- Works alone + fails in playlist → investigate playlist handling.
- Fails alone + fails in playlist → investigate Short/yt-dlp/media-specific behavior.
- EXE works + Extension fails → investigate extension/protocol/URL handling.
- EXE fails + Extension fails → investigate application/download engine.

Do not change code until the failure path is confirmed.

## Priority 2 — Complete Bug Inventory

For every discovered bug, record:

- Bug ID
- Exact URL type
- Entry point: EXE / Extension
- Format: MP3 / MP4
- Playlist: Yes / No
- Reproduction steps
- Expected result
- Actual result
- Error/log output
- Reproducibility
- Root cause
- Fix
- Regression result

## Priority 3 — Regression Testing

After each fix, test the smallest related group first, then run a broader regression pass.

### Core Matrix

| Feature | EXE | Extension |
|---|---:|---:|
| Single MP3 | PASS | PASS |
| Single MP4 | PASS | PASS |
| Playlist MP3 | PASS | PASS |
| Playlist MP4 | PASS | PASS |
| Single item from playlist | PASS | PASS |
| Entire playlist | PASS | PASS |
| Playlist choice UI | PASS | PASS |
| Pause / Resume | PASS | PASS |
| Cancel | PASS | PASS |
| Completion window | PASS | PASS |
| Open / Folder actions | PASS | PASS |
| Special characters | PASS | PASS |
| Post-processing output | PASS | PASS |

The matrix should be updated as new tests are performed.

## Priority 4 — Release Preparation

After all known bugs are resolved:

1. Freeze feature changes temporarily.
2. Run a clean full regression test.
3. Build the application in Release configuration.
4. Test the Release EXE outside Visual Studio.
5. Verify required yt-dlp/FFmpeg files and runtime assets.
6. Verify the extension against the Release EXE.
7. Verify the custom `ytdlp:` protocol registration.
8. Verify installation/first-run behavior.
9. Clean temporary repository artifacts.
10. Update README and release documentation.
11. Tag a stable release.

## Priority 5 — Post-Stability Improvements

Only after the release candidate is stable:

- Consider breaking DownloadWorker into smaller components.
- Improve error messages.
- Add automated tests for URL classification and playlist detection.
- Add automated tests for output-file detection.
- Improve extension robustness.
- Improve UI polish.
- Add future downloader features.

## Recommended Development Order

**Bug discovery**
→ **Reproduce**
→ **Identify root cause**
→ **Minimal fix**
→ **Targeted retest**
→ **Regression test**
→ **Update logs**
→ **Release candidate**
→ **Refactor**
→ **New features**

This keeps the current stable download functionality protected while the Extension branch continues to evolve.
