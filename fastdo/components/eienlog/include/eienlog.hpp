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

/** \brief 日志样式旗标 */
union LogFlag
{
    struct
    {
        winux::uint16 color:16;     //!< 颜色
        winux::uint16 font:12;      //!< 字体编号
        winux::uint16 bold:1;       //!< 是否粗体
        winux::uint16 italic:1;     //!< 是否斜体
        winux::uint16 underline:1;  //!< 是否下划线
        winux::uint16 binary:1;     //!< 是否二进制数据
    };
    winux::uint32 value;
};

/** \brief 日志分块头部 */
struct LogChunkHeader
{
    winux::uint16 packetSize;   //!< 数据包固定大小0~255
    winux::uint16 realLen;      //!< 数据包日志数据占用长度
    winux::uint32 flag;         //!< 控制日志字体样式或颜色等信息，目前暂且保留为0
    winux::uint32 index;        //!< 分块编号
    winux::uint32 chunks;       //!< 总块数
    time_t utcTime;             //!< UTC时间戳(ms)
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
     *  \param host 地址
     *  \param port 端口号
     *  \param chunkSize 分块封包大小 */
    LogWriter( winux::String const & host, winux::ushort port, size_t chunkSize = LOG_CHUNK_SIZE ) : _ep( host, port ), _chunkSize(chunkSize)
    {
    }

    /** \brief 发送字符串日志
     *
     *  \param str 字符串内容
     *  \param flag 日志样式
     *  \return size_t 发送的封包数量 */
    size_t log( winux::String const & str, winux::uint32 flag = 0 );

    /** \brief 发送二进制日志
     *
     *  \param data 二进制数据
     *  \param flag 日志样式
     *  \return size_t 发送的封包数量 */
    size_t logBin( winux::Buffer const & data, winux::uint32 flag = 0 );

private:
    eiennet::ip::udp::Socket _sock;
    eiennet::ip::EndPoint _ep;
    size_t const _chunkSize;
};

/** \brief 日志读取器 */
class EIENLOG_DLL LogReader
{
public:
    struct LogChunksData
    {
        std::vector< winux::Packet<LogChunk> > packs;
        time_t lastUpdate;
    };

    /** \brief 构造函数
     *
     *  \param host 地址
     *  \param port 端口号
     *  \param chunkSize 分块封包大小 */
    LogReader( winux::String const & host, winux::ushort port, size_t chunkSize = LOG_CHUNK_SIZE );

    /** \brief 阻塞读取一个分块封包
     *
     *  \param pack 接受封包
     *  \param ep 接受发送者EndPoint
     *  \return bool */
    bool readPack( winux::Packet<LogChunk> * pack, eiennet::ip::EndPoint * ep );

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
    std::map< time_t, LogChunksData > _packsMap;
    size_t const _chunkSize;
    int _errno;
};


} // namespace eienlog


#endif // __EIENLOG_HPP__
