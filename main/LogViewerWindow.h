#pragma once
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

};
