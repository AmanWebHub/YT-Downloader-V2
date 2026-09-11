#include <windows.h>
#include <shellapi.h>
#include <string>
#include "MainWindow.h"
#include "DownloadUtils.h"

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ PWSTR pCmdLine,
    _In_ int nCmdShow
)
{
    std::wstring initialUrl;

    int argumentCount = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);

    if (arguments != nullptr)
    {
        if (argumentCount > 1)
        {
            initialUrl =
                DownloadUtils::DecodeExternalUrl(
                    arguments[1]);
        }

        LocalFree(arguments);
    }

    MainWindow window;

    if (!window.Create(hInstance, nCmdShow, initialUrl))
    {
        return 0;
    }

    MSG msg{};

    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
