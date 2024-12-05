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
    int saveTargetType = 0; // 保存文件时日志目标类型：0全部日志，1已选择的日志，2不选择的日志
};
