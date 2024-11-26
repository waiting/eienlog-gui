#pragma once
#include <vector>
#include <thread>
#include <mutex>

struct MainWindow;
struct EienLogWindowsManager;

struct LogTextRecord
{
    winux::Buffer content; //!< 日志内容
    winux::Utf8String strContent; //!< 字符串内容
    winux::Utf8String strContentSlashes; //!< 字符串转义内容
    winux::Utf8String utcTime;  //!< UTC时间戳
    eienlog::LogFlag flag;  //!< 日志样式FLAG
};

struct EienLogWindow
{
    EienLogWindow( EienLogWindowsManager * manager, App::ListenParams const & lparams );
    ~EienLogWindow();
    void render();

    EienLogWindowsManager * manager = nullptr;
    App::ListenParams lparams;
    std::vector<LogTextRecord> logs;
    int selected = -1;
    bool bToggleVScrollToBottom = false; // 触发“自动滚动到底”复选框
    std::mutex mtx;
    winux::SimplePointer<std::thread> th;
    bool show = true;
};

struct EienLogWindowsManager
{
    EienLogWindowsManager( MainWindow * mainWindow );

    void addWindow( App::ListenParams const & lparams );
    void render();

    std::vector< winux::SimplePointer<EienLogWindow> > wins;
    MainWindow * mainWindow;
};
