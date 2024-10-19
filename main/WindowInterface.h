#pragma once
// ���ڵײ� Win32����
struct WindowInterface
{
    static LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

    bool registerWndClass( LPCWSTR wndClassName, HINSTANCE hInstance );

    void unregisterWndClass();

    bool createWindow( LPCWSTR wndTitleName );

    void showUpdate( int cmdShow = SW_SHOWDEFAULT, bool update = true );

    void destroyWindow();

    WNDCLASSEXW wc;
    HWND hWnd;
    HICON hIcon;
};
