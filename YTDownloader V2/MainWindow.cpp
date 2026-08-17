#include "MainWindow.h"
#include "resource.h"
#include "DownloadManager.h"

#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

namespace
{
    const wchar_t CLASS_NAME[] = L"ITDownloaderV2Window";
}

bool MainWindow::Create(
    HINSTANCE hInstance,
    int nCmdShow,
    const std::wstring& initialUrl)
{
    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_PROGRESS_CLASS;

    InitCommonControlsEx(&commonControls);

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
        this);

    if (m_hwnd == nullptr)
    {
        MessageBoxW(
            nullptr,
            L"Failed to create the main window.",
            L"IT Downloader V2",
            MB_OK | MB_ICONERROR);

        return false;
    }

    if (!initialUrl.empty())
    {
        SetWindowTextW(
            m_urlEdit,
            initialUrl.c_str());
    }

    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);

    return true;
}

LRESULT CALLBACK MainWindow::WindowProcStatic(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    MainWindow* self = nullptr;

    if (uMsg == WM_NCCREATE)
    {
        CREATESTRUCTW* cs =
            reinterpret_cast<CREATESTRUCTW*>(lParam);

        self =
            reinterpret_cast<MainWindow*>(cs->lpCreateParams);

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self =
            reinterpret_cast<MainWindow*>(
                GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self != nullptr)
    {
        return self->HandleMessage(
            hwnd,
            uMsg,
            wParam,
            lParam);
    }

    return DefWindowProcW(
        hwnd,
        uMsg,
        wParam,
        lParam);
}

LRESULT MainWindow::HandleMessage(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        CreateControls(hwnd);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_DOWNLOAD_BTN &&
            HIWORD(wParam) == BN_CLICKED)
        {
            OnDownloadClicked(hwnd);
            return 0;
        }
        break;

    case WM_APP_DOWNLOAD_PROGRESS:
        if (m_progressBar != nullptr)
        {
            SendMessageW(
                m_progressBar,
                PBM_SETPOS,
                wParam,
                0);
        }

        return 0;

    case WM_APP_DOWNLOAD_STATUS:
    {
        auto* status =
            reinterpret_cast<std::wstring*>(lParam);

        if (status != nullptr)
        {
            if (m_statusLabel != nullptr)
            {
                SetWindowTextW(
                    m_statusLabel,
                    status->c_str());
            }

            delete status;
        }

        return 0;
    }

    case WM_APP_DOWNLOAD_FINISHED:
    {
        auto* info =
            reinterpret_cast<DownloadFinishedInfo*>(lParam);

        if (info != nullptr)
        {
            const DWORD exitCode = info->exitCode;
            const std::wstring folder = info->downloadsFolder;

            SetDownloadingState(false);

            if (exitCode == 0)
            {
                if (m_statusLabel != nullptr)
                {
                    SetWindowTextW(
                        m_statusLabel,
                        L"Download complete.");
                }

                MessageBoxW(
                    hwnd,
                    (L"Download complete!\n\nSaved to:\n" + folder)
                        .c_str(),
                    L"IT Downloader V2",
                    MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                if (m_statusLabel != nullptr)
                {
                    SetWindowTextW(
                        m_statusLabel,
                        L"Download failed.");
                }

                if (exitCode != 1)
                {
                    std::wstring message =
                        L"yt-dlp exited with error code " +
                        std::to_wstring(exitCode) +
                        L".\n\nCheck the status shown in the window.";

                    MessageBoxW(
                        hwnd,
                        message.c_str(),
                        L"IT Downloader V2",
                        MB_OK | MB_ICONERROR);
                }
            }

            delete info;
        }

        return 0;
    }

    case WM_DESTROY:
        DownloadManager::StopDownload();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(
        hwnd,
        uMsg,
        wParam,
        lParam);
}

void MainWindow::CreateControls(HWND hwnd)
{
    CreateWindowW(
        L"STATIC",
        L"Video URL:",
        WS_VISIBLE | WS_CHILD,
        30, 30, 100, 25,
        hwnd,
        nullptr,
        nullptr,
        nullptr);

    m_urlEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
        30, 60, 620, 30,
        hwnd,
        (HMENU)IDC_URL,
        nullptr,
        nullptr);

    CreateWindowW(
        L"STATIC",
        L"Format:",
        WS_VISIBLE | WS_CHILD,
        30, 110, 100, 25,
        hwnd,
        nullptr,
        nullptr,
        nullptr);

    CreateWindowW(
        L"BUTTON",
        L"MP4 Video",
        WS_VISIBLE | WS_CHILD |
            WS_GROUP | BS_AUTORADIOBUTTON,
        30, 140, 120, 25,
        hwnd,
        (HMENU)IDC_RADIO_MP4,
        nullptr,
        nullptr);

    CreateWindowW(
        L"BUTTON",
        L"MP3 Audio",
        WS_VISIBLE | WS_CHILD |
            BS_AUTORADIOBUTTON,
        160, 140, 120, 25,
        hwnd,
        (HMENU)IDC_RADIO_MP3,
        nullptr,
        nullptr);

    SendMessageW(
        GetDlgItem(hwnd, IDC_RADIO_MP4),
        BM_SETCHECK,
        BST_CHECKED,
        0);

    m_downloadButton = CreateWindowW(
        L"BUTTON",
        L"Download",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        30, 190, 120, 35,
        hwnd,
        (HMENU)IDC_DOWNLOAD_BTN,
        nullptr,
        nullptr);

    m_progressBar = CreateWindowExW(
        0,
        PROGRESS_CLASSW,
        nullptr,
        WS_VISIBLE | WS_CHILD,
        30, 250, 620, 25,
        hwnd,
        (HMENU)IDC_PROGRESS,
        nullptr,
        nullptr);

    SendMessageW(
        m_progressBar,
        PBM_SETRANGE,
        0,
        MAKELPARAM(0, 100));

    SendMessageW(
        m_progressBar,
        PBM_SETPOS,
        0,
        0);

    m_statusLabel = CreateWindowW(
        L"STATIC",
        L"Ready.",
        WS_VISIBLE | WS_CHILD,
        30, 290, 620, 25,
        hwnd,
        (HMENU)IDC_STATUS,
        nullptr,
        nullptr);
}

void MainWindow::SetDownloadingState(bool downloading)
{
    if (m_downloadButton != nullptr)
    {
        EnableWindow(
            m_downloadButton,
            downloading ? FALSE : TRUE);
    }

    if (m_urlEdit != nullptr)
    {
        EnableWindow(
            m_urlEdit,
            downloading ? FALSE : TRUE);
    }

    EnableWindow(
        GetDlgItem(m_hwnd, IDC_RADIO_MP4),
        downloading ? FALSE : TRUE);

    EnableWindow(
        GetDlgItem(m_hwnd, IDC_RADIO_MP3),
        downloading ? FALSE : TRUE);
}

void MainWindow::OnDownloadClicked(HWND hwnd)
{
    wchar_t urlBuffer[2048]{};

    GetWindowTextW(
        m_urlEdit,
        urlBuffer,
        2048);

    const bool isMp3 =
        SendMessageW(
            GetDlgItem(hwnd, IDC_RADIO_MP3),
            BM_GETCHECK,
            0,
            0) == BST_CHECKED;

    if (DownloadManager::StartDownload(
        hwnd,
        urlBuffer,
        isMp3))
    {
        SendMessageW(
            m_progressBar,
            PBM_SETPOS,
            0,
            0);

        SetWindowTextW(
            m_statusLabel,
            L"Starting download...");

        SetDownloadingState(true);
    }
}
