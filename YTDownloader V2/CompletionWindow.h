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
        bool isPlaylist);

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
};