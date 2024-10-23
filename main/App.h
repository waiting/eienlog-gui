#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "misc/cpp/imgui_stdlib.h"
#include <d3d9.h>
#include <d3dx9tex.h>
#include "winux.hpp"
#include "eiennet.hpp"
#include "eienlog.hpp"
#include "GraphicsInterface.h"
#include "WindowInterface.h"
#include "MainWindow.h"


// 本应用程序
struct App
{
    App()
    {
    }

    ~App()
    {
    }

    bool initInstance( HINSTANCE hInstance, int nCmdShow );

    void exitInstance();

    // 消息循环
    int run();

    // 渲染界面
    void renderUI();

    GraphicsInterface gi; // DirectX 3D
    WindowInterface wi; // Win32 Window
    ImGuiContext * ctx; // ImGui Context

    // 背景相关
    LPDIRECT3DTEXTURE9 pBgTexture = nullptr;
    D3DXIMAGE_INFO bgImgInfo;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // 字体加载
    ImFont * bigFont;
    ImFont * normalFont;
    // 主渲染窗口
    MainWindow * mainWindow = nullptr;
    std::string welcomeText;
};

extern App g_app;

// 渲染Tooltip
static void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if ( ImGui::BeginItemTooltip() )
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
