#include "eienlog.hpp"

namespace eienlog
{
// 根据数据创建一系列分块
static std::vector< winux::Packet<LogChunk> > _BuildChunks( winux::Buffer const & data, time_t utcTime, winux::uint32 flag, size_t chunkSize )
{
    std::vector< winux::Packet<LogChunk> > packs;
    // 日志空间，每个数据包可以容纳的日志数据
    size_t logSpaceSize = chunkSize - sizeof(LogChunkHeader);
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
        memcpy( pack->logSpace, data.get<byte>() + ( data.size() - remainingSize ), logSpaceSize );
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
        pack->realLen = remainingSize;
        pack->utcTime = utcTime;
        memcpy( pack->logSpace, data.get<byte>() + ( data.size() - remainingSize ), remainingSize );
        packs.push_back( std::move(pack) );
    }
    return packs;
}

// 通过封包还原日志数据
static bool _ResumeRecord( std::vector< winux::Packet<LogChunk> > const & packs, LogRecord * record, size_t chunkSize )
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
        memcpy( record->data.get<byte>() + chunk->index * logSpaceSize, chunk->logSpace, chunk->realLen );
        n += chunk->realLen;
    }
    record->data._setSize(n);
    return record->data.capacity() - n < logSpaceSize;
}

// class LogWriter ----------------------------------------------------------------------------
inline static size_t _SendChunks( eiennet::ip::udp::Socket & sock, eiennet::ip::EndPoint const & ep, winux::Buffer const & data, winux::uint32 flag, size_t chunkSize )
{
    auto packs = _BuildChunks( data, winux::GetUtcTimeMs(), flag, chunkSize );
    for ( auto && pack : packs )
    {
        sock.sendTo( ep, pack );
    }
    return packs.size();
}

size_t LogWriter::log( winux::String const & str, winux::uint32 flag )
{
    LogFlag lflag;
    lflag.value = flag;
    lflag.binary = 0;
    return _SendChunks( _sock, _ep, winux::Buffer( str.c_str(), str.length(), true ), lflag.value, _chunkSize );
}

size_t LogWriter::logBin( winux::Buffer const & data, winux::uint32 flag )
{
    LogFlag lflag;
    lflag.value = flag;
    lflag.binary = 1;
    return _SendChunks( _sock, _ep, data, lflag.value, _chunkSize );
}

// class LogReader ----------------------------------------------------------------------------
LogReader::LogReader( winux::String const & host, winux::ushort port, size_t chunkSize ) : _ep( host, port ), _chunkSize(chunkSize), _errno(0)
{
    _sock.bind(_ep);
    _errno = eiennet::Socket::ErrNo();
}

bool LogReader::readPack( winux::Packet<LogChunk> * pack, eiennet::ip::EndPoint * ep )
{
    pack->alloc(_chunkSize);
    size_t hadBytes = 0;
    do
    {
        int rc = _sock.recvFrom( ep, pack->Buffer::get<byte>() + hadBytes, pack->capacity() - hadBytes );
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
        while ( _sock.getAvailable() < _chunkSize && winux::GetUtcTimeMs() - curTime < waitTimeout )
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


} // namespace eienlog
