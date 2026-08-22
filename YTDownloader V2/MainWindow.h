#pragma once

#include <windows.h>
#include <string>

class MainWindow
{
public:
    bool Create(HINSTANCE hInstance, int nCmdShow, const std::wstring& initialUrl);
    HWND GetHandle() const { return m_hwnd; }
    HINSTANCE GetInstance() const { return m_hInstance; }

private:
    static LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void CreateControls(HWND hwnd);
    void OnDownloadClicked(HWND hwnd);
    void OnCancelClicked();
    void OnPauseResumeClicked(HWND hwnd);
    void SetDownloadingState(bool downloading);
    void StartDownloadWithParams(HWND hwnd, const std::wstring& url, bool isMp3, bool isPlaylist);

    // Returns 0 = single video, 1 = whole playlist, -1 = user cancelled.
    // Detects a playlist URL and asks the user which they want, unless
    // it's unambiguous (a pure playlist link with no specific video).
    int ResolvePlaylistChoice(HWND hwnd, const std::wstring& url);

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    HWND m_urlEdit = nullptr;
    HWND m_downloadButton = nullptr;
    HWND m_cancelButton = nullptr;
    HWND m_pauseButton = nullptr;
    HWND m_progressBar = nullptr;
    HWND m_statusLabel = nullptr;

    bool m_isPaused = false;
    std::wstring m_lastStatusText;

    // Remembers exactly what the current/last download was, so Resume
    // can restart it identically without re-prompting the playlist
    // question or re-reading (possibly stale) UI state.
    std::wstring m_lastUrl;
    bool m_lastIsMp3 = false;
    bool m_lastIsPlaylist = false;
};
