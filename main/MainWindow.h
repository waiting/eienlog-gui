#pragma once
#include "NewLogListenWindowModal.h"
#include "LogWindowsManager.h"

struct App;
struct MainWindow
{
    MainWindow( App & app );

    void render();

    void renderDockSpace();
    void renderDockSpaceMenuBar();

    App & app;
    // Our state
    bool showSettingsWindow = true;
    bool showDemoWindow = false;
    bool showAboutWindow = false;

    // Dock space state
    ImGuiID dockSpaceId;
    bool optFullScreen = true;
    bool optPadding = false;
    ImGuiDockNodeFlags dockSpaceFlags = ImGuiDockNodeFlags_None;

    // MyData
    winux::SimplePointer<LogWindowsManager> logWinManager;
    winux::SimplePointer<NewLogListenWindowModal> newListenWindow;
};

