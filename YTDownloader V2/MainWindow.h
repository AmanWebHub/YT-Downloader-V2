#pragma once

#include <windows.h>
#include <string>

class MainWindow
{
public:
    bool Create(HINSTANCE hInstance, int nCmdShow, const std::wstring& initialUrl);
    HWND GetHandle() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void CreateControls(HWND hwnd);
    void OnDownloadClicked(HWND hwnd);
    void SetDownloadingState(bool downloading);

    HWND m_hwnd = nullptr;
    HWND m_urlEdit = nullptr;
    HWND m_downloadButton = nullptr;
    HWND m_progressBar = nullptr;
    HWND m_statusLabel = nullptr;
};
