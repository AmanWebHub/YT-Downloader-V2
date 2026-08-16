#include <windows.h>
#include <shellapi.h>
#include <string>

#define IDC_URL             1001
#define IDC_RADIO_MP4       1002
#define IDC_RADIO_MP3       1003
#define IDC_DOWNLOAD_BTN    1004

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

        // Format label
        CreateWindowW(
            L"STATIC",
            L"Format:",
            WS_VISIBLE | WS_CHILD,
            30,
            110,
            100,
            25,
            hwnd,
            nullptr,
            nullptr,
            nullptr
        );

        // MP4 radio button (first in the group, default checked)
        CreateWindowW(
            L"BUTTON",
            L"MP4 Video",
            WS_VISIBLE | WS_CHILD | WS_GROUP | BS_AUTORADIOBUTTON,
            30,
            140,
            120,
            25,
            hwnd,
            (HMENU)IDC_RADIO_MP4,
            nullptr,
            nullptr
        );

        // MP3 radio button
        CreateWindowW(
            L"BUTTON",
            L"MP3 Audio",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
            160,
            140,
            120,
            25,
            hwnd,
            (HMENU)IDC_RADIO_MP3,
            nullptr,
            nullptr
        );

        // Default the format selection to MP4
        SendMessageW(
            GetDlgItem(hwnd, IDC_RADIO_MP4),
            BM_SETCHECK,
            BST_CHECKED,
            0
        );

        // Download button
        CreateWindowW(
            L"BUTTON",
            L"Download",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            30,
            190,
            120,
            35,
            hwnd,
            (HMENU)IDC_DOWNLOAD_BTN,
            nullptr,
            nullptr
        );

        return 0;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDC_DOWNLOAD_BTN)
        {
            // Read the URL
            HWND urlBox = GetDlgItem(hwnd, IDC_URL);
            wchar_t urlBuffer[2048]{};
            GetWindowTextW(urlBox, urlBuffer, 2048);

            // Read the selected format
            bool isMp3 = (SendMessageW(
                GetDlgItem(hwnd, IDC_RADIO_MP3),
                BM_GETCHECK,
                0,
                0
            ) == BST_CHECKED);

            std::wstring message = L"URL: ";
            message += urlBuffer;
            message += L"\nFormat: ";
            message += isMp3 ? L"MP3 Audio" : L"MP4 Video";
            message += L"\n\n(Actual download logic comes next)";

            MessageBoxW(
                hwnd,
                message.c_str(),
                L"IT Downloader V2",
                MB_OK | MB_ICONINFORMATION
            );
        }

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