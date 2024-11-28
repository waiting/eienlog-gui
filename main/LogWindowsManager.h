#pragma once

#include <thread>
#include <mutex>

#include "LogViewerWindow.h"

struct LogWindowsManager;

struct LogListenWindow
{
    LogListenWindow( LogWindowsManager * manager, App::ListenParams const & lparams );
    ~LogListenWindow();
    void render();

    LogWindowsManager * manager = nullptr;
    App::ListenParams lparams;
    std::vector<LogTextRecord> logs;
    int selected = -1; // 选中行
    bool bToggleVScrollToBottom = false; // 触发“自动滚动到底”复选框
    std::mutex mtx; // 数据同步互斥量
    winux::SimplePointer<std::thread> th; // 监听线程
    bool show = true;
};

struct MainWindow;

struct LogWindowsManager
{
    LogWindowsManager( MainWindow * mainWindow );

    void addWindow( App::ListenParams const & lparams );
    void render();

    std::vector< winux::SimplePointer<LogListenWindow> > wins;
    MainWindow * mainWindow;
};
