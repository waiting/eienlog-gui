#ifndef __EIENLOG_HPP__
#define __EIENLOG_HPP__

#include "eiennet.hpp"

/** \brief 日志功能，通过UDP协议高效的收发日志 */
namespace eienlog
{
#ifdef EIENLOG_DLL_USE
    #if defined(_MSC_VER) || defined(WIN32)
        #pragma warning( disable: 4251 )
        #ifdef EIENLOG_DLL_EXPORTS
            #define EIENLOG_DLL  __declspec(dllexport)
        #else
            #define EIENLOG_DLL  __declspec(dllimport)
        #endif

        #define EIENLOG_API __stdcall
    #else
        #define EIENLOG_DLL
        #define EIENLOG_API
    #endif
#else
    #define EIENLOG_DLL
    #define EIENLOG_API
#endif

#define EIENLOG_FUNC_DECL(ret) EIENLOG_DLL ret EIENLOG_API
#define EIENLOG_FUNC_IMPL(ret) ret EIENLOG_API

// 日志UDP协议相关结构体

#define LOG_CHUNK_SIZE 80

/** \brief 日志编码。`Local`表示本地多字节编码，不同国家可能不同 */
enum LogEncoding
{
    leLocal,
    leUtf8,
    leUtf16Le,
    leUtf16Be,
};

//! 前景色: R5G5B5
#define LOG_FG_COLOR( r, g, b ) (winux::uint16)( (r) | ( (g) << 5 ) | ( (b) << 10 ) )

//! 背景色: R4G4B4
#define LOG_BG_COLOR( r, g, b ) (winux::uint16)( (r) | ( (g) << 4 ) | ( (b) << 8 ) )

/** \brief 日志颜色 */
enum LogFgColor : winux::uint16
{
    lfcBlack = LOG_FG_COLOR( 0, 0, 0 ),

    lfcMaroon = LOG_FG_COLOR( 16, 0, 0 ),
    lfcAtrovirens = LOG_FG_COLOR( 0, 16, 0 ),
    lfcOlive = lfcMaroon | lfcAtrovirens,
    lfcNavy = LOG_FG_COLOR( 0, 0, 16 ),
    lfcPurple = lfcMaroon | lfcNavy,
    lfcTeal = lfcAtrovirens | lfcNavy, 
    lfcGray = lfcMaroon | lfcAtrovirens | lfcNavy,

    lfcSilver = LOG_FG_COLOR( 24, 24, 24 ),

    lfcRed = LOG_FG_COLOR( 31, 0, 0 ),
    lfcGreen = LOG_FG_COLOR( 0, 31, 0 ),
    lfcYellow = lfcRed | lfcGreen,
    lfcBlue = LOG_FG_COLOR( 0, 0, 31 ),
    lfcFuchsia = lfcRed | lfcBlue,
    lfcAqua = lfcGreen | lfcBlue,
    lfcWhite = lfcRed | lfcGreen | lfcBlue
};

/** \brief 日志背景颜色 */
enum LogBgColor : winux::uint16
{
    lbcBlack = LOG_BG_COLOR( 0, 0, 0 ),

    lbcMaroon = LOG_BG_COLOR( 8, 0, 0 ),
    lbcAtrovirens = LOG_BG_COLOR( 0, 8, 0 ),
    lbcOlive = lbcMaroon | lbcAtrovirens,
    lbcNavy = LOG_BG_COLOR( 0, 0, 8 ),
    lbcPurple = lbcMaroon | lbcNavy,
    lbcTeal = lbcAtrovirens | lbcNavy, 
    lbcGray = lbcMaroon | lbcAtrovirens | lbcNavy,

    lbcSilver = LOG_BG_COLOR( 12, 12, 12 ),

    lbcRed = LOG_BG_COLOR( 15, 0, 0 ),
    lbcGreen = LOG_BG_COLOR( 0, 15, 0 ),
    lbcYellow = lbcRed | lbcGreen,
    lbcBlue = LOG_BG_COLOR( 0, 0, 15 ),
    lbcFuchsia = lbcRed | lbcBlue,
    lbcAqua = lbcGreen | lbcBlue,
    lbcWhite = lbcRed | lbcGreen | lbcBlue
};

/** \brief 日志样式旗标 */
union LogFlag
{
    struct
    {
        winux::uint16 fgColor:15;       //!< 颜色R5G5B5
        winux::uint16 fgColorUse:1;     //!< 颜色是否使用
        winux::uint16 bgColor:12;       //!< 背景色R4G4B4
        winux::uint16 bgColorUse:1;     //!< 背景色是否使用
        winux::uint16 logEncoding:2;    //!< 日志编码
        winux::uint16 binary:1;         //!< 是否二进制数据
    };
    winux::uint32 value;

    LogFlag() : value(0)
    {
    }

    explicit LogFlag( winux::uint8 logEncoding ) : value(0)
    {
        this->logEncoding = logEncoding;
        this->binary = false;
    }

    LogFlag( bool useFgColor, winux::uint16 fgColor, bool useBgColor, winux::uint16 bgColor, winux::uint8 logEncoding, bool isBinary )
    {
        this->fgColorUse = useFgColor;
        this->fgColor = fgColor;
        this->bgColorUse = useBgColor;
        this->bgColor = bgColor;
        this->logEncoding = logEncoding;
        this->binary = isBinary;
    }

    LogFlag( winux::Mixed const & fgColor, winux::Mixed const & bgColor, winux::uint8 logEncoding = leUtf8, bool isBinary = false )
    {
        this->fgColorUse = !fgColor.isNull();
        this->fgColor = fgColor;
        this->bgColorUse = !bgColor.isNull();
        this->bgColor = bgColor;
        this->logEncoding = logEncoding;
        this->binary = isBinary;
    }
};

/** \brief 日志分块头部 */
struct LogChunkHeader
{
    winux::uint16 chunkSize;    //!< 数据包固定大小0~65535
    winux::uint16 realLen;      //!< 数据包日志数据占用长度
    winux::uint16 index;        //!< 分块编号
    winux::uint16 total;        //!< 总块数
    winux::uint32 flag;         //!< 控制日志样式或颜色等信息
    winux::uint32 id;           //!< 记录ID
    winux::uint64 utcTime;      //!< UTC时间戳(ms)
};

/** \brief 日志分块 */
struct LogChunk : LogChunkHeader
{
    char logSpace[1];   //!< 日志空间
};

/** \brief 日志记录 */
struct LogRecord
{
    winux::Buffer data; //!< 日志数据
    time_t utcTime;     //!< UTC时间戳(ms)
    winux::uint32 flag; //!< 日志样式FLAG
};

/** \brief 日志写入器 */
class EIENLOG_DLL LogWriter
{
public:
    /** \brief 构造函数
     *
     *  \param addr 地址
     *  \param port 端口号
     *  \param chunkSize 分块封包大小 */
    LogWriter( winux::String const & addr, winux::ushort port, winux::uint16 chunkSize = LOG_CHUNK_SIZE );

    /** \brief 发送日志（不转换编码）
     *
     *  \param data 数据
     *  \return size_t 发送的封包数量 */
    size_t logEx( winux::Buffer const & data, LogFlag flag );

    /** \brief 发送日志（不转换编码）
     *
     *  \param data 数据
     *  \param fgColor 前景色，若`winux::mxNull`则忽略
     *  \param bgColor 背景色，若`winux::mxNull`则忽略
     *  \param logEncoding 指定日志编码
     *  \param isBinary 是否二进制
     *  \return size_t 发送的封包数量 */
    size_t logEx( winux::Buffer const & data, winux::Mixed const & fgColor = winux::mxNull, winux::Mixed const & bgColor = winux::mxNull, winux::uint8 logEncoding = leUtf8, bool isBinary = false );

    /** \brief 发送字符串日志（会转换编码）
     *
     *  \param str 字符串内容
     *  \return size_t 发送的封包数量 */
    size_t log( winux::String const & str, LogFlag flag );

    /** \brief 发送字符串日志（会转换编码）
     *
     *  \param str 字符串内容
     *  \param fgColor 前景色，若`winux::mxNull`则忽略
     *  \param bgColor 背景色，若`winux::mxNull`则忽略
     *  \param logEncoding 指定转换成目标编码发送
     *  \return size_t 发送的封包数量 */
    size_t log( winux::String const & str, winux::Mixed const & fgColor = winux::mxNull, winux::Mixed const & bgColor = winux::mxNull, winux::uint8 logEncoding = leUtf8 );

    /** \brief 发送二进制日志
     *
     *  \param data 二进制数据
     *  \return size_t 发送的封包数量 */
    size_t logBin( winux::Buffer const & data, LogFlag flag )
    {
        flag.logEncoding = 0;
        flag.binary = true;
        return this->logEx( data, flag );
    }

    /** \brief 发送二进制日志
     *
     *  \param data 二进制数据
     *  \param fgColor 前景色，若`winux::mxNull`则忽略
     *  \param bgColor 背景色，若`winux::mxNull`则忽略
     *  \return size_t 发送的封包数量 */
    size_t logBin( winux::Buffer const & data, winux::Mixed const & fgColor = winux::mxNull, winux::Mixed const & bgColor = winux::mxNull )
    {
        return this->logEx( data, LogFlag( !fgColor.isNull(), fgColor, !bgColor.isNull(), bgColor, 0, true ) );
    }

    int errNo() const { return _errno; }

private:
    eiennet::ip::udp::Socket _sock;
    eiennet::ip::EndPoint _ep;
    winux::uint16 const _chunkSize;
    int _errno;
};

/** \brief 日志读取器 */
class EIENLOG_DLL LogReader
{
public:
    struct LogChunksData
    {
        std::vector< winux::Packet<LogChunk> > chunks;
        time_t lastUpdate;
    };

    /** \brief 构造函数
     *
     *  \param addr 地址
     *  \param port 端口号
     *  \param chunkSize 分块封包大小 */
    LogReader( winux::String const & addr, winux::ushort port, winux::uint16 chunkSize = LOG_CHUNK_SIZE );

    /** \brief 阻塞读取一个分块封包
     *
     *  \param chunk 接受封包
     *  \param ep 接受发送者EndPoint
     *  \return bool */
    bool readChunk( winux::Packet<LogChunk> * chunk, eiennet::ip::EndPoint * ep );

    /** \brief 读取一条日志记录
     *
     *  \param record 接受记录
     *  \param waitTimeout 等待超时
     *  \param updateTimeout 封包更新超时
     *  \return bool */
    bool readRecord( LogRecord * record, time_t waitTimeout = 3000, time_t updateTimeout = 3000 );

    int errNo() const { return _errno; }

private:
    eiennet::ip::udp::Socket _sock;
    eiennet::ip::EndPoint _ep;
    std::map< winux::uint32, LogChunksData > _chunksMap;
    winux::uint16 const _chunkSize;
    int _errno;
};


/** \brief 启用日志 */
EIENLOG_FUNC_DECL(bool) EnableLog( winux::String const & addr = $T("127.0.0.1"), winux::ushort port = 22345, winux::uint16 chunkSize = LOG_CHUNK_SIZE );

/** \brief 禁用日志 */
EIENLOG_FUNC_DECL(void) DisableLog();

/** \brief 写字符串日志（带进程ID） */
EIENLOG_FUNC_DECL(void) WriteLog( winux::String const & str );

/** \brief 写二进制日志 */
EIENLOG_FUNC_DECL(void) WriteLogBin( void const * data, size_t size );

/** \brief 发送日志（不转换编码）
 *
 *  \param data 数据
 *  \return size_t 发送的封包数量 */
EIENLOG_FUNC_DECL(size_t) LogEx( winux::Buffer const & data, LogFlag flag );

/** \brief 发送日志（不转换编码）
 *
 *  \param data 数据
 *  \param fgColor 前景色，若`winux::mxNull`则忽略
 *  \param bgColor 背景色，若`winux::mxNull`则忽略
 *  \param logEncoding 指定日志编码
 *  \param isBinary 是否二进制
 *  \return size_t 发送的封包数量 */
EIENLOG_FUNC_DECL(size_t) LogEx( winux::Buffer const & data, winux::Mixed const & fgColor = winux::mxNull, winux::Mixed const & bgColor = winux::mxNull, winux::uint8 logEncoding = leUtf8, bool isBinary = false );

/** \brief 发送字符串日志（会转换编码）
 *
 *  \param str 字符串内容
 *  \return 发送的封包数量 */
EIENLOG_FUNC_DECL(size_t) Log( winux::String const & str, LogFlag flag );

/** \brief 发送字符串日志（会转换编码）
 *
 *  \param str 字符串内容
 *  \param fgColor 前景色，若`winux::mxNull`则忽略
 *  \param bgColor 背景色，若`winux::mxNull`则忽略
 *  \param logEncoding 指定转换成目标编码发送
 *  \return 发送的封包数量 */
EIENLOG_FUNC_DECL(size_t) Log( winux::String const & str, winux::Mixed const & fgColor = winux::mxNull, winux::Mixed const & bgColor = winux::mxNull, winux::uint8 logEncoding = leUtf8 );

/** \brief 发送二进制日志
 *
 *  \param data 二进制数据
 *  \return 发送的封包数量 */
EIENLOG_FUNC_DECL(size_t) LogBin( winux::Buffer const & data, LogFlag flag );

/** \brief 发送二进制日志
 *
 *  \param data 二进制数据
 *  \param fgColor 前景色，若`winux::mxNull`则忽略
 *  \param bgColor 背景色，若`winux::mxNull`则忽略
 *  \return 发送的封包数量 */
EIENLOG_FUNC_DECL(size_t) LogBin( winux::Buffer const & data, winux::Mixed const & fgColor = winux::mxNull, winux::Mixed const & bgColor = winux::mxNull );

/** \brief 发送日志（不转换编码）
 *
 *  \return size_t 发送的封包数量 */
template < typename... _ArgType >
inline static size_t LogExOutput( LogFlag flag, _ArgType&& ... arg )
{
    std::basic_ostringstream<winux::tchar> sout;
    winux::OutputV( sout, std::forward<_ArgType>(arg)... );
    return LogEx( sout.str(), flag );
}

/** \brief 发送字符串日志（会转换编码）
 *
 *  \return 发送的封包数量 */
template < typename... _ArgType >
inline static size_t LogOutput( LogFlag flag, _ArgType&& ... arg )
{
    std::basic_ostringstream<winux::tchar> sout;
    winux::OutputV( sout, std::forward<_ArgType>(arg)... );
    return Log( sout.str(), flag );
}

//#define __LOG__
#ifdef __LOG__
#define LOG(s) eienlog::WriteLog(s)
#define LOG_BIN(d,s) eienlog::WriteLogBin((d),(s))
#else
#define LOG(s)
#define LOG_BIN(d,s)
#endif

/** \brief 冗余信息输出类型 */
enum VerboseOutputType
{
    votNone, //!< 不输出冗余信息
    votConsole, //!< 在控制台输出
    votLogViewer //!< 在日志查看器输出
};

/** \brief 冗余信息顔色屬性 */
enum VerboseColorAttrFlags
{
    vcaFgBlack = 0x00,
    vcaFgNavy = 0x01,
    vcaFgAtrovirens = 0x02,
    vcaFgTeal = 0x03,
    vcaFgMaroon = 0x04,
    vcaFgPurple = 0x05,
    vcaFgOlive = 0x06,
    vcaFgSilver = 0x07,
    vcaFgGray = 0x08,
    vcaFgBlue = 0x09,
    vcaFgGreen = 0x0a,
    vcaFgAqua = 0x0b,
    vcaFgRed = 0x0c,
    vcaFgFuchsia = 0x0d,
    vcaFgYellow = 0x0e,
    vcaFgWhite = 0x0f,

    vcaBgBlack = 0x00,
    vcaBgNavy = 0x10,
    vcaBgAtrovirens = 0x20,
    vcaBgTeal = 0x30,
    vcaBgMaroon = 0x40,
    vcaBgPurple = 0x50,
    vcaBgOlive = 0x60,
    vcaBgSilver = 0x70,
    vcaBgGray = 0x80,
    vcaBgBlue = 0x90,
    vcaBgGreen = 0xa0,
    vcaBgAqua = 0xb0,
    vcaBgRed = 0xc0,
    vcaBgFuchsia = 0xd0,
    vcaBgYellow = 0xe0,
    vcaBgWhite = 0xf0,

    vcaFgIgnore = 0x0800,
    vcaBgIgnore = 0x8000,
    vcaIgnore = 0x8800
};

EIENLOG_FUNC_DECL(winux::ushort) GetCcaFlagsFromVcaFlags( winux::ushort vcaFlags );
EIENLOG_FUNC_DECL(LogFlag) GetLogFlagFromVcaFlags( winux::ushort vcaFlags );

/** \brief 冗余信息输出 */
template < typename... _ArgType >
inline static void VerboseOutput( int verbose, winux::ushort vcaFlags, _ArgType&& ... arg )
{
    switch ( verbose )
    {
    case votConsole:
        winux::ColorOutputLine( GetCcaFlagsFromVcaFlags(vcaFlags), std::forward<_ArgType>(arg)... );
        break;
    case votLogViewer:
        eienlog::LogOutput( GetLogFlagFromVcaFlags(vcaFlags), std::forward<_ArgType>(arg)... );
        break;
    }
}


} // namespace eienlog


#endif // __EIENLOG_HPP__
