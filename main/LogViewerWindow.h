#pragma once
#include <mutex>

struct LogWindowsManager;

struct LogTextRecord
{
    size_t contentSize;  //!< 日志内容大小
    winux::Utf8String strContent;   //!< 字符串内容
    winux::Utf8String strContentSlashes;    //!< 字符串转义内容
    winux::Utf8String utcTime;  //!< UTC时间戳
    eienlog::LogFlag flag;  //!< 日志样式FLAG
};

struct LogViewerWindow
{
    LogViewerWindow( LogWindowsManager * manager, winux::Utf8String const & name, bool vScrollToBottom, winux::Utf8String const & logFile = u8"" );
    virtual ~LogViewerWindow();

    void render();
    virtual void renderComponents();

    LogWindowsManager * manager;
    winux::Utf8String name;
    bool vScrollToBottom;
    winux::Utf8String logFile;
    std::vector<LogTextRecord> logs;

    std::map< int, bool > selected; // 选中行
    int clickRowPrev = -1;  // 上次点击行
    bool bToggleVScrollToBottom = false; // 触发“自动滚动到底”复选框
    bool bToggleToTop = false; // 触发“到顶”
    std::mutex mtx; // 数据同步互斥量
    bool show = true;
};
