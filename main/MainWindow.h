#pragma once
#include "EienLogWindows.h"

struct App;
struct MainWindow
{
    MainWindow( App & app );
    void render();

    void renderDockSpace();
    void renderDockSpaceMenuBar();

    App & app;
    // Our state
    bool show_settings_window = true;
    bool show_demo_window = true;
    bool show_about_window = false;

    // Dock space state
    ImGuiID dockspace_id;
    bool opt_fullscreen = true;
    bool opt_padding = false;
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

    // MyData
    winux::SimplePointer<EienLogWindows> logWindows;
};

