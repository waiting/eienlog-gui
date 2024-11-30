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
#include "winplus.hpp"
#include "GraphicsInterface.h"
#include "WindowInterface.h"

struct MainWindow;
// 本应用程序
struct App
{
    // 监听参数
    struct ListenParams
    {
        winux::Utf8String name;
        winux::Utf8String addr;
        winux::ushort port;
        time_t waitTimeout;
        time_t updateTimeout;
        bool vScrollToBottom; // 是否滚动到底

        bool operator == ( ListenParams const & other ) const
        {
            return
                this->name == other.name &&
                this->addr == other.addr &&
                this->port == other.port &&
                this->waitTimeout == other.waitTimeout &&
                this->updateTimeout == other.updateTimeout &&
                this->vScrollToBottom == other.vScrollToBottom
            ;
        }
    };

    // JSON配置参数
    struct AppConfig
    {
        int colorTheme;
        winux::Utf8String fontPath;
        float fontSize;
        winux::Utf8String bigFontPath;
        float bigFontSize;
        bool logTableColumnResize;
        std::vector<ListenParams> listenHistory;
        std::vector<winux::Utf8String> logFileHistory;
    };

    App()
    {
    }

    ~App()
    {
    }

    // 加载应用配置
    void loadConfig();

    // 保存应用配置
    void saveConfig();

    // 设置一条最近监听，如果没有则加入到第一条，如果存在则移动到第一条
    void setRecentListen( ListenParams const & lparams );

    void setRecentOpen( winux::Utf8String const & logFile );

    bool initInstance( HINSTANCE hInstance, int nCmdShow );

    void exitInstance();

    // 消息循环
    int run();

    // 渲染界面
    void renderUI();

    AppConfig appConfig;

    GraphicsInterface gi; // DirectX 3D
    WindowInterface wi; // Win32 Window
    ImGuiContext * ctx; // ImGui Context

    // 背景相关
    LPDIRECT3DTEXTURE9 pBgTexture = nullptr;
    D3DXIMAGE_INFO bgImgInfo;
    ImVec4 bgClearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // 字体加载
    ImFont * bigFont;
    ImFont * normalFont;
    // 主渲染窗口
    MainWindow * mainWindow = nullptr;
    winux::Utf8String welcomeText;
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

// Foreground color: R5G5B5
inline static void _GetImVec4ColorFromColorR5G5B5( winux::uint16 fgColor, ImVec4 * color )
{
    float r = ( fgColor & 31 ) / 31.0f, g = ( ( fgColor >> 5 ) & 31 ) / 31.0f, b = ( ( fgColor >> 10 ) & 31 ) / 31.0f;
    color->x = r;
    color->y = g;
    color->z = b;
    color->w = 1.0f;
}

// Background color: R4G4B4
inline static void _GetImVec4ColorFromColorR4G4B4( winux::uint16 bgColor, ImVec4 * color )
{
    float r = ( bgColor & 15 ) / 15.0f, g = ( ( bgColor >> 4 ) & 15 ) / 15.0f, b = ( ( bgColor >> 8 ) & 15 ) / 15.0f;
    color->x = r;
    color->y = g;
    color->z = b;
    color->w = 1.0f;
}
