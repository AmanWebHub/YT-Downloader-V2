#include "MainWindow.h"
#include "resource.h"
#include "DownloadManager.h"

namespace
{
    const wchar_t CLASS_NAME[] = L"ITDownloaderV2Window";
}

bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow, const std::wstring& initialUrl)
{
    WNDCLASSW wc{};
    wc.lpfnWndProc = MainWindow::WindowProcStatic;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    m_hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"IT Downloader V2",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        700,
        500,
        nullptr,
        nullptr,
        hInstance,
        this // passed through to WM_NCCREATE so the static proc can find us
    );

    if (m_hwnd == nullptr)
    {
        MessageBoxW(
            nullptr,
            L"Failed to create the main window.",
            L"IT Downloader V2",
            MB_OK | MB_ICONERROR
        );
        return false;
    }

    if (!initialUrl.empty())
    {
        SetWindowTextW(GetDlgItem(m_hwnd, IDC_URL), initialUrl.c_str());
    }

    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);
    return true;
}

LRESULT CALLBACK MainWindow::WindowProcStatic(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MainWindow* self = nullptr;

    if (uMsg == WM_NCCREATE)
    {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self != nullptr)
    {
        return self->HandleMessage(hwnd, uMsg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        CreateControls(hwnd);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_DOWNLOAD_BTN)
        {
            OnDownloadClicked(hwnd);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void MainWindow::CreateControls(HWND hwnd)
{
    // Video URL label
    CreateWindowW(
        L"STATIC", L"Video URL:",
        WS_VISIBLE | WS_CHILD,
        30, 30, 100, 25,
        hwnd, nullptr, nullptr, nullptr
    );

    // URL input box
    CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT", L"",
        WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
        30, 60, 620, 30,
        hwnd, (HMENU)IDC_URL, nullptr, nullptr
    );

    // Format label
    CreateWindowW(
        L"STATIC", L"Format:",
        WS_VISIBLE | WS_CHILD,
        30, 110, 100, 25,
        hwnd, nullptr, nullptr, nullptr
    );

    // MP4 radio button (first in the group, default checked)
    CreateWindowW(
        L"BUTTON", L"MP4 Video",
        WS_VISIBLE | WS_CHILD | WS_GROUP | BS_AUTORADIOBUTTON,
        30, 140, 120, 25,
        hwnd, (HMENU)IDC_RADIO_MP4, nullptr, nullptr
    );

    // MP3 radio button
    CreateWindowW(
        L"BUTTON", L"MP3 Audio",
        WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
        160, 140, 120, 25,
        hwnd, (HMENU)IDC_RADIO_MP3, nullptr, nullptr
    );

    SendMessageW(GetDlgItem(hwnd, IDC_RADIO_MP4), BM_SETCHECK, BST_CHECKED, 0);

    // Download button
    CreateWindowW(
        L"BUTTON", L"Download",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        30, 190, 120, 35,
        hwnd, (HMENU)IDC_DOWNLOAD_BTN, nullptr, nullptr
    );
}

void MainWindow::OnDownloadClicked(HWND hwnd)
{
    wchar_t urlBuffer[2048]{};
    GetWindowTextW(GetDlgItem(hwnd, IDC_URL), urlBuffer, 2048);

    bool isMp3 = (SendMessageW(GetDlgItem(hwnd, IDC_RADIO_MP3), BM_GETCHECK, 0, 0) == BST_CHECKED);

    // The window's job ends here: it just reports what the user asked for.
    // DownloadManager decides what to actually do about it.
    DownloadManager::StartDownload(hwnd, urlBuffer, isMp3);
}
