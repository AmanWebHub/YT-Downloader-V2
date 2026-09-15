#pragma once

#include <windows.h>
#include <string>

class CompletionWindow
{
public:
    static CompletionWindow* Create(
        HINSTANCE hInstance,
        HWND ownerToRestore,
        const std::wstring& filePath,
        bool isPlaylist,
        bool closeWindowsOnAction = false);

private:
    static LRESULT CALLBACK WindowProcStatic(
        HWND hwnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam);

    LRESULT HandleMessage(
        HWND hwnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam);

    void CreateControls(HWND hwnd);
    void PaintBackground(HDC hdc, const RECT& rc);
    void DrawOwnerButton(const DRAWITEMSTRUCT* dis);

    void OnOpenClicked();
    void OnOpenWithClicked();
    void OnOpenFolderClicked();

    // Destroys both this window and m_ownerToRestore (the main
    // window this session belongs to), instead of the normal
    // hide/restore dance - used when an Open/Open With/Open Folder
    // action succeeds on an extension-launched session, so it closes
    // itself out rather than leaving windows behind for the user to
    // clean up. Only ever touches this session's own two windows.
    void CloseAssociatedWindows();

    HWND m_hwnd = nullptr;
    HWND m_ownerToRestore = nullptr;
    HWND m_pathLabel = nullptr;
    HWND m_statusLabel = nullptr;

    HFONT m_titleFont = nullptr;
    HFONT m_bodyFont = nullptr;
    HFONT m_smallFont = nullptr;
    HFONT m_buttonFont = nullptr;

    std::wstring m_filePath;

    // True when the completed operation was a playlist download.
    bool m_isPlaylist = false;

    // True when the supplied path itself is a folder.
    bool m_isFolderOnly = false;

    // True when Open/Open With/Open Folder should close this
    // session's windows after successfully performing the action
    // (see CloseAssociatedWindows()).
    bool m_closeWindowsOnAction = false;

    // Remaining short-interval retries for forcing this window to
    // the foreground (see the WM_TIMER handling in HandleMessage and
    // ForceForegroundWindow in the .cpp) - a single synchronous
    // attempt can lose a race with the OS/another app's own
    // activation handling, so this keeps trying briefly instead of
    // giving up after one shot. Reaches 0 once it succeeds or the
    // retries run out, at which point the timer is stopped.
    int m_foregroundRetriesRemaining = 0;
};