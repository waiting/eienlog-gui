#pragma once
#include <vector>
#include <thread>
#include <mutex>

struct MainWindow;
struct EienLogWindows;

struct LogTextRecord
{
    winux::Buffer content; //!< 日志内容
    winux::AnsiString strContent; //!< 字符串内容
    winux::AnsiString utcTime;  //!< UTC时间戳
    eienlog::LogFlag flag;  //!< 日志样式FLAG
};

struct EienLogWindow
{
    EienLogWindow( EienLogWindows * manager, std::string const & name, std::string const & addr, USHORT port, bool vScrollToBottom );
    ~EienLogWindow();
    void render();

    EienLogWindows * manager = nullptr;
    std::string name;
    std::string addr;
    USHORT port = 0;
    std::vector<LogTextRecord> logs;
    int selected = -1;
    bool bToggleVScrollToBottom = false; // 触发“自动滚动到底”复选框
    bool vScrollToBottom = true;
    std::mutex mtx;
    winux::SimplePointer<std::thread> th;
    bool show = true;
};

struct EienLogWindows
{
    EienLogWindows( MainWindow * mainWindow );

    void addWindow( std::string const & name, std::string const & addr, USHORT port, bool vScrollToBottom );
    void render();

    std::vector< winux::SimplePointer<EienLogWindow> > wins;
    MainWindow * mainWindow;
};

