#pragma once

#include <windows.h>
#include <string>

// Self-owned: created with `new`, deletes itself once its HWND is
// destroyed. Shows the downloaded file's path with Open File / Open
// With / Open Folder / Close buttons. Hides the owner window while
// visible, and re-shows it when Close is clicked.
class CompletionWindow
{
public:
    static CompletionWindow* Create(HINSTANCE hInstance, HWND ownerToRestore, const std::wstring& filePath);

private:
    static LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void CreateControls(HWND hwnd);
    void OnOpenFileClicked();
    void OnOpenWithClicked();
    void OnOpenFolderClicked();

    HWND m_hwnd = nullptr;
    HWND m_ownerToRestore = nullptr;
    std::wstring m_filePath;
    bool m_isFolderOnly = false; // true if we only know the folder, not the exact file
};
