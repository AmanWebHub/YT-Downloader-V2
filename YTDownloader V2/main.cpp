#include <windows.h>
#include <shellapi.h>
#include <string>

#define IDC_URL         1001

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // Video URL label
        CreateWindowW(
            L"STATIC",
            L"Video URL:",
            WS_VISIBLE | WS_CHILD,
            30,
            30,
            100,
            25,
            hwnd,
            nullptr,
            nullptr,
            nullptr
        );

        // URL input box
        CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_VISIBLE |
            WS_CHILD |
            ES_AUTOHSCROLL,
            30,
            60,
            620,
            30,
            hwnd,
            (HMENU)IDC_URL,
            nullptr,
            nullptr
        );

        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(
        hwnd,
        uMsg,
        wParam,
        lParam
    );
}

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ PWSTR pCmdLine,
    _In_ int nCmdShow
)
{
    const wchar_t CLASS_NAME[] =
        L"ITDownloaderV2Window";

    WNDCLASSW wc{};

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(
        nullptr,
        IDC_ARROW
    );
    wc.hbrBackground =
        (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
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
        nullptr
    );

    if (hwnd == nullptr)
    {
        MessageBoxW(
            nullptr,
            L"Failed to create the main window.",
            L"IT Downloader V2",
            MB_OK | MB_ICONERROR
        );

        return 0;
    }

    // Read command-line arguments
    int argumentCount = 0;

    LPWSTR* arguments =
        CommandLineToArgvW(
            GetCommandLineW(),
            &argumentCount
        );

    if (arguments != nullptr)
    {
        if (argumentCount > 1)
        {
            HWND urlBox = GetDlgItem(
                hwnd,
                IDC_URL
            );

            SetWindowTextW(
                urlBox,
                arguments[1]
            );
        }

        LocalFree(arguments);
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg{};

    while (GetMessageW(
        &msg,
        nullptr,
        0,
        0
    ))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}