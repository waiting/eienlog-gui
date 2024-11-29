#pragma once

#include <thread>
#include "LogViewerWindow.h"

struct LogWindowsManager;
struct LogListenWindow : LogViewerWindow
{
    LogListenWindow( LogWindowsManager * manager, App::ListenParams const & lparams );
    ~LogListenWindow() override;
    void renderComponents() override;

    App::ListenParams lparams;
    winux::SimplePointer<std::thread> th; // 监听线程
};
