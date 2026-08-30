#pragma once

#include <windows.h>
#include <string>

class CompletionWindow
{
public:
    static CompletionWindow* Create(HINSTANCE hInstance,
                                    HWND ownerToRestore,
                                    const std::wstring& filePath);

private:
    static LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void CreateControls(HWND hwnd);
    void DrawButton(const DRAWITEMSTRUCT* dis);
    void Paint(HDC hdc, const RECT& rc);
    void OnOpenFileClicked();
    void OnOpenWithClicked();
    void OnOpenFolderClicked();

    HWND m_hwnd = nullptr;
    HWND m_ownerToRestore = nullptr;
    HWND m_pathLabel = nullptr;
    HFONT m_titleFont = nullptr;
    HFONT m_bodyFont = nullptr;
    HFONT m_buttonFont = nullptr;
    std::wstring m_filePath;
    bool m_isFolderOnly = false;
};
