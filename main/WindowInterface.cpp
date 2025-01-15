#include "App.h"
#include "resource.h"
#include "WindowInterface.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED                   0x02E0
#endif

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WindowInterface::WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_app.gi.resizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_app.gi.resizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
        {
            //const int dpi = HIWORD(wParam);
            //printf("WM_DPICHANGED to %d (%.0f%%)\n", dpi, (float)dpi / 96.0f * 100.0f);
            const RECT* suggested_rect = (RECT*)lParam;
            ::SetWindowPos(hWnd, nullptr, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool WindowInterface::registerWndClass( LPCWSTR wndClassName, HINSTANCE hInstance )
{
    this->hIcon = LoadIconW( hInstance, MAKEINTRESOURCEW(IDI_EIENLOGGUI) );
    this->wc.cbSize = sizeof(WNDCLASSEXW);
    this->wc.style = CS_CLASSDC;
    this->wc.lpfnWndProc = WndProc;
    this->wc.cbClsExtra = 0;
    this->wc.cbWndExtra = 0;
    this->wc.hInstance = hInstance;
    this->wc.hIcon = this->hIcon;
    this->wc.hCursor = nullptr;
    this->wc.hbrBackground = nullptr;
    this->wc.lpszMenuName = nullptr;
    this->wc.lpszClassName = wndClassName;
    this->wc.hIconSm = this->hIcon;

    if ( !RegisterClassExW(&this->wc) ) return false;
    return true;
}

void WindowInterface::unregisterWndClass()
{
    UnregisterClassW( wc.lpszClassName, wc.hInstance );
}

bool WindowInterface::createWindow( LPCWSTR wndTitleName )
{
    this->hWnd = ::CreateWindowExW( WS_EX_TRANSPARENT, wc.lpszClassName, wndTitleName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, wc.hInstance, nullptr );
    bool ok = this->hWnd != nullptr;
    return ok;
}

void WindowInterface::showUpdate( int cmdShow, bool update )
{
    ::ShowWindow( this->hWnd, cmdShow );
    if ( update ) ::UpdateWindow(this->hWnd);
}

void WindowInterface::destroyWindow()
{
    if ( this->hWnd != nullptr ) ::DestroyWindow(this->hWnd);
}
