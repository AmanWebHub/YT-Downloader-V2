#include "MainWindow.h"
#include "resource.h"
#include "DownloadManager.h"
#include "CompletionWindow.h"
#include "DownloadLogger.h"

#include <commctrl.h>
#include <uxtheme.h>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")

namespace
{
    const wchar_t CLASS_NAME[] = L"ITDownloaderV2Window";

    // ------------------------------------------------------------
    // UI COLORS
    // ------------------------------------------------------------

    constexpr COLORREF CLR_BG =
        RGB(248, 249, 251);

    constexpr COLORREF CLR_WHITE =
        RGB(255, 255, 255);

    constexpr COLORREF CLR_TEXT =
        RGB(32, 34, 38);

    constexpr COLORREF CLR_SECONDARY =
        RGB(100, 105, 113);

    constexpr COLORREF CLR_BORDER =
        RGB(222, 225, 230);

    constexpr COLORREF CLR_RED =
        RGB(214, 39, 40);

    constexpr COLORREF CLR_RED_DARK =
        RGB(180, 28, 29);

    constexpr COLORREF CLR_RED_LIGHT =
        RGB(255, 246, 246);

    constexpr COLORREF CLR_PROGRESS_BG =
        RGB(232, 234, 238);

    constexpr COLORREF CLR_DISABLED =
        RGB(170, 174, 181);

    // ------------------------------------------------------------
    // FONT HELPERS
    // ------------------------------------------------------------

    HFONT MakeFont(int height, int weight)
    {
        return CreateFontW(
            height,
            0,
            0,
            0,
            weight,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI");
    }

    void DeleteFont(HFONT &font)
    {
        if (font)
        {
            DeleteObject(font);
            font = nullptr;
        }
    }

    // ------------------------------------------------------------
    // ROUNDED RECTANGLE HELPER
    // ------------------------------------------------------------

    void FillRounded(
        HDC hdc,
        const RECT &rc,
        COLORREF fill,
        COLORREF border,
        int radius = 8,
        int borderWidth = 1)
    {
        HBRUSH brush =
            CreateSolidBrush(fill);

        HPEN pen =
            CreatePen(
                PS_SOLID,
                borderWidth,
                border);

        HGDIOBJ oldBrush =
            SelectObject(hdc, brush);

        HGDIOBJ oldPen =
            SelectObject(hdc, pen);

        RoundRect(
            hdc,
            rc.left,
            rc.top,
            rc.right,
            rc.bottom,
            radius,
            radius);

        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);

        DeleteObject(pen);
        DeleteObject(brush);
    }
}

// ============================================================
// CREATE MAIN WINDOW
// ============================================================

bool MainWindow::Create(
    HINSTANCE hInstance,
    int nCmdShow,
    const std::wstring &initialUrl)
{
    m_hInstance = hInstance;

    INITCOMMONCONTROLSEX cc{
        sizeof(cc),
        ICC_PROGRESS_CLASS};

    InitCommonControlsEx(&cc);

    WNDCLASSW wc{};

    wc.lpfnWndProc =
        MainWindow::WindowProcStatic;

    wc.hInstance =
        hInstance;

    wc.lpszClassName =
        CLASS_NAME;

    wc.hCursor =
        LoadCursorW(nullptr, IDC_ARROW);

    wc.hbrBackground =
        nullptr;

    static bool registered = false;

    if (!registered)
    {
        RegisterClassW(&wc);
        registered = true;
    }

    m_hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"IT Downloader V2",
        WS_OVERLAPPED |
            WS_CAPTION |
            WS_SYSMENU |
            WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        720,
        500,
        nullptr,
        nullptr,
        hInstance,
        this);

    if (!m_hwnd)
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

    // Center window.
    RECT rc{};
    GetWindowRect(m_hwnd, &rc);

    const int w =
        rc.right - rc.left;

    const int h =
        rc.bottom - rc.top;

    const int sw =
        GetSystemMetrics(SM_CXSCREEN);

    const int sh =
        GetSystemMetrics(SM_CYSCREEN);

    SetWindowPos(
        m_hwnd,
        nullptr,
        (sw - w) / 2,
        (sh - h) / 2,
        0,
        0,
        SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(
        m_hwnd,
        nCmdShow);

    UpdateWindow(m_hwnd);

    return true;
}

// ============================================================
// WINDOW PROCEDURE
// ============================================================

LRESULT CALLBACK MainWindow::WindowProcStatic(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    MainWindow *self = nullptr;

    if (uMsg == WM_NCCREATE)
    {
        auto *cs =
            reinterpret_cast<CREATESTRUCTW *>(lParam);

        self =
            reinterpret_cast<MainWindow *>(
                cs->lpCreateParams);

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self =
            reinterpret_cast<MainWindow *>(
                GetWindowLongPtrW(
                    hwnd,
                    GWLP_USERDATA));
    }

    return self
               ? self->HandleMessage(
                     hwnd,
                     uMsg,
                     wParam,
                     lParam)
               : DefWindowProcW(
                     hwnd,
                     uMsg,
                     wParam,
                     lParam);
}

// ============================================================
// MESSAGE HANDLER
// ============================================================

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

    case WM_ERASEBKGND:
        // Background is painted manually.
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};

        HDC hdc =
            BeginPaint(hwnd, &ps);

        RECT rc{};

        GetClientRect(
            hwnd,
            &rc);

        // Double buffering.
        HDC memDC =
            CreateCompatibleDC(hdc);

        HBITMAP bmp =
            CreateCompatibleBitmap(
                hdc,
                rc.right,
                rc.bottom);

        HGDIOBJ oldBmp =
            SelectObject(
                memDC,
                bmp);

        PaintBackground(
            memDC,
            rc);

        BitBlt(
            hdc,
            0,
            0,
            rc.right,
            rc.bottom,
            memDC,
            0,
            0,
            SRCCOPY);

        SelectObject(
            memDC,
            oldBmp);

        DeleteObject(bmp);
        DeleteDC(memDC);

        EndPaint(
            hwnd,
            &ps);

        return 0;
    }

        // --------------------------------------------------------
        // STATIC TEXT
        // --------------------------------------------------------

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc =
            reinterpret_cast<HDC>(wParam);

        HWND control =
            reinterpret_cast<HWND>(lParam);

        // The status area is inside the white progress card.
        // Give it an opaque background so old status text is
        // completely erased before the new text is drawn.
        if (control == m_statusLabel ||
            control == m_progressPercent)
        {
            SetBkMode(
                hdc,
                OPAQUE);

            SetBkColor(
                hdc,
                CLR_WHITE);

            if (control == m_progressPercent)
            {
                SetTextColor(
                    hdc,
                    CLR_SECONDARY);
            }
            else
            {
                SetTextColor(
                    hdc,
                    CLR_TEXT);
            }

            static HBRUSH whiteBrush =
                CreateSolidBrush(CLR_WHITE);

            return reinterpret_cast<LRESULT>(
                whiteBrush);
        }

        // All other static controls remain transparent.
        SetBkMode(
            hdc,
            TRANSPARENT);

        SetTextColor(
            hdc,
            CLR_TEXT);

        return reinterpret_cast<LRESULT>(
            GetStockObject(NULL_BRUSH));
    }

        // --------------------------------------------------------
        // URL EDIT
        // --------------------------------------------------------

    case WM_CTLCOLOREDIT:
    {
        HDC hdc =
            reinterpret_cast<HDC>(wParam);

        SetBkColor(
            hdc,
            CLR_WHITE);

        SetTextColor(
            hdc,
            CLR_TEXT);

        static HBRUSH brush =
            CreateSolidBrush(CLR_WHITE);

        return reinterpret_cast<LRESULT>(
            brush);
    }

        // --------------------------------------------------------
        // BUTTON COMMANDS
        // --------------------------------------------------------

    case WM_COMMAND:

        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IDC_DOWNLOAD_BTN:
                OnDownloadClicked(hwnd);
                return 0;

            case IDC_CANCEL_BTN:
                OnCancelClicked();
                return 0;

            case IDC_PAUSE_BTN:
                OnPauseResumeClicked(hwnd);
                return 0;

            case IDC_RADIO_MP4:
                SetFormatSelection(false);
                return 0;

            case IDC_RADIO_MP3:
                SetFormatSelection(true);
                return 0;
            }
        }

        break;

        // --------------------------------------------------------
        // OWNER DRAWN BUTTONS
        // --------------------------------------------------------

    case WM_DRAWITEM:

        DrawOwnerButton(
            reinterpret_cast<const DRAWITEMSTRUCT *>(
                lParam));

        return TRUE;

        // --------------------------------------------------------
        // DOWNLOAD PROGRESS
        // --------------------------------------------------------

    case WM_APP_DOWNLOAD_PROGRESS:

        if (m_progressBar)
        {
            SendMessageW(
                m_progressBar,
                PBM_SETPOS,
                wParam,
                0);
        }

        UpdateProgressText(
            static_cast<int>(wParam));

        return 0;

        // --------------------------------------------------------
        // DOWNLOAD STATUS
        // --------------------------------------------------------

    case WM_APP_DOWNLOAD_STATUS:
    {
        auto *status =
            reinterpret_cast<std::wstring *>(lParam);

        if (status)
        {
            m_lastStatusText =
                *status;

            if (m_statusLabel)
            {
                SetWindowTextW(
                    m_statusLabel,
                    status->c_str());

                RedrawWindow(
                    m_statusLabel,
                    nullptr,
                    nullptr,
                    RDW_INVALIDATE |
                        RDW_ERASE |
                        RDW_UPDATENOW);
            }

            delete status;
        }

        return 0;
    }

        // --------------------------------------------------------
        // DOWNLOAD FINISHED
        // --------------------------------------------------------

    case WM_APP_DOWNLOAD_FINISHED:
    {
        auto *info =
            reinterpret_cast<DownloadFinishedInfo *>(
                lParam);

        if (!info)
            return 0;

        const DWORD exitCode =
            info->exitCode;

        const std::wstring folder =
            info->downloadsFolder;

        const std::wstring filePath =
            info->filePath;

        const bool wasPaused =
            info->wasPaused;

        const bool wasCancelled =
            info->wasCancelled;

        // ----------------------------------------------------
        // PAUSED
        // ----------------------------------------------------

        if (wasPaused)
        {
            m_isPaused = true;
            m_cancelPending = false;

            EnableWindow(
                m_pauseButton,
                TRUE);

            SetWindowTextW(
                m_pauseButton,
                L"Resume");

            SetWindowTextW(
                m_statusLabel,
                L"Paused");

            InvalidateRect(
                m_hwnd,
                nullptr,
                FALSE);
        }

        // ----------------------------------------------------
        // CANCELLED
        // ----------------------------------------------------

        else if (wasCancelled)
        {
            SetDownloadingState(false);

            m_cancelPending = false;

            SetWindowTextW(
                m_statusLabel,
                L"Download cancelled.");
        }

        // ----------------------------------------------------
        // FINISHED
        // ----------------------------------------------------

        else
        {
            SetDownloadingState(false);

            m_cancelPending = false;

            if (exitCode == 0)
            {
                SetWindowTextW(
                    m_statusLabel,
                    L"Download complete.");

                SendMessageW(
                    m_progressBar,
                    PBM_SETPOS,
                    100,
                    0);

                UpdateProgressText(100);

                // Preserve the existing playlist handling.
                CompletionWindow::Create(
                    m_hInstance,
                    m_hwnd,
                    m_lastIsPlaylist
                        ? folder
                        : (filePath.empty()
                               ? folder
                               : filePath),
                    m_lastIsPlaylist);
            }
            else
            {
                SetWindowTextW(
                    m_statusLabel,
                    L"Download failed.");

                if (exitCode != PRE_LAUNCH_FAILURE_CODE)
                {
                    MessageBoxW(
                        hwnd,
                        L"The download could not be completed. "
                        L"Please check the URL and try again.",
                        L"IT Downloader V2",
                        MB_OK | MB_ICONERROR);
                }
            }
        }

        delete info;

        return 0;
    }

        // --------------------------------------------------------
        // WINDOW DESTROY
        // --------------------------------------------------------

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

    return DefWindowProcW(
        hwnd,
        uMsg,
        wParam,
        lParam);
}

// ============================================================
// CREATE CONTROLS
// ============================================================

void MainWindow::CreateControls(HWND hwnd)
{
    // --------------------------------------------------------
    // FONTS
    // --------------------------------------------------------

    m_titleFont =
        MakeFont(24, FW_SEMIBOLD);

    m_sectionFont =
        MakeFont(13, FW_SEMIBOLD);

    m_bodyFont =
        MakeFont(14, FW_NORMAL);

    m_smallFont =
        MakeFont(12, FW_NORMAL);

    m_buttonFont =
        MakeFont(13, FW_SEMIBOLD);

    // --------------------------------------------------------
    // STATIC CONTROL HELPER
    // --------------------------------------------------------

    auto makeStatic =
        [&](const wchar_t *text,
            int x,
            int y,
            int w,
            int h,
            HFONT font) -> HWND
    {
        HWND ctrl =
            CreateWindowW(
                L"STATIC",
                text,
                WS_VISIBLE |
                    WS_CHILD,
                x,
                y,
                w,
                h,
                hwnd,
                nullptr,
                nullptr,
                nullptr);

        SendMessageW(
            ctrl,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(font),
            TRUE);

        return ctrl;
    };

    // ========================================================
    // HEADER
    // ========================================================

    makeStatic(
        L"IT Downloader V2",
        32,
        22,
        500,
        34,
        m_titleFont);

    makeStatic(
        L"Download a video or audio file quickly and simply.",
        34,
        57,
        620,
        20,
        m_smallFont);

    // ========================================================
    // URL SECTION
    // ========================================================

    makeStatic(
        L"Video or playlist URL",
        32,
        99,
        300,
        20,
        m_sectionFont);

    m_urlEdit =
        CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_VISIBLE |
                WS_CHILD |
                ES_AUTOHSCROLL,
            32,
            124,
            656,
            40,
            hwnd,
            (HMENU)(INT_PTR)IDC_URL,
            nullptr,
            nullptr);

    SendMessageW(
        m_urlEdit,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(m_bodyFont),
        TRUE);

    SendMessageW(
        m_urlEdit,
        EM_SETCUEBANNER,
        TRUE,
        reinterpret_cast<LPARAM>(
            L"Paste a video or playlist URL here"));

    SetWindowTheme(
        m_urlEdit,
        L"Explorer",
        nullptr);

    // ========================================================
    // FORMAT SECTION
    // ========================================================

    makeStatic(
        L"Format",
        32,
        184,
        200,
        20,
        m_sectionFont);

    // MP4
    m_mp4Button =
        CreateWindowW(
            L"BUTTON",
            L"MP4 Video",
            WS_VISIBLE |
                WS_CHILD |
                BS_OWNERDRAW,
            32,
            212,
            318,
            66,
            hwnd,
            (HMENU)(INT_PTR)IDC_RADIO_MP4,
            nullptr,
            nullptr);

    // MP3
    m_mp3Button =
        CreateWindowW(
            L"BUTTON",
            L"MP3 Audio",
            WS_VISIBLE |
                WS_CHILD |
                BS_OWNERDRAW,
            370,
            212,
            318,
            66,
            hwnd,
            (HMENU)(INT_PTR)IDC_RADIO_MP3,
            nullptr,
            nullptr);

    // ========================================================
    // ACTION BUTTONS
    // ========================================================

    m_downloadButton =
        CreateWindowW(
            L"BUTTON",
            L"Download",
            WS_VISIBLE |
                WS_CHILD |
                BS_OWNERDRAW,
            32,
            304,
            318,
            46,
            hwnd,
            (HMENU)(INT_PTR)IDC_DOWNLOAD_BTN,
            nullptr,
            nullptr);

    m_pauseButton =
        CreateWindowW(
            L"BUTTON",
            L"Pause",
            WS_CHILD |
                BS_OWNERDRAW,
            370,
            304,
            154,
            46,
            hwnd,
            (HMENU)(INT_PTR)IDC_PAUSE_BTN,
            nullptr,
            nullptr);

    m_cancelButton =
        CreateWindowW(
            L"BUTTON",
            L"Cancel",
            WS_CHILD |
                BS_OWNERDRAW,
            534,
            304,
            154,
            46,
            hwnd,
            (HMENU)(INT_PTR)IDC_CANCEL_BTN,
            nullptr,
            nullptr);

    // ========================================================
    // PROGRESS SECTION
    // ========================================================

    makeStatic(
        L"Progress",
        32,
        374,
        100,
        20,
        m_sectionFont);

    // Percentage.
    m_progressPercent =
        makeStatic(
            L"0%",
            624,
            374,
            64,
            20,
            m_smallFont);

    SetWindowLongPtrW(
        m_progressPercent,
        GWL_STYLE,
        GetWindowLongPtrW(
            m_progressPercent,
            GWL_STYLE) |
            SS_RIGHT);

    // Progress bar.
    m_progressBar =
        CreateWindowExW(
            0,
            PROGRESS_CLASSW,
            nullptr,
            WS_VISIBLE |
                WS_CHILD |
                PBS_SMOOTH,
            32,
            402,
            656,
            10,
            hwnd,
            (HMENU)(INT_PTR)IDC_PROGRESS,
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

    SendMessageW(
        m_progressBar,
        PBM_SETBARCOLOR,
        0,
        CLR_RED);

    SendMessageW(
        m_progressBar,
        PBM_SETBKCOLOR,
        0,
        CLR_PROGRESS_BG);

    // ========================================================
    // STATUS
    // ========================================================

    m_statusCaption =
        makeStatic(
            L"Status",
            32,
            430,
            52,
            20,
            m_smallFont);

    // IMPORTANT:
    // The status label is deliberately limited to the area
    // before the percentage. This prevents text overlap.
    m_statusLabel =
        makeStatic(
            L"Ready",
            88,
            430,
            500,
            20,
            m_bodyFont);

    // End-ellipsis for long status messages.
    LONG statusStyle =
        GetWindowLongW(
            m_statusLabel,
            GWL_STYLE);

    statusStyle |=
        SS_ENDELLIPSIS;

    SetWindowLongW(
        m_statusLabel,
        GWL_STYLE,
        statusStyle);

    // Initial state.
    SetFormatSelection(false);

    SetDownloadingState(false);

    ApplyControlFonts();
}

// ============================================================
// APPLY BUTTON FONTS
// ============================================================

void MainWindow::ApplyControlFonts()
{
    auto setFont =
        [this](HWND ctrl)
    {
        if (ctrl)
        {
            SendMessageW(
                ctrl,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    m_buttonFont),
                TRUE);
        }
    };

    setFont(m_downloadButton);
    setFont(m_pauseButton);
    setFont(m_cancelButton);
    setFont(m_mp4Button);
    setFont(m_mp3Button);
}

// ============================================================
// PAINT BACKGROUND
// ============================================================

void MainWindow::PaintBackground(
    HDC hdc,
    const RECT &clientRect)
{
    // --------------------------------------------------------
    // Main background
    // --------------------------------------------------------

    HBRUSH bgBrush =
        CreateSolidBrush(CLR_BG);

    FillRect(
        hdc,
        &clientRect,
        bgBrush);

    DeleteObject(bgBrush);

    // --------------------------------------------------------
    // Header divider
    // --------------------------------------------------------

    RECT divider{
        32,
        82,
        clientRect.right - 32,
        83};

    HBRUSH dividerBrush =
        CreateSolidBrush(CLR_BORDER);

    FillRect(
        hdc,
        &divider,
        dividerBrush);

    DeleteObject(dividerBrush);

    // --------------------------------------------------------
    // Format card background
    // --------------------------------------------------------

    RECT formatSurface{
        24,
        200,
        clientRect.right - 24,
        290};

    FillRounded(
        hdc,
        formatSurface,
        CLR_WHITE,
        CLR_BORDER,
        10,
        1);

    // --------------------------------------------------------
    // Action background
    // --------------------------------------------------------

    RECT actionSurface{
        24,
        294,
        clientRect.right - 24,
        358};

    FillRounded(
        hdc,
        actionSurface,
        CLR_WHITE,
        CLR_BORDER,
        10,
        1);

    // --------------------------------------------------------
    // Progress background
    // --------------------------------------------------------

    RECT progressSurface{
        24,
        364,
        clientRect.right - 24,
        462};

    FillRounded(
        hdc,
        progressSurface,
        CLR_WHITE,
        CLR_BORDER,
        10,
        1);
}

// ============================================================
// FORMAT CARD
// ============================================================

void MainWindow::DrawFormatCard(
    HDC hdc,
    const RECT &rect,
    const wchar_t *title,
    const wchar_t *subtitle,
    bool selected)
{
    const COLORREF fill =
        selected
            ? CLR_RED_LIGHT
            : CLR_WHITE;

    const COLORREF border =
        selected
            ? CLR_RED
            : CLR_BORDER;

    const COLORREF titleColor =
        selected
            ? CLR_RED_DARK
            : CLR_TEXT;

    FillRounded(
        hdc,
        rect,
        fill,
        border,
        9,
        selected ? 2 : 1);

    SetBkMode(
        hdc,
        TRANSPARENT);

    // --------------------------------------------------------
    // Title
    // --------------------------------------------------------

    SetTextColor(
        hdc,
        titleColor);

    SelectObject(
        hdc,
        m_buttonFont);

    RECT titleRect{
        rect.left + 18,
        rect.top + 8,
        rect.right - 55,
        rect.top + 34};

    DrawTextW(
        hdc,
        title,
        -1,
        &titleRect,
        DT_LEFT |
            DT_SINGLELINE |
            DT_VCENTER);

    // --------------------------------------------------------
    // Subtitle
    // --------------------------------------------------------

    SetTextColor(
        hdc,
        CLR_SECONDARY);

    SelectObject(
        hdc,
        m_smallFont);

    RECT subtitleRect{
        rect.left + 18,
        rect.top + 37,
        rect.right - 18,
        rect.bottom - 7};

    DrawTextW(
        hdc,
        subtitle,
        -1,
        &subtitleRect,
        DT_LEFT |
            DT_SINGLELINE |
            DT_VCENTER);

    // --------------------------------------------------------
    // Selected check mark
    // --------------------------------------------------------

    if (selected)
    {
        const int cx =
            rect.right - 25;

        const int cy =
            rect.top + 33;

        HBRUSH brush =
            CreateSolidBrush(CLR_RED);

        HGDIOBJ oldBrush =
            SelectObject(
                hdc,
                brush);

        Ellipse(
            hdc,
            cx - 9,
            cy - 9,
            cx + 9,
            cy + 9);

        SelectObject(
            hdc,
            oldBrush);

        DeleteObject(brush);

        HPEN pen =
            CreatePen(
                PS_SOLID,
                2,
                CLR_WHITE);

        HGDIOBJ oldPen =
            SelectObject(
                hdc,
                pen);

        MoveToEx(
            hdc,
            cx - 4,
            cy,
            nullptr);

        LineTo(
            hdc,
            cx - 1,
            cy + 3);

        LineTo(
            hdc,
            cx + 5,
            cy - 4);

        SelectObject(
            hdc,
            oldPen);

        DeleteObject(pen);
    }
}

// ============================================================
// OWNER DRAW BUTTONS
// ============================================================

void MainWindow::DrawOwnerButton(
    const DRAWITEMSTRUCT *dis)
{
    if (!dis)
        return;

    HDC hdc =
        dis->hDC;

    RECT rc =
        dis->rcItem;

    const bool disabled =
        (dis->itemState & ODS_DISABLED) != 0;

    const bool pressed =
        (dis->itemState & ODS_SELECTED) != 0;

    // --------------------------------------------------------
    // FORMAT CARDS
    // --------------------------------------------------------

    if (dis->CtlID == IDC_RADIO_MP4 ||
        dis->CtlID == IDC_RADIO_MP3)
    {
        const bool selected =
            (dis->CtlID == IDC_RADIO_MP3) == m_selectedMp3;

        DrawFormatCard(
            hdc,
            rc,
            dis->CtlID == IDC_RADIO_MP3
                ? L"MP3 Audio"
                : L"MP4 Video",
            dis->CtlID == IDC_RADIO_MP3
                ? L"Audio only"
                : L"Video with sound",
            selected && !disabled);

        return;
    }

    // --------------------------------------------------------
    // NORMAL BUTTONS
    // --------------------------------------------------------

    COLORREF fill =
        CLR_WHITE;

    COLORREF text =
        CLR_TEXT;

    COLORREF border =
        CLR_BORDER;

    // Download button.
    if (dis->CtlID == IDC_DOWNLOAD_BTN)
    {
        fill =
            disabled
                ? RGB(215, 218, 222)
                : (pressed
                       ? CLR_RED_DARK
                       : CLR_RED);

        text =
            CLR_WHITE;

        border =
            fill;
    }

    // Disabled button.
    else if (disabled)
    {
        fill =
            RGB(238, 239, 242);

        text =
            CLR_DISABLED;

        border =
            RGB(220, 222, 226);
    }

    // Pressed button.
    else if (pressed)
    {
        fill =
            RGB(244, 245, 247);

        border =
            RGB(194, 197, 202);
    }

    FillRounded(
        hdc,
        rc,
        fill,
        border,
        8,
        1);

    wchar_t label[128]{};

    GetWindowTextW(
        dis->hwndItem,
        label,
        127);

    SetBkMode(
        hdc,
        TRANSPARENT);

    SetTextColor(
        hdc,
        text);

    SelectObject(
        hdc,
        m_buttonFont);

    DrawTextW(
        hdc,
        label,
        -1,
        &rc,
        DT_CENTER |
            DT_VCENTER |
            DT_SINGLELINE);

    // Focus rectangle.
    if (dis->itemState & ODS_FOCUS)
    {
        RECT focus =
            rc;

        InflateRect(
            &focus,
            -4,
            -4);

        DrawFocusRect(
            hdc,
            &focus);
    }
}

// ============================================================
// FORMAT SELECTION
// ============================================================

void MainWindow::SetFormatSelection(
    bool mp3)
{
    m_selectedMp3 =
        mp3;

    if (m_mp4Button)
    {
        InvalidateRect(
            m_mp4Button,
            nullptr,
            FALSE);
    }

    if (m_mp3Button)
    {
        InvalidateRect(
            m_mp3Button,
            nullptr,
            FALSE);
    }
}

// ============================================================
// PROGRESS TEXT
// ============================================================

void MainWindow::UpdateProgressText(
    int progress)
{
    progress =
        (std::max)(0,
                   (std::min)(100,
                              progress));

    if (m_progressPercent)
    {
        const std::wstring text =
            std::to_wstring(progress) +
            L"%";

        SetWindowTextW(
            m_progressPercent,
            text.c_str());
    }
}

// ============================================================
// DOWNLOADING STATE
// ============================================================

void MainWindow::SetDownloadingState(
    bool downloading)
{
    m_isPaused =
        false;

    EnableWindow(
        m_urlEdit,
        !downloading);

    EnableWindow(
        m_mp4Button,
        !downloading);

    EnableWindow(
        m_mp3Button,
        !downloading);

    EnableWindow(
        m_downloadButton,
        !downloading);

    if (downloading)
    {
        ShowWindow(
            m_pauseButton,
            SW_SHOW);

        ShowWindow(
            m_cancelButton,
            SW_SHOW);

        SetWindowTextW(
            m_pauseButton,
            L"Pause");

        EnableWindow(
            m_pauseButton,
            TRUE);

        EnableWindow(
            m_cancelButton,
            TRUE);

        m_cancelPending =
            false;
    }
    else
    {
        ShowWindow(
            m_pauseButton,
            SW_HIDE);

        ShowWindow(
            m_cancelButton,
            SW_HIDE);

        m_cancelPending =
            false;
    }

    InvalidateRect(
        m_hwnd,
        nullptr,
        FALSE);
}

// ============================================================
// PLAYLIST DETECTION
// ============================================================

int MainWindow::ResolvePlaylistChoice(
    HWND hwnd,
    const std::wstring &url)
{
    const bool hasList =
        url.find(L"list=") != std::wstring::npos;

    const bool hasVideo =
        url.find(L"v=") != std::wstring::npos;

    if (!hasList)
        return 0;

    if (!hasVideo)
        return 1;

    const int result =
        MessageBoxW(
            hwnd,
            L"This URL contains a playlist.\n\n"
            L"Yes: download the entire playlist\n"
            L"No: download this video only",
            L"Playlist detected",
            MB_YESNOCANCEL |
                MB_ICONQUESTION);

    if (result == IDYES)
        return 1;

    if (result == IDNO)
        return 0;

    return -1;
}

// ============================================================
// DOWNLOAD CLICKED
// ============================================================

void MainWindow::OnDownloadClicked(
    HWND hwnd)
{
    wchar_t urlBuffer[4096]{};

    GetWindowTextW(
        m_urlEdit,
        urlBuffer,
        static_cast<int>(
            sizeof(urlBuffer) /
            sizeof(urlBuffer[0])));

    std::wstring url =
        urlBuffer;

    if (url.empty())
    {
        MessageBoxW(
            hwnd,
            L"Please enter a video or playlist URL.",
            L"IT Downloader V2",
            MB_OK |
                MB_ICONINFORMATION);

        SetFocus(
            m_urlEdit);

        return;
    }

    const int choice =
        ResolvePlaylistChoice(
            hwnd,
            url);

    if (choice == -1)
        return;

    StartDownloadWithParams(
        hwnd,
        url,
        m_selectedMp3,
        choice == 1);
}

// ============================================================
// START DOWNLOAD
// ============================================================

void MainWindow::StartDownloadWithParams(
    HWND hwnd,
    const std::wstring &url,
    bool isMp3,
    bool isPlaylist)
{
    if (DownloadManager::StartDownload(
            hwnd,
            url,
            isMp3,
            isPlaylist))
    {
        m_lastUrl =
            url;

        m_lastIsMp3 =
            isMp3;

        m_lastIsPlaylist =
            isPlaylist;

        SendMessageW(
            m_progressBar,
            PBM_SETPOS,
            0,
            0);

        UpdateProgressText(0);

        SetWindowTextW(
            m_statusLabel,
            L"Starting download...");

        SetDownloadingState(
            true);
    }
}

// ============================================================
// CANCEL
// ============================================================

void MainWindow::OnCancelClicked()
{
    if (m_cancelPending)
        return;

    m_cancelPending =
        true;

    DownloadManager::CancelDownload();

    EnableWindow(
        m_cancelButton,
        FALSE);

    EnableWindow(
        m_pauseButton,
        FALSE);

    SetWindowTextW(
        m_statusLabel,
        L"Cancelling...");
}

// ============================================================
// PAUSE / RESUME
// ============================================================

void MainWindow::OnPauseResumeClicked(
    HWND hwnd)
{
    if (!m_isPaused)
    {
        DownloadManager::PauseDownload();

        EnableWindow(
            m_pauseButton,
            FALSE);

        SetWindowTextW(
            m_statusLabel,
            L"Pausing...");
    }
    else
    {
        m_isPaused =
            false;

        StartDownloadWithParams(
            hwnd,
            m_lastUrl,
            m_lastIsMp3,
            m_lastIsPlaylist);
    }
}