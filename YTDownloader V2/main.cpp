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
    // Pin our own working directory to the exe's folder up front.
    // Without this, our CWD is whatever the OS/launching process
    // decided (which varies by how we were started - a shell/
    // protocol-handler launch behaves differently than Visual
    // Studio's debugger, for example) - and if anything ever does a
    // bare-filename file operation (no directory component), that
    // uncertainty otherwise flows straight through to us and
    // anything we launch. yt-dlp's own working directory is set
    // explicitly and separately in DownloadWorker.cpp; this covers
    // our own process the same way.
    SetCurrentDirectoryW(
        DownloadUtils::GetExeDirectory().c_str());

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
