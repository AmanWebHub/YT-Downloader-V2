#include "MainWindow.h"
#include "resource.h"
#include "DownloadManager.h"
#include "CompletionWindow.h"
#include "DownloadLogger.h"

#include <commctrl.h>
#include <uxtheme.h>
#include <algorithm>
#include <memory>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")

namespace
{
    const wchar_t CLASS_NAME[] = L"ITDownloaderV2Window";

    constexpr COLORREF CLR_BG        = RGB(248, 248, 248);
    constexpr COLORREF CLR_SURFACE   = RGB(255, 255, 255);
    constexpr COLORREF CLR_TEXT      = RGB(24, 24, 24);
    constexpr COLORREF CLR_MUTED     = RGB(105, 105, 105);
    constexpr COLORREF CLR_BORDER    = RGB(220, 220, 220);
    constexpr COLORREF CLR_RED       = RGB(220, 38, 38);
    constexpr COLORREF CLR_RED_DARK  = RGB(185, 28, 28);
    constexpr COLORREF CLR_DISABLED  = RGB(180, 180, 180);

    HFONT MakeFont(int height, int weight)
    {
        return CreateFontW(
            height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    void DeleteFont(HFONT& font)
    {
        if (font) { DeleteObject(font); font = nullptr; }
    }
}

bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow, const std::wstring& initialUrl)
{
    m_hInstance = hInstance;

    INITCOMMONCONTROLSEX cc = { sizeof(cc), ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&cc);

    WNDCLASSW wc{};
    wc.lpfnWndProc = MainWindow::WindowProcStatic;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;

    static bool registered = false;
    if (!registered) { RegisterClassW(&wc); registered = true; }

    m_hwnd = CreateWindowExW(
        0, CLASS_NAME, L"IT Downloader V2",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 480,
        nullptr, nullptr, hInstance, this);

    if (!m_hwnd)
    {
        MessageBoxW(nullptr, L"Failed to create the main window.",
                    L"IT Downloader V2", MB_OK | MB_ICONERROR);
        return false;
    }

    if (!initialUrl.empty())
        SetWindowTextW(m_urlEdit, initialUrl.c_str());

    // Center window
    RECT rc; GetWindowRect(m_hwnd, &rc);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(m_hwnd, nullptr, (sw - w) / 2, (sh - h) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);
    return true;
}

LRESULT CALLBACK MainWindow::WindowProcStatic(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MainWindow* self = nullptr;
    if (uMsg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return self ? self->HandleMessage(hwnd, uMsg, wParam, lParam)
                : DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        CreateControls(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1; // we paint the background ourselves

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Double-buffer to avoid flicker
        RECT rc; GetClientRect(hwnd, &rc);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBmp = SelectObject(memDC, bmp);
        PaintBackground(memDC, rc);
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(bmp);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_TEXT);
        return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    }

    case WM_CTLCOLOREDIT:
    {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkColor(hdc, CLR_SURFACE);
        SetTextColor(hdc, CLR_TEXT);
        static HBRUSH brush = CreateSolidBrush(CLR_SURFACE);
        return reinterpret_cast<LRESULT>(brush);
    }

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IDC_DOWNLOAD_BTN: OnDownloadClicked(hwnd); return 0;
            case IDC_CANCEL_BTN:   OnCancelClicked(); return 0;
            case IDC_PAUSE_BTN:    OnPauseResumeClicked(hwnd); return 0;
            case IDC_RADIO_MP4:    SetFormatSelection(false); return 0;
            case IDC_RADIO_MP3:    SetFormatSelection(true); return 0;
            }
        }
        break;

    case WM_DRAWITEM:
        DrawOwnerButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        return TRUE;

    case WM_APP_DOWNLOAD_PROGRESS:
        if (m_progressBar)
            SendMessageW(m_progressBar, PBM_SETPOS, wParam, 0);
        UpdateProgressText(static_cast<int>(wParam));
        return 0;

    case WM_APP_DOWNLOAD_STATUS:
    {
        auto* status = reinterpret_cast<std::wstring*>(lParam);
        if (status)
        {
            m_lastStatusText = *status;
            if (m_statusLabel) SetWindowTextW(m_statusLabel, status->c_str());
            delete status;
        }
        return 0;
    }

    case WM_APP_DOWNLOAD_FINISHED:
    {
        auto* info = reinterpret_cast<DownloadFinishedInfo*>(lParam);
        if (!info) return 0;

        const DWORD exitCode = info->exitCode;
        const std::wstring folder = info->downloadsFolder;
        const std::wstring filePath = info->filePath;
        const bool wasPaused = info->wasPaused;
        const bool wasCancelled = info->wasCancelled;

        if (wasPaused)
        {
            m_isPaused = true;
            m_cancelPending = false;
            EnableWindow(m_pauseButton, TRUE);
            SetWindowTextW(m_pauseButton, L"Resume");
            SetWindowTextW(m_statusLabel, L"Paused");
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        else if (wasCancelled)
        {
            SetDownloadingState(false);
            m_cancelPending = false;
            SetWindowTextW(m_statusLabel, L"Download cancelled.");
        }
        else
        {
            SetDownloadingState(false);
            m_cancelPending = false;
            if (exitCode == 0)
            {
                SetWindowTextW(m_statusLabel, L"Download complete.");
                SendMessageW(m_progressBar, PBM_SETPOS, 100, 0);
                UpdateProgressText(100);
                CompletionWindow::Create(m_hInstance, m_hwnd,
                                         filePath.empty() ? folder : filePath);
            }
            else
            {
                SetWindowTextW(m_statusLabel, L"Download failed.");
                if (exitCode != PRE_LAUNCH_FAILURE_CODE)
                {
                    MessageBoxW(hwnd,
                        L"The download could not be completed. Please check the URL and try again.",
                        L"IT Downloader V2", MB_OK | MB_ICONERROR);
                }
            }
        }
        delete info;
        return 0;
    }

    case WM_DESTROY:
        DownloadManager::CancelDownload();
        DeleteFont(m_titleFont);
        DeleteFont(m_sectionFont);
        DeleteFont(m_bodyFont);
        DeleteFont(m_smallFont);
        DeleteFont(m_buttonFont);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void MainWindow::CreateControls(HWND hwnd)
{
    // Fonts – reduced sizes for a cleaner look
    m_titleFont   = MakeFont(22, FW_SEMIBOLD);
    m_sectionFont = MakeFont(13, FW_SEMIBOLD);
    m_bodyFont    = MakeFont(13, FW_NORMAL);
    m_smallFont   = MakeFont(12, FW_NORMAL);
    m_buttonFont  = MakeFont(12, FW_SEMIBOLD);

    auto makeStatic = [&](const wchar_t* text, int x, int y, int w, int h, HFONT font) -> HWND
    {
        HWND ctrl = CreateWindowW(L"STATIC", text, WS_VISIBLE | WS_CHILD,
                                  x, y, w, h, hwnd, nullptr, nullptr, nullptr);
        SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return ctrl;
    };

    // Title
    makeStatic(L"IT Downloader V2", 36, 24, 400, 30, m_titleFont);

    // Subtitle
    makeStatic(L"Download videos and audio with a simple, focused workflow.",
               38, 58, 600, 20, m_smallFont);

    // URL label
    makeStatic(L"Video or playlist URL", 36, 96, 200, 20, m_sectionFont);

    m_urlEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
        36, 120, 648, 34,
        hwnd, (HMENU)(INT_PTR)IDC_URL, nullptr, nullptr);
    SendMessageW(m_urlEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_bodyFont), TRUE);
    SendMessageW(m_urlEdit, EM_SETCUEBANNER, TRUE,
                 reinterpret_cast<LPARAM>(L"Paste video or playlist URL..."));

    // Format label
    makeStatic(L"Format", 36, 174, 200, 20, m_sectionFont);

    // MP4 and MP3 cards – custom drawn
    m_mp4Button = CreateWindowW(
        L"BUTTON", L"MP4 Video\nVideo",
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        36, 200, 314, 56,
        hwnd, (HMENU)(INT_PTR)IDC_RADIO_MP4, nullptr, nullptr);

    m_mp3Button = CreateWindowW(
        L"BUTTON", L"MP3 Audio\nAudio",
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        370, 200, 314, 56,
        hwnd, (HMENU)(INT_PTR)IDC_RADIO_MP3, nullptr, nullptr);

    // Action buttons
    m_downloadButton = CreateWindowW(
        L"BUTTON", L"Download",
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        36, 280, 180, 42,
        hwnd, (HMENU)(INT_PTR)IDC_DOWNLOAD_BTN, nullptr, nullptr);

    m_pauseButton = CreateWindowW(
        L"BUTTON", L"Pause",
        WS_CHILD | BS_OWNERDRAW,
        232, 280, 120, 42,
        hwnd, (HMENU)(INT_PTR)IDC_PAUSE_BTN, nullptr, nullptr);

    m_cancelButton = CreateWindowW(
        L"BUTTON", L"Cancel",
        WS_CHILD | BS_OWNERDRAW,
        364, 280, 120, 42,
        hwnd, (HMENU)(INT_PTR)IDC_CANCEL_BTN, nullptr, nullptr);

    // Progress
    m_progressBar = CreateWindowExW(
        0, PROGRESS_CLASSW, nullptr,
        WS_VISIBLE | WS_CHILD | PBS_SMOOTH,
        36, 340, 648, 12,
        hwnd, (HMENU)(INT_PTR)IDC_PROGRESS, nullptr, nullptr);

    SendMessageW(m_progressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(m_progressBar, PBM_SETPOS, 0, 0);
    SendMessageW(m_progressBar, PBM_SETBARCOLOR, 0, CLR_RED);

    // Status row
    m_statusCaption = makeStatic(L"Status", 36, 364, 60, 20, m_smallFont);
    m_statusLabel   = makeStatic(L"Ready", 96, 364, 400, 20, m_bodyFont);
    m_progressPercent = makeStatic(L"0%", 620, 364, 64, 20, m_smallFont);

    SetFormatSelection(false);
    SetDownloadingState(false);
    ApplyControlFonts();
}

void MainWindow::ApplyControlFonts()
{
    auto setFont = [this](HWND ctrl) {
        if (ctrl) SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(m_buttonFont), TRUE);
    };
    setFont(m_downloadButton);
    setFont(m_pauseButton);
    setFont(m_cancelButton);
    setFont(m_mp4Button);
    setFont(m_mp3Button);
}

void MainWindow::PaintBackground(HDC hdc, const RECT& clientRect)
{
    // Background
    HBRUSH bgBrush = CreateSolidBrush(CLR_BG);
    FillRect(hdc, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Draw four rounded cards
    const int left = 20, right = clientRect.right - 20;
    const RECT cards[4] = {
        { left, 86, right, 166 },
        { left, 178, right, 268 },
        { left, 270, right, 334 },
        { left, 330, right, 392 }
    };

    HBRUSH surfBrush = CreateSolidBrush(CLR_SURFACE);
    HPEN   borderPen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HGDIOBJ oldBrush = SelectObject(hdc, surfBrush);
    HGDIOBJ oldPen   = SelectObject(hdc, borderPen);

    for (const auto& rc : cards)
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    DeleteObject(surfBrush);
}

void MainWindow::DrawFormatCard(HDC hdc, const RECT& rect, const wchar_t* title,
                                const wchar_t* subtitle, bool selected)
{
    COLORREF fill = selected ? RGB(255, 240, 240) : CLR_SURFACE;
    COLORREF line = selected ? CLR_RED : CLR_BORDER;
    COLORREF titleColor = selected ? CLR_RED_DARK : CLR_TEXT;

    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, selected ? 2 : 1, line);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rect.left + 1, rect.top + 1, rect.right - 1, rect.bottom - 1, 7, 7);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, titleColor);
    SelectObject(hdc, m_buttonFont);
    RECT titleRect{ rect.left + 16, rect.top + 10, rect.right - 16, rect.top + 32 };
    DrawTextW(hdc, title, -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SetTextColor(hdc, CLR_MUTED);
    SelectObject(hdc, m_smallFont);
    RECT subRect{ rect.left + 16, rect.top + 32, rect.right - 16, rect.bottom - 8 };
    DrawTextW(hdc, subtitle, -1, &subRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    if (selected)
    {
        HBRUSH dot = CreateSolidBrush(CLR_RED);
        HGDIOBJ old = SelectObject(hdc, dot);
        Ellipse(hdc, rect.right - 28, rect.top + 20, rect.right - 16, rect.top + 32);
        SelectObject(hdc, old);
        DeleteObject(dot);
    }
}

void MainWindow::DrawOwnerButton(const DRAWITEMSTRUCT* dis)
{
    if (!dis) return;

    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    const bool pressed = (dis->itemState & ODS_SELECTED) != 0;

    // Format selection cards
    if (dis->CtlID == IDC_RADIO_MP4 || dis->CtlID == IDC_RADIO_MP3)
    {
        const bool selected = (dis->CtlID == IDC_RADIO_MP3) == m_selectedMp3;
        DrawFormatCard(
            hdc, rc,
            dis->CtlID == IDC_RADIO_MP3 ? L"MP3 Audio" : L"MP4 Video",
            dis->CtlID == IDC_RADIO_MP3 ? L"Audio only • MP3" : L"Video • MP4",
            selected && !disabled);
        return;
    }

    COLORREF fill = CLR_SURFACE, text = CLR_TEXT, border = CLR_BORDER;

    if (dis->CtlID == IDC_DOWNLOAD_BTN)
    {
        fill = disabled ? RGB(220, 220, 220) : (pressed ? CLR_RED_DARK : CLR_RED);
        text = RGB(255, 255, 255);
        border = fill;
    }
    else if (disabled)
    {
        fill = RGB(235, 235, 235);
        text = CLR_DISABLED;
        border = RGB(200, 200, 200);
    }
    else if (pressed)
    {
        fill = RGB(242, 242, 242);
        border = RGB(190, 190, 190);
    }

    HBRUSH brush = CreateSolidBrush(fill);
    HPEN   pen   = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen   = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1, 6, 6);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    wchar_t label[128];
    GetWindowTextW(dis->hwndItem, label, 128);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, text);
    SelectObject(hdc, m_buttonFont);
    DrawTextW(hdc, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (dis->itemState & ODS_FOCUS)
    {
        RECT focus = rc;
        InflateRect(&focus, -4, -4);
        DrawFocusRect(hdc, &focus);
    }
}

void MainWindow::SetFormatSelection(bool mp3)
{
    m_selectedMp3 = mp3;
    if (m_mp4Button) InvalidateRect(m_mp4Button, nullptr, FALSE);
    if (m_mp3Button) InvalidateRect(m_mp3Button, nullptr, FALSE);
}

void MainWindow::UpdateProgressText(int progress)
{
    progress = (std::max)(0, (std::min)(100, progress));
    if (m_progressPercent)
        SetWindowTextW(m_progressPercent, (std::to_wstring(progress) + L"%").c_str());
}

void MainWindow::SetDownloadingState(bool downloading)
{
    m_isPaused = false;
    EnableWindow(m_urlEdit, !downloading);
    EnableWindow(m_mp4Button, !downloading);
    EnableWindow(m_mp3Button, !downloading);
    EnableWindow(m_downloadButton, !downloading);

    if (downloading)
    {
        ShowWindow(m_pauseButton, SW_SHOW);
        ShowWindow(m_cancelButton, SW_SHOW);
        SetWindowTextW(m_pauseButton, L"Pause");
        EnableWindow(m_pauseButton, TRUE);
        EnableWindow(m_cancelButton, TRUE);
        m_cancelPending = false;
    }
    else
    {
        ShowWindow(m_pauseButton, SW_HIDE);
        ShowWindow(m_cancelButton, SW_HIDE);
        m_cancelPending = false;
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

int MainWindow::ResolvePlaylistChoice(HWND hwnd, const std::wstring& url)
{
    bool hasList = url.find(L"list=") != std::wstring::npos;
    bool hasVideo = url.find(L"v=") != std::wstring::npos;
    if (!hasList) return 0;
    if (!hasVideo) return 1;

    int result = MessageBoxW(hwnd,
        L"This URL contains a playlist.\n\n"
        L"What would you like to download?\n\n"
        L"Yes  — Entire playlist\n"
        L"No   — This video only",
        L"Playlist detected", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (result == IDYES) return 1;
    if (result == IDNO) return 0;
    return -1;
}

void MainWindow::OnDownloadClicked(HWND hwnd)
{
    wchar_t urlBuffer[4096];
    GetWindowTextW(m_urlEdit, urlBuffer, 4096);
    std::wstring url = urlBuffer;

    if (url.empty())
    {
        MessageBoxW(hwnd, L"Please enter a video or playlist URL.",
                    L"IT Downloader V2", MB_OK | MB_ICONINFORMATION);
        SetFocus(m_urlEdit);
        return;
    }

    int choice = ResolvePlaylistChoice(hwnd, url);
    if (choice == -1) return;

    StartDownloadWithParams(hwnd, url, m_selectedMp3, choice == 1);
}

void MainWindow::StartDownloadWithParams(HWND hwnd, const std::wstring& url,
                                         bool isMp3, bool isPlaylist)
{
    if (DownloadManager::StartDownload(hwnd, url, isMp3, isPlaylist))
    {
        m_lastUrl = url;
        m_lastIsMp3 = isMp3;
        m_lastIsPlaylist = isPlaylist;

        SendMessageW(m_progressBar, PBM_SETPOS, 0, 0);
        UpdateProgressText(0);
        SetWindowTextW(m_statusLabel, L"Starting download...");
        SetDownloadingState(true);
    }
}

void MainWindow::OnCancelClicked()
{
    if (m_cancelPending) return;
    m_cancelPending = true;
    DownloadManager::CancelDownload();
    EnableWindow(m_cancelButton, FALSE);
    EnableWindow(m_pauseButton, FALSE);
    SetWindowTextW(m_statusLabel, L"Cancelling...");
}

void MainWindow::OnPauseResumeClicked(HWND hwnd)
{
    if (!m_isPaused)
    {
        DownloadManager::PauseDownload();
        EnableWindow(m_pauseButton, FALSE);
        SetWindowTextW(m_statusLabel, L"Pausing...");
    }
    else
    {
        m_isPaused = false;
        StartDownloadWithParams(hwnd, m_lastUrl, m_lastIsMp3, m_lastIsPlaylist);
    }
}