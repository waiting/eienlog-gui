#pragma once
#include <vector>
#include <thread>
#include <mutex>

struct MainWindow;
struct EienLogWindows;

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
    EienLogWindow( EienLogWindows * manager, winux::Utf8String const & name, winux::Utf8String const & addr, winux::ushort port, time_t waitTimeout, time_t updateTimeout, bool vScrollToBottom );
    ~EienLogWindow();
    void render();

    EienLogWindows * manager = nullptr;
    winux::Utf8String name;
    winux::Utf8String addr;
    winux::ushort port = 0;
    time_t waitTimeout;
    time_t updateTimeout;
    std::vector<LogTextRecord> logs;
    int selected = -1;
    bool bToggleVScrollToBottom = false; // 触发“自动滚动到底”复选框
    bool vScrollToBottom = true; // 是否滚动到底
    std::mutex mtx;
    winux::SimplePointer<std::thread> th;
    bool show = true;
};

struct EienLogWindows
{
    EienLogWindows( MainWindow * mainWindow );

    void addWindow( winux::Utf8String const & name, winux::Utf8String const & addr, winux::ushort port, time_t waitTimeout, time_t updateTimeout, bool vScrollToBottom );
    void render();

    std::vector< winux::SimplePointer<EienLogWindow> > wins;
    MainWindow * mainWindow;
};

