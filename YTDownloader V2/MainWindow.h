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
    int ResolvePlaylistChoice(HWND hwnd, const std::wstring& url);

    void SetFormatSelection(bool mp3);
    void UpdateProgressText(int progress);
    void ApplyControlFonts();
    void PaintBackground(HDC hdc, const RECT& clientRect);
    void DrawFormatCard(HDC hdc, const RECT& rect, const wchar_t* title,
                        const wchar_t* subtitle, bool selected);
    void DrawOwnerButton(const DRAWITEMSTRUCT* dis);

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    HWND m_urlEdit = nullptr;
    HWND m_downloadButton = nullptr;
    HWND m_cancelButton = nullptr;
    HWND m_pauseButton = nullptr;
    HWND m_progressBar = nullptr;
    HWND m_statusLabel = nullptr;
    HWND m_progressPercent = nullptr;
    HWND m_mp4Button = nullptr;
    HWND m_mp3Button = nullptr;
    HWND m_statusCaption = nullptr;

    HFONT m_titleFont = nullptr;
    HFONT m_sectionFont = nullptr;
    HFONT m_bodyFont = nullptr;
    HFONT m_smallFont = nullptr;
    HFONT m_buttonFont = nullptr;

    bool m_isPaused = false;
    bool m_selectedMp3 = false;
    bool m_cancelPending = false;
    std::wstring m_lastStatusText;

    std::wstring m_lastUrl;
    bool m_lastIsMp3 = false;
    bool m_lastIsPlaylist = false;
};
