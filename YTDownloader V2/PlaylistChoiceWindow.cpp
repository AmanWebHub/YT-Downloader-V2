#include "PlaylistChoiceWindow.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

namespace
{
    const wchar_t CLASS_NAME[] =
        L"ITDownloaderV2PlaylistChoiceWindow";

    constexpr int IDC_PC_PLAYLIST = 3002;
    constexpr int IDC_PC_VIDEO    = 3003;
    constexpr int IDC_PC_CANCEL   = 3004;

    // Same palette as CompletionWindow, so this reads as part of the
    // same app rather than a bolted-on dialog.
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

    // Owner-drawn "card" buttons carry two lines of text separated
    // by '\n': a bold title and a smaller description underneath.
    void SplitCardText(
        const wchar_t* text,
        std::wstring& title,
        std::wstring& description)
    {
        const wchar_t* newline =
            wcschr(text, L'\n');

        if (newline)
        {
            title.assign(
                text,
                newline);

            description.assign(
                newline + 1);
        }
        else
        {
            title = text;
            description.clear();
        }
    }
}

PlaylistChoiceWindow::Choice PlaylistChoiceWindow::Show(
    HINSTANCE hInstance,
    HWND owner,
    const std::wstring& /*url*/)
{
    PlaylistChoiceWindow* self =
        new PlaylistChoiceWindow();

    self->m_owner =
        owner;

    Choice result =
        Choice::Cancelled;

    self->m_resultOut =
        &result;

    WNDCLASSW wc{};

    wc.lpfnWndProc =
        PlaylistChoiceWindow::WindowProcStatic;

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
            L"Playlist detected",
            WS_OVERLAPPED |
            WS_CAPTION |
            WS_SYSMENU,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            620,
            420,
            nullptr,
            nullptr,
            hInstance,
            self);

    if (!self->m_hwnd)
    {
        delete self;
        return Choice::Cancelled;
    }

    const HWND hwnd =
        self->m_hwnd;

    if (owner)
    {
        EnableWindow(
            owner,
            FALSE);
    }

    RECT rc{};

    GetWindowRect(
        hwnd,
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
        hwnd,
        nullptr,
        (sw - w) / 2,
        (sh - h) / 2,
        0,
        0,
        SWP_NOSIZE |
        SWP_NOZORDER);

    ShowWindow(
        hwnd,
        SW_SHOW);

    UpdateWindow(
        hwnd);

    SetForegroundWindow(
        hwnd);

    // Blocking loop: pumps this thread's messages (so the owner
    // window can still repaint itself while disabled) until this
    // specific window is destroyed. `self` may already be freed by
    // that point (WM_NCDESTROY deletes it), so only IsWindow(hwnd)
    // is checked here - never self - and the result was already
    // written into `result` via m_resultOut before that happened.
    MSG msg{};

    while (IsWindow(hwnd) &&
        GetMessageW(
            &msg,
            nullptr,
            0,
            0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (owner)
    {
        EnableWindow(
            owner,
            TRUE);

        SetForegroundWindow(
            owner);
    }

    return result;
}

LRESULT CALLBACK PlaylistChoiceWindow::WindowProcStatic(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    PlaylistChoiceWindow* self =
        nullptr;

    if (uMsg == WM_NCCREATE)
    {
        const CREATESTRUCTW* cs =
            reinterpret_cast<
                const CREATESTRUCTW*>(
                    lParam);

        self =
            reinterpret_cast<
                PlaylistChoiceWindow*>(
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
                PlaylistChoiceWindow*>(
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

LRESULT PlaylistChoiceWindow::HandleMessage(
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

        SetBkMode(
            hdc,
            TRANSPARENT);

        SetTextColor(
            hdc,
            CLR_SECONDARY);

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
            case IDC_PC_PLAYLIST:
                Finish(Choice::EntirePlaylist);
                return 0;

            case IDC_PC_VIDEO:
                Finish(Choice::SingleVideo);
                return 0;

            case IDC_PC_CANCEL:
                Finish(Choice::Cancelled);
                return 0;

            default:
                break;
            }
        }

        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            Finish(Choice::Cancelled);
            return 0;
        }

        return 0;

    case WM_CLOSE:
        Finish(Choice::Cancelled);
        return 0;

    case WM_NCDESTROY:
    {
        if (m_resultOut)
        {
            *m_resultOut = m_choice;
        }

        DeleteFont(m_titleFont);
        DeleteFont(m_bodyFont);
        DeleteFont(m_cardTitleFont);
        DeleteFont(m_cardBodyFont);
        DeleteFont(m_cancelFont);

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

void PlaylistChoiceWindow::Finish(Choice choice)
{
    m_choice = choice;

    DestroyWindow(m_hwnd);
}

void PlaylistChoiceWindow::CreateControls(
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

    m_cardTitleFont =
        MakeFont(
            16,
            FW_SEMIBOLD);

    m_cardBodyFont =
        MakeFont(
            13,
            FW_NORMAL);

    m_cancelFont =
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
        L"Playlist detected",
        36,
        24,
        540,
        32,
        m_titleFont);

    makeStatic(
        L"This link points to a video that's part of a playlist. "
        L"What would you like to download?",
        38,
        58,
        544,
        40,
        m_bodyFont);

    auto makeCard =
        [&](const wchar_t* title,
            const wchar_t* description,
            int id,
            int x,
            int y,
            int w,
            int h) -> HWND
    {
        const std::wstring combined =
            std::wstring(title) +
            L"\n" +
            description;

        HWND button =
            CreateWindowW(
                L"BUTTON",
                combined.c_str(),
                WS_VISIBLE |
                WS_CHILD |
                BS_OWNERDRAW,
                x,
                y,
                w,
                h,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(id)),
                nullptr,
                nullptr);

        return button;
    };

    constexpr int cardX = 28;
    constexpr int cardWidth = 556;
    constexpr int cardHeight = 96;
    constexpr int cardGap = 14;

    const int card1Y = 112;
    const int card2Y = card1Y + cardHeight + cardGap;

    makeCard(
        L"Download Entire Playlist",
        L"Downloads every video in this playlist as separate files.",
        IDC_PC_PLAYLIST,
        cardX,
        card1Y,
        cardWidth,
        cardHeight);

    makeCard(
        L"Download This Video Only",
        L"Downloads just this one video and skips the rest of the playlist.",
        IDC_PC_VIDEO,
        cardX,
        card2Y,
        cardWidth,
        cardHeight);

    HWND cancelButton =
        CreateWindowW(
            L"BUTTON",
            L"Cancel",
            WS_VISIBLE |
            WS_CHILD |
            BS_OWNERDRAW,
            cardX,
            card2Y + cardHeight + 20,
            140,
            36,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(IDC_PC_CANCEL)),
            nullptr,
            nullptr);

    SendMessageW(
        cancelButton,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(
            m_cancelFont),
        TRUE);
}

void PlaylistChoiceWindow::DrawOwnerButton(
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

    wchar_t rawText[256]{};

    GetWindowTextW(
        dis->hwndItem,
        rawText,
        255);

    // ---------------------------------------------------------------
    // Cancel: a small plain outlined button, same treatment as any
    // other secondary action elsewhere in the app.
    // ---------------------------------------------------------------
    if (dis->CtlID ==
        IDC_PC_CANCEL)
    {
        const COLORREF fill =
            pressed
                ? RGB(244, 245, 247)
                : CLR_WHITE;

        FillRounded(
            hdc,
            rc,
            fill,
            CLR_BORDER,
            8,
            1);

        SetBkMode(
            hdc,
            TRANSPARENT);

        SetTextColor(
            hdc,
            CLR_SECONDARY);

        HGDIOBJ oldFont =
            SelectObject(
                hdc,
                m_cancelFont);

        DrawTextW(
            hdc,
            rawText,
            -1,
            &rc,
            DT_CENTER |
            DT_VCENTER |
            DT_SINGLELINE);

        SelectObject(
            hdc,
            oldFont);

        return;
    }

    // ---------------------------------------------------------------
    // Option cards: a title line plus a description line. The
    // "Entire Playlist" card is the visually primary choice (filled
    // red), the "This Video Only" card is secondary (outlined),
    // mirroring how the completion window distinguishes its primary
    // and secondary actions.
    // ---------------------------------------------------------------
    const bool isPrimary =
        dis->CtlID ==
        IDC_PC_PLAYLIST;

    COLORREF fill =
        CLR_WHITE;

    COLORREF border =
        CLR_BORDER;

    COLORREF titleColor =
        CLR_TEXT;

    COLORREF descColor =
        CLR_SECONDARY;

    if (isPrimary)
    {
        fill =
            pressed
                ? CLR_RED_DARK
                : CLR_RED;

        border =
            fill;

        titleColor =
            CLR_WHITE;

        descColor =
            RGB(255, 226, 226);
    }
    else
    {
        fill =
            pressed
                ? CLR_RED_LIGHT
                : CLR_WHITE;

        border =
            pressed
                ? CLR_RED
                : CLR_BORDER;
    }

    FillRounded(
        hdc,
        rc,
        fill,
        border,
        10,
        1);

    std::wstring title;
    std::wstring description;

    SplitCardText(
        rawText,
        title,
        description);

    SetBkMode(
        hdc,
        TRANSPARENT);

    RECT titleRect =
        rc;

    titleRect.left += 24;
    titleRect.right -= 24;
    titleRect.top += 18;
    titleRect.bottom =
        titleRect.top + 24;

    SetTextColor(
        hdc,
        titleColor);

    HGDIOBJ oldFont =
        SelectObject(
            hdc,
            m_cardTitleFont);

    DrawTextW(
        hdc,
        title.c_str(),
        -1,
        &titleRect,
        DT_LEFT |
        DT_VCENTER |
        DT_SINGLELINE);

    RECT descRect =
        rc;

    descRect.left += 24;
    descRect.right -= 24;
    descRect.top =
        titleRect.bottom + 4;

    descRect.bottom =
        rc.bottom - 14;

    SelectObject(
        hdc,
        m_cardBodyFont);

    SetTextColor(
        hdc,
        descColor);

    DrawTextW(
        hdc,
        description.c_str(),
        -1,
        &descRect,
        DT_LEFT |
        DT_TOP |
        DT_WORDBREAK);

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

void PlaylistChoiceWindow::PaintBackground(
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
        100,
        rc.right - 36,
        102
    };

    HBRUSH accentBrush =
        CreateSolidBrush(
            CLR_BORDER);

    FillRect(
        hdc,
        &accent,
        accentBrush);

    DeleteObject(
        accentBrush);
}
