#include "CompletionWindow.h"
#include <shellapi.h>
#include <shlobj.h>
#include <algorithm>

#pragma comment(lib, "shell32.lib")

namespace
{
    const wchar_t CLASS_NAME[] = L"ITDownloaderV2CompletionWindow";
    constexpr int IDC_COMP_PATH_LABEL  = 2001;
    constexpr int IDC_COMP_OPEN_FILE   = 2002;
    constexpr int IDC_COMP_OPEN_WITH   = 2003;
    constexpr int IDC_COMP_OPEN_FOLDER = 2004;
    constexpr int IDC_COMP_CLOSE       = 2005;

    constexpr COLORREF CLR_BG     = RGB(248, 248, 248);
    constexpr COLORREF CLR_TEXT   = RGB(24, 24, 24);
    constexpr COLORREF CLR_MUTED  = RGB(105, 105, 105);
    constexpr COLORREF CLR_BORDER = RGB(220, 220, 220);
    constexpr COLORREF CLR_RED    = RGB(220, 38, 38);

    HFONT MakeFont(int height, int weight)
    {
        return CreateFontW(
            height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    // Find the newest file in a folder with a given extension (case-insensitive)
    std::wstring FindNewestFileInFolder(const std::wstring& folder, const std::wstring& extension)
    {
        if (folder.empty()) return L"";
        std::wstring search = folder + L"\\*" + extension;
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) return L"";

        std::wstring bestPath;
        ULARGE_INTEGER bestTime;
        bestTime.QuadPart = 0;

        do
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;

            ULARGE_INTEGER ft;
            ft.LowPart = fd.ftLastWriteTime.dwLowDateTime;
            ft.HighPart = fd.ftLastWriteTime.dwHighDateTime;

            if (ft.QuadPart > bestTime.QuadPart)
            {
                bestTime = ft;
                bestPath = folder + L"\\" + fd.cFileName;
            }
        } while (FindNextFileW(hFind, &fd));

        FindClose(hFind);
        return bestPath;
    }
}

CompletionWindow* CompletionWindow::Create(HINSTANCE hInstance,
                                           HWND ownerToRestore,
                                           const std::wstring& filePath)
{
    auto* self = new CompletionWindow();
    self->m_ownerToRestore = ownerToRestore;
    self->m_filePath = filePath;

    // Determine if the path is a folder
    DWORD attrs = GetFileAttributesW(filePath.c_str());
    bool isFolder = (attrs != INVALID_FILE_ATTRIBUTES &&
                     (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0);

    // If it's a folder, try to find the newest MP4 file in it
    if (isFolder)
    {
        std::wstring foundFile = FindNewestFileInFolder(filePath, L".mp4");
        if (!foundFile.empty())
        {
            self->m_filePath = foundFile;
            isFolder = false; // we now have a file
        }
    }

    self->m_isFolderOnly = isFolder; // true if still a folder (no file found)

    WNDCLASSW wc{};
    wc.lpfnWndProc = CompletionWindow::WindowProcStatic;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;

    static bool registered = false;
    if (!registered)
    {
        RegisterClassW(&wc);
        registered = true;
    }

    self->m_hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        CLASS_NAME,
        L"Download Complete",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        480, 260,
        ownerToRestore, nullptr, hInstance, self);

    if (!self->m_hwnd)
    {
        delete self;
        return nullptr;
    }

    // Center the window
    RECT rc;
    GetWindowRect(self->m_hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(self->m_hwnd, nullptr,
                 (sw - w) / 2, (sh - h) / 2,
                 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    if (ownerToRestore)
        ShowWindow(ownerToRestore, SW_HIDE);

    ShowWindow(self->m_hwnd, SW_SHOW);
    UpdateWindow(self->m_hwnd);
    SetForegroundWindow(self->m_hwnd);
    return self;
}

LRESULT CALLBACK CompletionWindow::WindowProcStatic(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CompletionWindow* self = nullptr;
    if (uMsg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<CompletionWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<CompletionWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return self ? self->HandleMessage(hwnd, uMsg, wParam, lParam)
                : DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT CompletionWindow::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        Paint(hdc, rc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DRAWITEM:
        DrawButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        return TRUE;

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IDC_COMP_OPEN_FILE:   OnOpenFileClicked(); return 0;
            case IDC_COMP_OPEN_WITH:   OnOpenWithClicked(); return 0;
            case IDC_COMP_OPEN_FOLDER: OnOpenFolderClicked(); return 0;
            case IDC_COMP_CLOSE:       DestroyWindow(hwnd); return 0;
            }
        }
        break;

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_TEXT);
        return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    }

    case WM_NCDESTROY:
        if (m_ownerToRestore)
        {
            ShowWindow(m_ownerToRestore, SW_SHOW);
            SetForegroundWindow(m_ownerToRestore);
        }
        if (m_titleFont) DeleteObject(m_titleFont);
        if (m_bodyFont) DeleteObject(m_bodyFont);
        if (m_buttonFont) DeleteObject(m_buttonFont);
        delete this;
        return 0;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void CompletionWindow::CreateControls(HWND hwnd)
{
    m_titleFont  = MakeFont(22, FW_SEMIBOLD);
    m_bodyFont   = MakeFont(13, FW_NORMAL);
    m_buttonFont = MakeFont(12, FW_SEMIBOLD);

    m_pathLabel = CreateWindowW(
        L"STATIC", m_filePath.c_str(),
        WS_VISIBLE | WS_CHILD | SS_PATHELLIPSIS,
        30, 88, 420, 32,
        hwnd, (HMENU)(INT_PTR)IDC_COMP_PATH_LABEL, nullptr, nullptr);
    SendMessageW(m_pathLabel, WM_SETFONT, reinterpret_cast<WPARAM>(m_bodyFont), TRUE);

    const int buttonY = 140;

    if (!m_isFolderOnly)
    {
        // Four buttons: Open File, Open With, Open Folder, Close
        CreateWindowW(L"BUTTON", L"Open File",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            30, buttonY, 95, 38,
            hwnd, (HMENU)(INT_PTR)IDC_COMP_OPEN_FILE, nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"Open With...",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            135, buttonY, 95, 38,
            hwnd, (HMENU)(INT_PTR)IDC_COMP_OPEN_WITH, nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"Open Folder",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            240, buttonY, 95, 38,
            hwnd, (HMENU)(INT_PTR)IDC_COMP_OPEN_FOLDER, nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"Close",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            345, buttonY, 95, 38,
            hwnd, (HMENU)(INT_PTR)IDC_COMP_CLOSE, nullptr, nullptr);
    }
    else
    {
        // Folder-only: Open Folder and Close
        CreateWindowW(L"BUTTON", L"Open Folder",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            30, buttonY, 150, 38,
            hwnd, (HMENU)(INT_PTR)IDC_COMP_OPEN_FOLDER, nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"Close",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            300, buttonY, 150, 38,
            hwnd, (HMENU)(INT_PTR)IDC_COMP_CLOSE, nullptr, nullptr);
    }

    HWND buttons[] = {
        GetDlgItem(hwnd, IDC_COMP_OPEN_FILE),
        GetDlgItem(hwnd, IDC_COMP_OPEN_WITH),
        GetDlgItem(hwnd, IDC_COMP_OPEN_FOLDER),
        GetDlgItem(hwnd, IDC_COMP_CLOSE)
    };
    for (HWND btn : buttons)
        if (btn) SendMessageW(btn, WM_SETFONT, reinterpret_cast<WPARAM>(m_buttonFont), TRUE);
}

void CompletionWindow::Paint(HDC hdc, const RECT& rc)
{
    HBRUSH bg = CreateSolidBrush(CLR_BG);
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_TEXT);
    SelectObject(hdc, m_titleFont);
    RECT titleRect{ 30, 28, rc.right - 30, 56 };
    DrawTextW(hdc, L"Download Complete", -1, &titleRect,
              DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    SetTextColor(hdc, CLR_MUTED);
    SelectObject(hdc, m_bodyFont);
    RECT subRect{ 30, 60, rc.right - 30, 82 };
    DrawTextW(hdc, L"Your download is ready.", -1, &subRect,
              DT_CENTER | DT_SINGLELINE | DT_VCENTER);
}

void CompletionWindow::DrawButton(const DRAWITEMSTRUCT* dis)
{
    if (!dis) return;

    const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    const bool primary = (dis->CtlID == IDC_COMP_OPEN_FILE);

    COLORREF fill, text, border;
    if (primary)
    {
        fill = pressed ? RGB(185, 28, 28) : CLR_RED;
        text = RGB(255, 255, 255);
        border = fill;
    }
    else
    {
        fill = pressed ? RGB(240, 240, 240) : RGB(255, 255, 255);
        text = CLR_TEXT;
        border = pressed ? RGB(180, 180, 180) : CLR_BORDER;
    }

    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(dis->hDC, brush);
    HGDIOBJ oldPen = SelectObject(dis->hDC, pen);
    RoundRect(dis->hDC,
              dis->rcItem.left + 1, dis->rcItem.top + 1,
              dis->rcItem.right - 1, dis->rcItem.bottom - 1,
              6, 6);
    SelectObject(dis->hDC, oldPen);
    SelectObject(dis->hDC, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    wchar_t label[64];
    GetWindowTextW(dis->hwndItem, label, 64);
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, text);
    SelectObject(dis->hDC, m_buttonFont);
    DrawTextW(dis->hDC, label, -1, const_cast<RECT*>(&dis->rcItem),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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
        MessageBoxW(m_hwnd,
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
        // Extract folder from file path
        std::wstring folder = m_filePath;
        size_t pos = folder.find_last_of(L"\\/");
        if (pos != std::wstring::npos)
            folder = folder.substr(0, pos);
        else
            folder = L".";
        ShellExecuteW(m_hwnd, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}