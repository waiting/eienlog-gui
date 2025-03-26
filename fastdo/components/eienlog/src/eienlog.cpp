#include "eienlog.hpp"
#include <thread>
#include <chrono>

#if defined(OS_WIN)
    #include <sys/utime.h>
    #include <direct.h>
    #include <io.h>
    #include <process.h>
    #include <tchar.h>
#else
    #include <utime.h>
    #include <unistd.h>
    #include <errno.h>
    #include <wchar.h>
    #include <math.h>
#endif

namespace eienlog
{
// 根据数据创建一系列分块
static std::vector< winux::Packet<LogChunk> > _BuildChunks( winux::Buffer const & data, time_t utcTime, winux::uint32 flag, winux::uint16 chunkSize )
{
    static winux::uint32 recordId = 0;
    std::vector< winux::Packet<LogChunk> > chunks;
    // 日志空间，每个数据包可以容纳的日志数据
    winux::uint16 logSpaceSize = chunkSize - sizeof(LogChunkHeader);
    size_t remainingSize = data.size();
    auto total = (winux::uint16)ceil( data.size() / (double)logSpaceSize );
    winux::uint16 index = 0;
    while ( remainingSize > logSpaceSize )
    {
        winux::Packet<LogChunk> chunk(chunkSize);
        chunk->chunkSize = chunkSize;
        chunk->realLen = logSpaceSize;
        chunk->index = index++;
        chunk->total = total;
        chunk->flag = flag;
        chunk->id = recordId;
        chunk->utcTime = utcTime;
        memcpy( chunk->logSpace, data.get<winux::byte>() + ( data.size() - remainingSize ), logSpaceSize );
        chunks.push_back( std::move(chunk) );

        remainingSize -= logSpaceSize;
    }
    if ( true )
    {
        winux::Packet<LogChunk> chunk(chunkSize);
        chunk->chunkSize = chunkSize;
        chunk->realLen = (winux::uint16)remainingSize;
        chunk->index = index++;
        chunk->total = total;
        chunk->flag = flag;
        chunk->id = recordId;
        chunk->utcTime = utcTime;
        memcpy( chunk->logSpace, data.get<winux::byte>() + ( data.size() - remainingSize ), remainingSize );
        chunks.push_back( std::move(chunk) );
    }
    recordId++;
    return chunks;
}

// 通过封包还原日志数据
static bool _ResumeRecord( std::vector< winux::Packet<LogChunk> > const & chunkPacks, LogRecord * record, winux::uint16 chunkSize )
{
    // 日志空间，每个数据包可以容纳的日志数据
    size_t const logSpaceSize = chunkSize - sizeof(LogChunkHeader);
    if ( chunkPacks.size() > 0 )
    {
        LogChunk * chunk = chunkPacks[0].get();
        record->data.alloc( chunk->total * logSpaceSize );
        record->flag = chunk->flag;
        record->utcTime = chunk->utcTime;
    }
    size_t n = 0;
    for ( auto && chunkPack : chunkPacks )
    {
        LogChunk * chunk = chunkPack.get();
        memcpy( record->data.get<winux::byte>() + chunk->index * logSpaceSize, chunk->logSpace, chunk->realLen );
        n += chunk->realLen;
    }
    record->data._setSize(n);
    return record->data.capacity() - n < logSpaceSize;
}

// class LogWriter ----------------------------------------------------------------------------
LogWriter::LogWriter( winux::String const & addr, winux::ushort port, winux::uint16 chunkSize ) : _ep( addr, port ), _chunkSize(chunkSize), _errno(0)
{
    _sock.setAddrFamily( _ep.getAddrFamily() );
    if ( !_sock.create() )
        _errno = eiennet::Socket::ErrNo();
}

size_t LogWriter::logEx( winux::Buffer const & data, LogFlag flag )
{
    auto chunkPacks = _BuildChunks( data, winux::GetUtcTimeMs(), flag.value, _chunkSize );
    for ( auto && chunkPack : chunkPacks )
    {
        _sock.sendTo( _ep, chunkPack );
    }
    return chunkPacks.size();
}

size_t LogWriter::logEx( winux::Buffer const & data, winux::Mixed const & fgColor, winux::Mixed const & bgColor, winux::uint8 logEncoding, bool isBinary )
{
    return this->logEx( data, LogFlag( fgColor, bgColor, logEncoding, isBinary ) );
}

size_t LogWriter::log( winux::String const & str, LogFlag flag )
{
    flag.binary = false;
    switch ( flag.logEncoding )
    {
    case leUtf8:
        {
        #if defined(_UNICODE) || defined(UNICODE)
            winux::AnsiString mbs = winux::UnicodeConverter(str).toUtf8();
        #else
            winux::AnsiString mbs = LOCAL_TO_UTF8(str);
        #endif
            return this->logEx( winux::Buffer( mbs.c_str(), mbs.length(), true ), flag );
        }
        break;
    case leUtf16Le:
        {
        #if defined(_UNICODE) || defined(UNICODE)
            winux::Utf16String ustr = winux::UnicodeConverter(str).toUtf16();
        #else
            winux::Utf16String ustr = winux::UnicodeConverter( winux::LocalToUnicode(str) ).toUtf16();
        #endif
            if ( winux::IsBigEndian() )
            {
                if ( ustr.length() > 0 ) winux::InvertByteOrderArray( &ustr[0], ustr.length() );
            }
            return this->logEx( winux::Buffer( ustr.c_str(), ustr.length(), true ), flag );
        }
        break;
    case leUtf16Be:
        {
        #if defined(_UNICODE) || defined(UNICODE)
            winux::Utf16String ustr = winux::UnicodeConverter(str).toUtf16();
        #else
            winux::Utf16String ustr = winux::UnicodeConverter( winux::LocalToUnicode(str) ).toUtf16();
        #endif
            if ( winux::IsLittleEndian() )
            {
                if ( ustr.length() > 0 ) winux::InvertByteOrderArray( &ustr[0], ustr.length() );
            }
            return this->logEx( winux::Buffer( ustr.c_str(), ustr.length(), true ), flag );
        }
        break;
    default: // leLocal
        {
        #if defined(_UNICODE) || defined(UNICODE)
            winux::AnsiString mbs = winux::UnicodeToLocal(str);
            return this->logEx( winux::Buffer( mbs.c_str(), mbs.length(), true ), flag );
        #else
            return this->logEx( winux::Buffer( str.c_str(), str.length(), true ), flag );
        #endif
        }
        break;
    }
}

size_t LogWriter::log( winux::String const & str, winux::Mixed const & fgColor, winux::Mixed const & bgColor, winux::uint8 logEncoding )
{
    return this->log( str, LogFlag( !fgColor.isNull(), fgColor, !bgColor.isNull(), bgColor, logEncoding, false ) );
}

// class LogReader ----------------------------------------------------------------------------
LogReader::LogReader( winux::String const & addr, winux::ushort port, winux::uint16 chunkSize ) : _ep( addr, port ), _chunkSize(chunkSize), _errno(0)
{
    if ( !_sock.bind(_ep) )
        _errno = eiennet::Socket::ErrNo();
}

bool LogReader::readChunk( winux::Packet<LogChunk> * chunk, eiennet::ip::EndPoint * ep )
{
    chunk->alloc(_chunkSize);
    size_t hadBytes = 0;
    do
    {
        int rc = _sock.recvFrom( ep, chunk->Buffer::get<winux::byte>() + hadBytes, chunk->capacity() - hadBytes );
        if ( rc < 0 ) return false;
        hadBytes += rc;
    } while ( hadBytes < _chunkSize );
    return true;
}

bool LogReader::readRecord( LogRecord * record, time_t waitTimeout, time_t updateTimeout )
{
    thread_local io::SelectRead sel;
    while ( true )
    {
        time_t curTime = winux::GetUtcTimeMs();
        while ( _sock.getAvailable() < _chunkSize && winux::GetUtcTimeMs() - curTime < (winux::uint64)waitTimeout )
        {
            sel.clear();
            sel.setReadSock(_sock);
            sel.wait( waitTimeout / 1000.0 );
        }

        curTime = winux::GetUtcTimeMs();

        if ( _sock.getAvailable() < _chunkSize )
        {
            if ( _chunksMap.size() == 0 )
            {
                return false;
            }
        }
        else
        {
            winux::Packet<LogChunk> chunk;
            eiennet::ip::EndPoint ep;
            if ( this->readChunk( &chunk, &ep ) )
            {
                auto && chunksData = _chunksMap[chunk->id];
                chunksData.lastUpdate = curTime;
                chunksData.chunks.push_back( std::move(chunk) );
            }
        }

        // 检查已收到的分块是否超时或者完整
        for ( auto it = _chunksMap.begin(); it != _chunksMap.end(); )
        {
            if ( curTime - it->second.lastUpdate > updateTimeout )
            {
                _ResumeRecord( it->second.chunks, record, _chunkSize );
                it = _chunksMap.erase(it);
                return true;
            }
            else
            {
                if ( it->second.chunks.size() == it->second.chunks[0]->total )
                {
                    _ResumeRecord( it->second.chunks, record, _chunkSize );
                    it = _chunksMap.erase(it);
                    return true;
                }
                else
                {
                    it++;
                }
            }
        }
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static eiennet::SocketLib * __sockLib = nullptr; // Socket库初始化
static LogWriter * __logWriter = nullptr; // 日志写入器对象
static winux::MutexNative __mtxLogWriter; // 日志写入器共享互斥锁

EIENLOG_FUNC_IMPL(bool) EnableLog( winux::String const & addr, winux::ushort port, winux::uint16 chunkSize )
{
    winux::ScopeGuard guard(__mtxLogWriter);
    if ( __sockLib == nullptr ) __sockLib = new eiennet::SocketLib();

    if ( __logWriter == nullptr )
    {
        auto logWriter = new LogWriter( addr, port, chunkSize );
        if ( logWriter->errNo() == 0 )
        {
            __logWriter = logWriter;
            return true;
        }
        else
        {
            delete logWriter;
            return false;
        }
    }
    return true;
}

EIENLOG_FUNC_IMPL(void) DisableLog()
{
    winux::ScopeGuard guard(__mtxLogWriter);
    if ( __logWriter != nullptr )
    {
        delete __logWriter;
        __logWriter = nullptr;
    }

    if ( __sockLib != nullptr )
    {
        delete __sockLib;
        __sockLib = nullptr;
    }
}

EIENLOG_FUNC_IMPL(void) WriteLog( winux::String const & str )
{
    if ( __logWriter != nullptr )
    {
        winux::ScopeGuard guard(__mtxLogWriter);
        __logWriter->log( winux::Format( $T("[pid:%d, tid:%d] - "), winux::GetPid(), winux::GetTid() ) + winux::AddSlashes( str, $T("\t\r\n") ), winux::mxNull, winux::mxNull, eienlog::leUtf8 );
    }
}

EIENLOG_FUNC_IMPL(void) WriteLogBin( void const * data, size_t size )
{
    if ( __logWriter != nullptr )
    {
        winux::ScopeGuard guard(__mtxLogWriter);
        __logWriter->logBin( winux::Buffer( data, size, true ) );
    }
}

EIENLOG_FUNC_IMPL(size_t) LogEx( winux::Buffer const & data, LogFlag flag )
{
    if ( __logWriter != nullptr )
    {
        winux::ScopeGuard guard(__mtxLogWriter);
        return __logWriter->logEx( data, flag );
    }
    return 0;
}

EIENLOG_FUNC_IMPL(size_t) LogEx( winux::Buffer const & data, winux::Mixed const & fgColor, winux::Mixed const & bgColor, winux::uint8 logEncoding, bool isBinary )
{
    if ( __logWriter != nullptr )
    {
        winux::ScopeGuard guard(__mtxLogWriter);
        return __logWriter->logEx( data, fgColor, bgColor, logEncoding, isBinary );
    }
    return 0;
}

EIENLOG_FUNC_IMPL(size_t) Log( winux::String const & str, LogFlag flag )
{
    if ( __logWriter != nullptr )
    {
        winux::ScopeGuard guard(__mtxLogWriter);
        return __logWriter->log( str, flag );
    }
    return 0;
}

EIENLOG_FUNC_IMPL(size_t) Log( winux::String const & str, winux::Mixed const & fgColor, winux::Mixed const & bgColor, winux::uint8 logEncoding )
{
    if ( __logWriter != nullptr )
    {
        winux::ScopeGuard guard(__mtxLogWriter);
        return __logWriter->log( str, fgColor, bgColor, logEncoding );
    }
    return 0;
}

EIENLOG_FUNC_IMPL(size_t) LogBin( winux::Buffer const & data, LogFlag flag )
{
    if ( __logWriter != nullptr )
    {
        winux::ScopeGuard guard(__mtxLogWriter);
        return __logWriter->logBin( data, flag );
    }
    return 0;
}

EIENLOG_FUNC_IMPL(size_t) LogBin( winux::Buffer const & data, winux::Mixed const & fgColor, winux::Mixed const & bgColor )
{
    if ( __logWriter != nullptr )
    {
        winux::ScopeGuard guard(__mtxLogWriter);
        return __logWriter->logBin( data, fgColor, bgColor );
    }
    return 0;
}

winux::ushort __ccaFgColor[] = {
    winux::fgBlack,
    winux::fgNavy,
    winux::fgAtrovirens,
    winux::fgTeal,
    winux::fgMaroon,
    winux::fgPurple,
    winux::fgOlive,
    winux::fgSilver,
    winux::fgGray,
    winux::fgBlue,
    winux::fgGreen,
    winux::fgAqua,
    winux::fgRed,
    winux::fgFuchsia,
    winux::fgYellow,
    winux::fgWhite,
};
winux::ushort __ccaBgColor[] = {
    winux::bgBlack,
    winux::bgNavy,
    winux::bgAtrovirens,
    winux::bgTeal,
    winux::bgMaroon,
    winux::bgPurple,
    winux::bgOlive,
    winux::bgSilver,
    winux::bgGray,
    winux::bgBlue,
    winux::bgGreen,
    winux::bgAqua,
    winux::bgRed,
    winux::bgFuchsia,
    winux::bgYellow,
    winux::bgWhite,
};

winux::ushort __lfcFgColor[] = {
    lfcBlack,
    lfcNavy,
    lfcAtrovirens,
    lfcTeal,
    lfcMaroon,
    lfcPurple,
    lfcOlive,
    lfcSilver,
    lfcGray,
    lfcBlue,
    lfcGreen,
    lfcAqua,
    lfcRed,
    lfcFuchsia,
    lfcYellow,
    lfcWhite,
};
winux::ushort __lbcBgColor[] = {
    lbcBlack,
    lbcNavy,
    lbcAtrovirens,
    lbcTeal,
    lbcMaroon,
    lbcPurple,
    lbcOlive,
    lbcSilver,
    lbcGray,
    lbcBlue,
    lbcGreen,
    lbcAqua,
    lbcRed,
    lbcFuchsia,
    lbcYellow,
    lbcWhite,
};

EIENLOG_FUNC_IMPL(winux::ushort) GetCcaFlagsFromVcaFlags( winux::ushort vcaFlags )
{
    winux::byte fg = ( vcaFlags & 0x0F );
    winux::byte bg = ( ( vcaFlags >> 4 ) & 0x0F );
    return ( vcaFlags & 0xFF00 ) | __ccaBgColor[bg] | __ccaFgColor[fg];
}

EIENLOG_FUNC_IMPL(LogFlag) GetLogFlagFromVcaFlags( winux::ushort vcaFlags )
{
    winux::byte fg = ( vcaFlags & 0x0F );
    winux::byte bg = ( ( vcaFlags >> 4 ) & 0x0F );
    return LogFlag( ( vcaFlags & vcaFgIgnore ) != vcaFgIgnore, __lfcFgColor[fg], ( vcaFlags & vcaBgIgnore ) != vcaBgIgnore, __lbcBgColor[bg], leUtf8, false );
}


} // namespace eienlog
