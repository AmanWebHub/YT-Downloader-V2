#include "CompletionWindow.h"

#include <shellapi.h>
#include <shlobj.h>

#pragma comment(lib, "shell32.lib")

namespace
{
    const wchar_t CLASS_NAME[] =
        L"ITDownloaderV2CompletionWindow";

    constexpr int IDC_COMP_OPEN        = 2002;
    constexpr int IDC_COMP_OPEN_WITH   = 2003;
    constexpr int IDC_COMP_OPEN_FOLDER = 2004;
    constexpr int IDC_COMP_DONE        = 2005;

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

    constexpr COLORREF CLR_DISABLED =
        RGB(170, 174, 181);

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

    void DeleteFont(HFONT& font)
    {
        if (font)
        {
            DeleteObject(font);
            font = nullptr;
        }
    }

    void FillRounded(
        HDC hdc,
        const RECT& rc,
        COLORREF fill,
        COLORREF border,
        int radius = 10,
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
            SelectObject(
                hdc,
                brush);

        HGDIOBJ oldPen =
            SelectObject(
                hdc,
                pen);

        RoundRect(
            hdc,
            rc.left,
            rc.top,
            rc.right,
            rc.bottom,
            radius,
            radius);

        SelectObject(
            hdc,
            oldPen);

        SelectObject(
            hdc,
            oldBrush);

        DeleteObject(pen);
        DeleteObject(brush);
    }
}

CompletionWindow* CompletionWindow::Create(
    HINSTANCE hInstance,
    HWND ownerToRestore,
    const std::wstring& filePath,
    bool isPlaylist)
{
    CompletionWindow* self =
        new CompletionWindow();

    self->m_ownerToRestore =
        ownerToRestore;

    self->m_filePath =
        filePath;

    self->m_isPlaylist =
        isPlaylist;

    // Determine whether the supplied completion path is a folder.
    DWORD attributes =
        GetFileAttributesW(
            filePath.c_str());

    self->m_isFolderOnly =
        attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

    // A playlist is always treated as a folder-style completion.
    //
    // This is the important part for MP3 playlists:
    // DownloadWorker may report the final MP3 file as resolvedFilePath,
    // but the playlist flag tells us that the completion represents
    // multiple downloaded items.
    if (self->m_isPlaylist)
    {
        self->m_isFolderOnly = true;
    }

    WNDCLASSW wc{};

    wc.lpfnWndProc =
        CompletionWindow::WindowProcStatic;

    wc.hInstance =
        hInstance;

    wc.lpszClassName =
        CLASS_NAME;

    wc.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW);

    wc.hbrBackground =
        nullptr;

    static bool registered = false;

    if (!registered)
    {
        if (RegisterClassW(&wc) ||
            GetLastError() ==
                ERROR_CLASS_ALREADY_EXISTS)
        {
            registered = true;
        }
    }

    self->m_hwnd =
        CreateWindowExW(
            0,
            CLASS_NAME,
            L"Download finished",
            WS_OVERLAPPED |
            WS_CAPTION |
            WS_SYSMENU,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            620,
            300,
            nullptr,
            nullptr,
            hInstance,
            self);

    if (!self->m_hwnd)
    {
        delete self;
        return nullptr;
    }

    if (ownerToRestore)
    {
        ShowWindow(
            ownerToRestore,
            SW_HIDE);
    }

    RECT rc{};

    GetWindowRect(
        self->m_hwnd,
        &rc);

    const int w =
        rc.right - rc.left;

    const int h =
        rc.bottom - rc.top;

    const int sw =
        GetSystemMetrics(
            SM_CXSCREEN);

    const int sh =
        GetSystemMetrics(
            SM_CYSCREEN);

    SetWindowPos(
        self->m_hwnd,
        nullptr,
        (sw - w) / 2,
        (sh - h) / 2,
        0,
        0,
        SWP_NOSIZE |
        SWP_NOZORDER);

    ShowWindow(
        self->m_hwnd,
        SW_SHOW);

    UpdateWindow(
        self->m_hwnd);

    SetForegroundWindow(
        self->m_hwnd);

    return self;
}

LRESULT CALLBACK CompletionWindow::WindowProcStatic(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    CompletionWindow* self =
        nullptr;

    if (uMsg == WM_NCCREATE)
    {
        const CREATESTRUCTW* cs =
            reinterpret_cast<
                const CREATESTRUCTW*>(
                    lParam);

        self =
            reinterpret_cast<
                CompletionWindow*>(
                    cs->lpCreateParams);

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(
                self));
    }
    else
    {
        self =
            reinterpret_cast<
                CompletionWindow*>(
                    GetWindowLongPtrW(
                        hwnd,
                        GWLP_USERDATA));
    }

    if (self)
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

LRESULT CompletionWindow::HandleMessage(
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
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};

        HDC hdc =
            BeginPaint(
                hwnd,
                &ps);

        RECT rc{};

        GetClientRect(
            hwnd,
            &rc);

        PaintBackground(
            hdc,
            rc);

        EndPaint(
            hwnd,
            &ps);

        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc =
            reinterpret_cast<HDC>(
                wParam);

        HWND control =
            reinterpret_cast<HWND>(
                lParam);

        SetBkMode(
            hdc,
            TRANSPARENT);

        if (control ==
            m_statusLabel)
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

        return reinterpret_cast<LRESULT>(
            GetStockObject(
                NULL_BRUSH));
    }

    case WM_DRAWITEM:
        DrawOwnerButton(
            reinterpret_cast<
                const DRAWITEMSTRUCT*>(
                    lParam));

        return TRUE;

    case WM_COMMAND:
        if (HIWORD(wParam) ==
            BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IDC_COMP_OPEN:
                OnOpenClicked();
                return 0;

            case IDC_COMP_OPEN_WITH:
                OnOpenWithClicked();
                return 0;

            case IDC_COMP_OPEN_FOLDER:
                OnOpenFolderClicked();
                return 0;

            case IDC_COMP_DONE:
                DestroyWindow(hwnd);
                return 0;

            default:
                break;
            }
        }

        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_NCDESTROY:
    {
        if (m_ownerToRestore)
        {
            ShowWindow(
                m_ownerToRestore,
                SW_SHOW);

            SetForegroundWindow(
                m_ownerToRestore);
        }

        DeleteFont(m_titleFont);
        DeleteFont(m_bodyFont);
        DeleteFont(m_smallFont);
        DeleteFont(m_buttonFont);

        delete this;

        return 0;
    }
    }

    return DefWindowProcW(
        hwnd,
        uMsg,
        wParam,
        lParam);
}

void CompletionWindow::CreateControls(
    HWND hwnd)
{
    m_titleFont =
        MakeFont(
            24,
            FW_SEMIBOLD);

    m_bodyFont =
        MakeFont(
            14,
            FW_NORMAL);

    m_smallFont =
        MakeFont(
            12,
            FW_NORMAL);

    m_buttonFont =
        MakeFont(
            13,
            FW_SEMIBOLD);

    auto makeStatic =
        [&](const wchar_t* text,
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
            reinterpret_cast<WPARAM>(
                font),
            TRUE);

        return ctrl;
    };

    makeStatic(
        L"Download finished",
        36,
        24,
        500,
        32,
        m_titleFont);

    m_statusLabel =
        makeStatic(
            m_isPlaylist
                ? L"Your playlist downloads are ready."
                : L"Your file is ready.",
            38,
            58,
            540,
            20,
            m_smallFont);

    m_pathLabel =
        makeStatic(
            m_filePath.c_str(),
            50,
            112,
            518,
            52,
            m_bodyFont);

    LONG pathStyle =
        GetWindowLongW(
            m_pathLabel,
            GWL_STYLE);

    pathStyle |=
        SS_PATHELLIPSIS |
        SS_ENDELLIPSIS;

    SetWindowLongW(
        m_pathLabel,
        GWL_STYLE,
        pathStyle);

    auto makeButton =
        [&](const wchar_t* text,
            int id,
            int x,
            int y,
            int w) -> HWND
    {
        HWND button =
            CreateWindowW(
                L"BUTTON",
                text,
                WS_VISIBLE |
                WS_CHILD |
                BS_OWNERDRAW,
                x,
                y,
                w,
                44,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(id)),
                nullptr,
                nullptr);

        SendMessageW(
            button,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                m_buttonFont),
            TRUE);

        return button;
    };

    constexpr int buttonY = 190;
    constexpr int gap = 12;

    // ---------------------------------------------------------------
    // Single download
    //
    // Open | Open With... | Open Folder | Done
    // ---------------------------------------------------------------
    if (!m_isPlaylist)
    {
        constexpr int buttonWidth = 132;
        constexpr int left = 28;

        const int x1 =
            left;

        const int x2 =
            x1 + buttonWidth + gap;

        const int x3 =
            x2 + buttonWidth + gap;

        const int x4 =
            x3 + buttonWidth + gap;

        makeButton(
            L"Open",
            IDC_COMP_OPEN,
            x1,
            buttonY,
            buttonWidth);

        makeButton(
            L"Open With...",
            IDC_COMP_OPEN_WITH,
            x2,
            buttonY,
            buttonWidth);

        makeButton(
            L"Open Folder",
            IDC_COMP_OPEN_FOLDER,
            x3,
            buttonY,
            buttonWidth);

        makeButton(
            L"Done",
            IDC_COMP_DONE,
            x4,
            buttonY,
            buttonWidth);
    }
    // ---------------------------------------------------------------
    // Playlist
    //
    // Open Folder | Done
    // ---------------------------------------------------------------
    else
    {
        constexpr int buttonWidth = 274;

        makeButton(
            L"Open Folder",
            IDC_COMP_OPEN_FOLDER,
            28,
            buttonY,
            buttonWidth);

        makeButton(
            L"Done",
            IDC_COMP_DONE,
            318,
            buttonY,
            buttonWidth);
    }
}

void CompletionWindow::DrawOwnerButton(
    const DRAWITEMSTRUCT* dis)
{
    if (!dis)
        return;

    HDC hdc =
        dis->hDC;

    RECT rc =
        dis->rcItem;

    const bool pressed =
        (dis->itemState &
         ODS_SELECTED) != 0;

    const bool disabled =
        (dis->itemState &
         ODS_DISABLED) != 0;

    COLORREF fill =
        CLR_WHITE;

    COLORREF text =
        CLR_TEXT;

    COLORREF border =
        CLR_BORDER;

    if (dis->CtlID ==
        IDC_COMP_OPEN)
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
    else if (dis->CtlID ==
             IDC_COMP_OPEN_FOLDER)
    {
        fill =
            disabled
                ? RGB(238, 239, 242)
                : (pressed
                    ? CLR_RED_LIGHT
                    : CLR_WHITE);

        text =
            disabled
                ? CLR_DISABLED
                : CLR_RED_DARK;

        border =
            disabled
                ? RGB(220, 222, 226)
                : CLR_RED;
    }
    else if (disabled)
    {
        fill =
            RGB(238, 239, 242);

        text =
            CLR_DISABLED;

        border =
            RGB(220, 222, 226);
    }
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

    HGDIOBJ oldFont =
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

    SelectObject(
        hdc,
        oldFont);

    if (dis->itemState &
        ODS_FOCUS)
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

void CompletionWindow::PaintBackground(
    HDC hdc,
    const RECT& rc)
{
    HBRUSH bgBrush =
        CreateSolidBrush(
            CLR_BG);

    FillRect(
        hdc,
        &rc,
        bgBrush);

    DeleteObject(
        bgBrush);

    RECT accent{
        36,
        82,
        rc.right - 36,
        84
    };

    HBRUSH accentBrush =
        CreateSolidBrush(
            CLR_RED);

    FillRect(
        hdc,
        &accent,
        accentBrush);

    DeleteObject(
        accentBrush);

    RECT pathCard{
        28,
        94,
        rc.right - 28,
        168
    };

    FillRounded(
        hdc,
        pathCard,
        CLR_WHITE,
        CLR_BORDER,
        10,
        1);

    HBRUSH dot =
        CreateSolidBrush(
            CLR_RED);

    HGDIOBJ oldBrush =
        SelectObject(
            hdc,
            dot);

    Ellipse(
        hdc,
        37,
        112,
        49,
        124);

    SelectObject(
        hdc,
        oldBrush);

    DeleteObject(
        dot);
}

void CompletionWindow::OnOpenClicked()
{
    ShellExecuteW(
        m_hwnd,
        L"open",
        m_filePath.c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL);
}

void CompletionWindow::OnOpenWithClicked()
{
    OPENASINFO info{};

    info.pcszFile =
        m_filePath.c_str();

    info.pcszClass =
        nullptr;

    info.oaifInFlags =
        OAIF_EXEC |
        OAIF_ALLOW_REGISTRATION;

    const HRESULT hr =
        SHOpenWithDialog(
            m_hwnd,
            &info);

    if (FAILED(hr) &&
        hr != HRESULT_FROM_WIN32(
            ERROR_CANCELLED))
    {
        MessageBoxW(
            m_hwnd,
            L"Could not open the Open With dialog.",
            L"IT Downloader V2",
            MB_OK |
            MB_ICONERROR);
    }
}

void CompletionWindow::OnOpenFolderClicked()
{
    if (m_isPlaylist ||
        m_isFolderOnly)
    {
        // For playlists, the MainWindow should pass the
        // downloads folder. This opens that folder directly.
        ShellExecuteW(
            m_hwnd,
            L"open",
            m_filePath.c_str(),
            nullptr,
            nullptr,
            SW_SHOWNORMAL);

        return;
    }

    const std::wstring args =
        L"/select,\""
        + m_filePath
        + L"\"";

    ShellExecuteW(
        m_hwnd,
        L"open",
        L"explorer.exe",
        args.c_str(),
        nullptr,
        SW_SHOWNORMAL);
}