#include "CompletionWindow.h"
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>

#pragma comment(lib, "shell32.lib")

namespace
{
    const wchar_t CLASS_NAME[] = L"ITDownloaderV2CompletionWindow";

    constexpr int IDC_COMP_PATH_LABEL   = 2001;
    constexpr int IDC_COMP_OPEN_FILE    = 2002;
    constexpr int IDC_COMP_OPEN_WITH    = 2003;
    constexpr int IDC_COMP_OPEN_FOLDER  = 2004;
    constexpr int IDC_COMP_CLOSE        = 2005;
}

CompletionWindow* CompletionWindow::Create(HINSTANCE hInstance, HWND ownerToRestore, const std::wstring& filePath)
{
    CompletionWindow* self = new CompletionWindow();
    self->m_ownerToRestore = ownerToRestore;
    self->m_filePath = filePath;

    self->m_isFolderOnly =
        (GetFileAttributesW(filePath.c_str()) != INVALID_FILE_ATTRIBUTES) &&
        (GetFileAttributesW(filePath.c_str()) & FILE_ATTRIBUTE_DIRECTORY);

    WNDCLASSW wc{};
    wc.lpfnWndProc = CompletionWindow::WindowProcStatic;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    static bool registered = false;
    if (!registered)
    {
        RegisterClassW(&wc);
        registered = true;
    }

    self->m_hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Download Complete",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        480, 220,
        nullptr,
        nullptr,
        hInstance,
        self
    );

    if (self->m_hwnd == nullptr)
    {
        delete self;
        return nullptr;
    }

    // Swap: hide the main window, show this one.
    if (ownerToRestore != nullptr)
    {
        ShowWindow(ownerToRestore, SW_HIDE);
    }

    ShowWindow(self->m_hwnd, SW_SHOW);
    UpdateWindow(self->m_hwnd);
    return self;
}

LRESULT CALLBACK CompletionWindow::WindowProcStatic(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CompletionWindow* self = nullptr;

    if (uMsg == WM_NCCREATE)
    {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<CompletionWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<CompletionWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self != nullptr)
    {
        return self->HandleMessage(hwnd, uMsg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT CompletionWindow::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        CreateControls(hwnd);
        return 0;

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IDC_COMP_OPEN_FILE:
                OnOpenFileClicked();
                return 0;
            case IDC_COMP_OPEN_WITH:
                OnOpenWithClicked();
                return 0;
            case IDC_COMP_OPEN_FOLDER:
                OnOpenFolderClicked();
                return 0;
            case IDC_COMP_CLOSE:
                DestroyWindow(hwnd);
                return 0;
            }
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_NCDESTROY:
        // Restore the main window right before self-deleting.
        if (m_ownerToRestore != nullptr)
        {
            ShowWindow(m_ownerToRestore, SW_SHOW);
            SetForegroundWindow(m_ownerToRestore);
        }
        delete this;
        return 0;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void CompletionWindow::CreateControls(HWND hwnd)
{
    CreateWindowW(
        L"STATIC",
        L"Download complete!",
        WS_VISIBLE | WS_CHILD,
        20, 15, 420, 25,
        hwnd, nullptr, nullptr, nullptr
    );

    CreateWindowW(
        L"STATIC",
        m_filePath.c_str(),
        WS_VISIBLE | WS_CHILD | SS_PATHELLIPSIS,
        20, 45, 420, 40,
        hwnd, (HMENU)(INT_PTR)IDC_COMP_PATH_LABEL, nullptr, nullptr
    );

    if (!m_isFolderOnly)
    {
        CreateWindowW(
            L"BUTTON", L"Open File",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            20, 110, 130, 35,
            hwnd, (HMENU)(INT_PTR)IDC_COMP_OPEN_FILE, nullptr, nullptr
        );

        CreateWindowW(
            L"BUTTON", L"Open With...",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            160, 110, 130, 35,
            hwnd, (HMENU)(INT_PTR)IDC_COMP_OPEN_WITH, nullptr, nullptr
        );

        CreateWindowW(
            L"BUTTON", L"Open Folder",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            300, 110, 130, 35,
            hwnd, (HMENU)(INT_PTR)IDC_COMP_OPEN_FOLDER, nullptr, nullptr
        );

        CreateWindowW(
            L"BUTTON", L"Close",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            300, 155, 130, 35,
            hwnd, (HMENU)(INT_PTR)IDC_COMP_CLOSE, nullptr, nullptr
        );
    }
    else
    {
        // Playlist case (or unresolved file) - only offer to open the
        // folder, no single file to point at.
        CreateWindowW(
            L"BUTTON", L"Open Folder",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            20, 110, 200, 35,
            hwnd, (HMENU)(INT_PTR)IDC_COMP_OPEN_FOLDER, nullptr, nullptr
        );

        CreateWindowW(
            L"BUTTON", L"Close",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            300, 110, 130, 35,
            hwnd, (HMENU)(INT_PTR)IDC_COMP_CLOSE, nullptr, nullptr
        );
    }
}

void CompletionWindow::OnOpenFileClicked()
{
    ShellExecuteW(m_hwnd, L"open", m_filePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void CompletionWindow::OnOpenWithClicked()
{
    OPENASINFO info{};
    info.pcszFile = m_filePath.c_str();
    info.pcszClass = nullptr;
    info.oaifInFlags = OAIF_EXEC | OAIF_ALLOW_REGISTRATION;

    const HRESULT hr = SHOpenWithDialog(m_hwnd, &info);

    if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_CANCELLED))
    {
        MessageBoxW(
            m_hwnd,
            L"Could not open the 'Open With' dialog.",
            L"IT Downloader V2",
            MB_OK | MB_ICONERROR);
    }
}

void CompletionWindow::OnOpenFolderClicked()
{
    if (m_isFolderOnly)
    {
        ShellExecuteW(m_hwnd, L"open", m_filePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    else
    {
        std::wstring args = L"/select,\"" + m_filePath + L"\"";
        ShellExecuteW(m_hwnd, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    }
}
