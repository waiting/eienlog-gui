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
#endif


namespace eienlog
{
// 根据数据创建一系列分块
static std::vector< winux::Packet<LogChunk> > _BuildChunks( winux::Buffer const & data, time_t utcTime, winux::uint32 flag, winux::uint16 chunkSize )
{
    std::vector< winux::Packet<LogChunk> > packs;
    // 日志空间，每个数据包可以容纳的日志数据
    winux::uint16 logSpaceSize = chunkSize - sizeof(LogChunkHeader);
    size_t remainingSize = data.size();
    size_t chunks = (size_t)ceil( data.size() / (double)logSpaceSize );
    winux::uint32 index = 0;
    while ( remainingSize > logSpaceSize )
    {
        winux::Packet<LogChunk> pack(chunkSize);
        pack->packetSize = chunkSize;
        pack->flag = flag;
        pack->index = index++;
        pack->chunks = chunks;
        pack->realLen = logSpaceSize;
        pack->utcTime = utcTime;
        memcpy( pack->logSpace, data.get<winux::byte>() + ( data.size() - remainingSize ), logSpaceSize );
        packs.push_back( std::move(pack) );

        remainingSize -= logSpaceSize;
    }
    if ( true )
    {
        winux::Packet<LogChunk> pack(chunkSize);
        pack->packetSize = chunkSize;
        pack->flag = flag;
        pack->index = index++;
        pack->chunks = chunks;
        pack->realLen = (winux::uint16)remainingSize;
        pack->utcTime = utcTime;
        memcpy( pack->logSpace, data.get<winux::byte>() + ( data.size() - remainingSize ), remainingSize );
        packs.push_back( std::move(pack) );
    }
    return packs;
}

// 通过封包还原日志数据
static bool _ResumeRecord( std::vector< winux::Packet<LogChunk> > const & packs, LogRecord * record, winux::uint16 chunkSize )
{
    // 日志空间，每个数据包可以容纳的日志数据
    size_t const logSpaceSize = chunkSize - sizeof(LogChunkHeader);
    if ( packs.size() > 0 )
    {
        LogChunk * chunk = packs[0].get();
        record->data.alloc( chunk->chunks * logSpaceSize );
        record->flag = chunk->flag;
        record->utcTime = chunk->utcTime;
    }
    size_t n = 0;
    for ( auto && pack : packs )
    {
        LogChunk * chunk = pack.get();
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

size_t LogWriter::logEx( winux::Buffer const & data, bool useFgColor, winux::uint16 fgColor, bool useBgColor, winux::uint16 bgColor, winux::uint8 logEncoding, bool isBinary )
{
    LogFlag flag;
    flag.fgColorUse = useFgColor;
    flag.fgColor = fgColor;
    flag.bgColorUse = useBgColor;
    flag.bgColor = bgColor;
    flag.logEncoding = logEncoding;
    flag.binary = isBinary;

    auto packs = _BuildChunks( data, winux::GetUtcTimeMs(), flag.value, _chunkSize );
    for ( auto && pack : packs )
    {
        _sock.sendTo( _ep, pack );
    }
    return packs.size();
}

size_t LogWriter::log( winux::String const & str, bool useFgColor, winux::uint16 fgColor, bool useBgColor, winux::uint16 bgColor, winux::uint8 logEncoding )
{
    switch ( logEncoding )
    {
    case leUtf8:
        {
        #if defined(_UNICODE) || defined(UNICODE)
            winux::AnsiString mbs = winux::UnicodeConverter(str).toUtf8();
        #else
            winux::AnsiString mbs = winux::LocalToUtf8(str);
        #endif
            return this->logEx( winux::Buffer( mbs.c_str(), mbs.length(), true ), useFgColor, fgColor, useBgColor, bgColor, logEncoding, false );
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
            return this->logEx( winux::Buffer( ustr.c_str(), ustr.length(), true ), useFgColor, fgColor, useBgColor, bgColor, logEncoding, false );
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
            return this->logEx( winux::Buffer( ustr.c_str(), ustr.length(), true ), useFgColor, fgColor, useBgColor, bgColor, logEncoding, false );
        }
        break;
    default: // leLocal
        {
        #if defined(_UNICODE) || defined(UNICODE)
            winux::AnsiString mbs = winux::UnicodeToLocal(str);
            return this->logEx( winux::Buffer( mbs.c_str(), mbs.length(), true ), useFgColor, fgColor, useBgColor, bgColor, logEncoding, false );
        #else
            return this->logEx( winux::Buffer( str.c_str(), str.length(), true ), useFgColor, fgColor, useBgColor, bgColor, logEncoding, false );
        #endif
        }
        break;
    }
}

size_t LogWriter::logBin( winux::Buffer const & data, bool useFgColor, winux::uint16 fgColor, bool useBgColor, winux::uint16 bgColor )
{
    return this->logEx( data, useFgColor, fgColor, useBgColor, bgColor, 0, true );
}

// class LogReader ----------------------------------------------------------------------------
LogReader::LogReader( winux::String const & addr, winux::ushort port, winux::uint16 chunkSize ) : _ep( addr, port ), _chunkSize(chunkSize), _errno(0)
{
    if ( !_sock.bind(_ep) )
        _errno = eiennet::Socket::ErrNo();
}

bool LogReader::readPack( winux::Packet<LogChunk> * pack, eiennet::ip::EndPoint * ep )
{
    pack->alloc(_chunkSize);
    size_t hadBytes = 0;
    do
    {
        int rc = _sock.recvFrom( ep, pack->Buffer::get<winux::byte>() + hadBytes, pack->capacity() - hadBytes );
        if ( rc < 0 ) return false;
        hadBytes += rc;
    } while ( hadBytes < _chunkSize );
    return true;
}

bool LogReader::readRecord( LogRecord * record, time_t waitTimeout, time_t updateTimeout )
{
    while ( true )
    {
        time_t curTime = winux::GetUtcTimeMs();
        while ( _sock.getAvailable() < _chunkSize && winux::GetUtcTimeMs() - curTime < (winux::uint64)waitTimeout )
        {
            eiennet::io::SelectRead(_sock).wait( waitTimeout / 1000.0 );
        }

        curTime = winux::GetUtcTimeMs();

        if ( _sock.getAvailable() < _chunkSize )
        {
            if ( _packsMap.size() == 0 )
            {
                return false;
            }
        }
        else
        {
            winux::Packet<LogChunk> pack;
            eiennet::ip::EndPoint ep;
            if ( this->readPack( &pack, &ep ) )
            {
                auto && packsData = _packsMap[pack->utcTime];
                packsData.lastUpdate = curTime;
                packsData.packs.push_back( std::move(pack) );
            }
        }

        // 检查已收到的分块是否超时或者完整
        for ( auto it = _packsMap.begin(); it != _packsMap.end(); )
        {
            if ( curTime - it->second.lastUpdate > updateTimeout )
            {
                _ResumeRecord( it->second.packs, record, _chunkSize );
                it = _packsMap.erase(it);
                return true;
            }
            else
            {
                if ( it->second.packs.size() == it->second.packs[0]->chunks )
                {
                    _ResumeRecord( it->second.packs, record, _chunkSize );
                    it = _packsMap.erase(it);
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
        __logWriter->log( winux::Format( TEXT("[pid:%d] - "), getpid() ) + winux::AddCSlashes(str), eienlog::leUtf8 );
    }

    //winux::String exeFile;
    //winux::String exePath = winux::FilePath( winux::GetExecutablePath(), &exeFile );
    //winux::File out( exePath + winux::DirSep + winux::FileTitle(exeFile) + TEXT(".log"), TEXT("at") );
    //time_t tt = winux::GetUtcTime();
    //struct tm * t = gmtime(&tt);
    //winux::tchar sz[32] = { 0 };
    //_tcsftime( sz, 32, TEXT("%a, %d %b %Y %H:%M:%S GMT"), t );
    //winux::String log;
    //winux::StringWriter(&log) << winux::Format( TEXT("[pid:%d]"), getpid() ) << sz << TEXT(" - \"") << winux::AddCSlashes(s) << TEXT("\"") << std::endl;
    //out.puts(log);
}

EIENLOG_FUNC_IMPL(void) WriteLogBin( void const * data, size_t size )
{
    if ( __logWriter != nullptr )
    {
        winux::ScopeGuard guard(__mtxLogWriter);
        __logWriter->logBin( winux::Buffer( data, size, true ) );
    }

    /*winux::String exeFile;
    winux::String exePath = winux::FilePath( winux::GetExecutablePath(), &exeFile );
    winux::File out( exePath + winux::DirSep + winux::FileTitle(exeFile) + TEXT(".binlog"), TEXT("ab") );
    out.write( data, size );*/
}

EIENLOG_FUNC_IMPL(size_t) Log( winux::String const & str, winux::uint8 logEncoding )
{
    if ( __logWriter != nullptr )
    {
        winux::ScopeGuard guard(__mtxLogWriter);
        return __logWriter->log( str, logEncoding );
    }
    return 0;
}

EIENLOG_FUNC_IMPL(size_t) LogBin( winux::Buffer const & data )
{
    if ( __logWriter != nullptr )
    {
        winux::ScopeGuard guard(__mtxLogWriter);
        return __logWriter->logBin(data);
    }
    return 0;
}

EIENLOG_FUNC_IMPL(size_t) LogColor( winux::String const & str, winux::Mixed const & fgColor, winux::Mixed const & bgColor, winux::uint8 logEncoding )
{
    if ( __logWriter != nullptr )
    {
        winux::ScopeGuard guard(__mtxLogWriter);
        return __logWriter->logColor( str, fgColor, bgColor, logEncoding );
    }
    return 0;
}

EIENLOG_FUNC_IMPL(size_t) LogBinColor( winux::Buffer const & data, winux::Mixed const & fgColor, winux::Mixed const & bgColor )
{
    if ( __logWriter != nullptr )
    {
        winux::ScopeGuard guard(__mtxLogWriter);
        return __logWriter->logBinColor( data, fgColor, bgColor );
    }
    return 0;
}


} // namespace eienlog
