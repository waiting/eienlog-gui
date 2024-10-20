#pragma once
#include <vector>

struct EienLogContext
{
    USHORT port;
    std::string name;
    bool isShow = true;
};

struct App;
struct MainWindow
{
    MainWindow( App & app );
    void render();

    void renderDockSpace();
    void renderDockSpaceMenuBar();

    App & app;
    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;

    // Dock space state
    ImGuiID dockspace_id;
    bool opt_fullscreen = true;
    bool opt_padding = false;
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

    // MyData
    std::vector<EienLogContext> logCtxs;
};

