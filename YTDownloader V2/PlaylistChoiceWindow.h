#pragma once

#include <windows.h>
#include <string>

// Custom-styled replacement for the old MessageBoxW playlist prompt.
// Shown whenever a URL contains both a specific video and a playlist
// reference, so the user can choose which one to download.
//
// PlaylistChoiceWindow::Show() blocks the calling thread (disabling
// `owner` for the duration, like a modal dialog) and returns once the
// user picks an option, closes the window, or presses Escape.
class PlaylistChoiceWindow
{
public:
    enum class Choice
    {
        Cancelled      = -1,
        SingleVideo    = 0,
        EntirePlaylist = 1
    };

    static Choice Show(
        HINSTANCE hInstance,
        HWND owner,
        const std::wstring& url);

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

    void Finish(Choice choice);

    HWND m_hwnd = nullptr;
    HWND m_owner = nullptr;

    HFONT m_titleFont = nullptr;
    HFONT m_bodyFont = nullptr;
    HFONT m_cardTitleFont = nullptr;
    HFONT m_cardBodyFont = nullptr;
    HFONT m_cancelFont = nullptr;

    Choice m_choice = Choice::Cancelled;

    // Points at a Choice living on Show()'s stack. Written just
    // before this object deletes itself in WM_NCDESTROY, so the
    // result survives past that point safely.
    Choice* m_resultOut = nullptr;
};
