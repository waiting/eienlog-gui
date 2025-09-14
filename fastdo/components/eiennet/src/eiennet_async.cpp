#include "eiennet_base.hpp"
#include "eiennet_socket.hpp"
#include "eiennet_io.hpp"
#include "eiennet_io_select.hpp"
#include "eiennet_async.hpp"

#if defined(OS_WIN)
#else
#include <unistd.h>
#include <sys/timerfd.h>
#endif

namespace eiennet
{
// class IoService ----------------------------------------------------------------------------
IoService::IoService( int threadCount, double serverWait ) : _mtx(true), _cdt(true), _serverWait(0.002), _threadCount(4), _stop(false)
{
    this->init( threadCount, serverWait );
}

IoService::~IoService()
{

}

bool IoService::init( int threadCount, double serverWait )
{
    this->_threadCount = threadCount;
    this->_serverWait = serverWait;
    return true;
}

void IoService::stop()
{
    static_cast<volatile bool &>(_stop) = true;
}

void IoService::_post( winux::SharedPointer<AsyncSocket> sock, IoType type, winux::SharedPointer<IoCtx> ctx )
{
    {
        winux::ScopeGuard guard(_mtx);
        _ioMaps[sock][type] = ctx;
    }
    _cdt.notify();
}

void IoService::getSockIoCount( size_t * sockCount, size_t * ioCount ) const
{
    winux::ScopeGuard guard( const_cast<winux::RecursiveMutex&>(_mtx) );
    ASSIGN_PTR(sockCount) = _ioMaps.size();
    if ( ioCount )
    {
        size_t ioReqCount = 0;
        for ( auto & iomap : _ioMaps )
        {
            ioReqCount += iomap.second.size();
        }
        *ioCount = ioReqCount;
    }
}

void IoService::removeSock( winux::SharedPointer<AsyncSocket> sock )
{
    winux::ScopeGuard guard(_mtx);
    _ioMaps.erase(sock);
}

int IoService::run()
{
    io::Select sel;
    this->_pool.startup(_threadCount);
    this->_stop = false;
    size_t counter = 0;
    while ( !_stop )
    {
        // Hook1
        this->onRunBeforeJoin();

        sel.clear();

        bool hasRequest = false;
        {
            // 监听相应的事件操作
            winux::ScopeGuard guard(_mtx);
            // 输出一些服务器状态信息
            size_t counter2 = static_cast<size_t>( 0.01 / this->_serverWait );
            counter2 = counter2 ? counter2 : 1;
            if ( ++counter % counter2 == 0 )
            {
                winux::ColorOutput(
                    winux::fgWhite,
                    winux::DateTimeL::Current(),
                    ", Total socks:", this->_ioMaps.size(),
                    ", Current tasks:", this->_pool.getTaskCount(),
                    winux::String( 20, ' ' ),
                    "\r"
                );
            }

            if ( hasRequest = _cdt.waitUntil( [this] () { return _ioMaps.size() > 0; }, _mtx, _serverWait ) )
            {
                for ( auto & pr : _ioMaps )
                {
                    IoMapMap::key_type const & sock = pr.first;
                    IoMap & ioMap = pr.second;
                    // 监听错误
                    sel.setExceptSock(*sock.get());

                    // 监听其他IO请求
                    for ( auto & req : ioMap )
                    {
                        switch ( req.first )
                        {
                        case ioAccept:
                            sel.setReadSock(*sock.get());
                            break;
                        case ioConnect:
                            sel.setWriteSock(*sock.get());
                            break;
                        case ioRecv:
                            sel.setReadSock(*sock.get());
                            break;
                        case ioSend:
                            sel.setWriteSock(*sock.get());
                            break;
                        case ioRecvFrom:
                            sel.setReadSock(*sock.get());
                            break;
                        case ioSendTo:
                            sel.setWriteSock(*sock.get());
                            break;
                        }
                    }
                }
            }
        }

        if ( hasRequest )
        {
            int rc = sel.wait(_serverWait);
            // Hook2
            this->onRunAfterWait(rc);
            if ( rc < 0 )
            {
                winux::ColorOutputLine( winux::fgRed, "select err:", Socket::ErrNo() );
            }
            else
            {
                winux::ScopeGuard guard(_mtx);
                bool hasEraseInIoMaps = false;
                for ( auto itMaps = _ioMaps.begin(); itMaps != _ioMaps.end(); hasEraseInIoMaps = false )
                {
                    IoMapMap::key_type const & sock = (*itMaps).first;
                    IoMap & ioMap = (*itMaps).second;
                    if ( rc > 0 )
                    {
                        // 出错处理
                        if ( sel.hasExceptSock(*sock.get()) )
                        {
                            _pool.task( &AsyncSocket::onError, sock.get(), sock ).post();
                            // 删除该sock的所有IO事件
                            itMaps = _ioMaps.erase(itMaps);
                            hasEraseInIoMaps = true;
                            // 就绪数-1
                            rc--;

                            //// 如果已经是end则不能再++it
                            //if ( itMaps != _ioMaps.end() ) ++itMaps;
                            continue;
                        }
                    }

                    // 处理该sock的IO事件
                    bool hasEraseInIoMap = false;
                    for ( auto it = ioMap.begin(); it != ioMap.end(); hasEraseInIoMap = false )
                    {
                        IoMap::value_type & pr = *it;
                        // 是否超时
                        auto diff = winux::GetUtcTimeMs() - pr.second->startTime;
                        bool isTimeout = diff > pr.second->timeoutMs;
                        if ( isTimeout ) // 超时了
                        {
                            switch ( pr.first )
                            {
                            case ioAccept:
                                {
                                    IoAcceptCtx * ctx = static_cast<IoAcceptCtx *>( pr.second.get() );
                                    // 利用线程池处理这个回调
                                    if ( ctx->cbTimeout )
                                    {
                                        _pool.task( [ctx, this] ( winux::SharedPointer<AsyncSocket> servSock, IoMap::value_type & pr ) {
                                            if ( ctx->cbTimeout( servSock, ctx ) ) this->_post( servSock, pr.first, pr.second );
                                        }, sock, pr ).post();
                                    }
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
                                    hasEraseInIoMap = true;
                                }
                                break;
                            case ioConnect:
                                {
                                    IoConnectCtx * ctx = static_cast<IoConnectCtx *>( pr.second.get() );
                                    ctx->costTimeMs += diff;
                                    // 利用线程池处理这个回调
                                    if ( ctx->cbTimeout ) _pool.task( ctx->cbTimeout, sock, ctx, pr.second ).post();
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
                                    hasEraseInIoMap = true;
                                }
                                break;
                            case ioRecv:
                                {
                                    IoRecvCtx * ctx = static_cast<IoRecvCtx *>( pr.second.get() );
                                    // 利用线程池处理这个回调
                                    if ( ctx->cbTimeout ) _pool.task( ctx->cbTimeout, sock, ctx, pr.second ).post();
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
                                    hasEraseInIoMap = true;
                                }
                                break;
                            case ioSend:
                                {
                                    IoSendCtx * ctx = static_cast<IoSendCtx *>( pr.second.get() );
                                    ctx->costTimeMs += diff;
                                    // 利用线程池处理这个回调
                                    if ( ctx->cbTimeout ) _pool.task( ctx->cbTimeout, sock, ctx, pr.second ).post();
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
                                    hasEraseInIoMap = true;
                                }
                                break;
                            case ioRecvFrom:
                                {
                                    IoRecvFromCtx * ctx = static_cast<IoRecvFromCtx *>( pr.second.get() );
                                    // 利用线程池处理这个回调
                                    if ( ctx->cbTimeout ) _pool.task( ctx->cbTimeout, sock, ctx, pr.second ).post();
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
                                    hasEraseInIoMap = true;
                                }
                                break;
                            case ioSendTo:
                                {
                                    IoSendToCtx * ctx = static_cast<IoSendToCtx *>( pr.second.get() );
                                    ctx->costTimeMs += diff;
                                    // 利用线程池处理这个回调
                                    if ( ctx->cbTimeout ) _pool.task( ctx->cbTimeout, sock, ctx, pr.second ).post();
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
                                    hasEraseInIoMap = true;
                                }
                                break;
                            } // switch()
                        }
                        else // 没有超时 isTimeout == false
                        {
                            if ( rc > 0 )
                            {
                                switch ( pr.first )
                                {
                                case ioAccept:
                                    if ( sel.hasReadSock(*sock.get()) )
                                    {
                                        IoAcceptCtx * ctx = static_cast<IoAcceptCtx *>( pr.second.get() );
                                        // 接受客户连接
                                        auto clientSock = sock->accept(&ctx->ep);
                                        // 利用线程池处理这个回调
                                        if ( ctx->cbOk )
                                        {
                                            _pool.task( [ctx, this] ( winux::SharedPointer<AsyncSocket> servSock, winux::SharedPointer<AsyncSocket> clientSock, ip::EndPoint const & ep, IoMap::value_type & pr ) {
                                                if ( ctx->cbOk( servSock, clientSock, ep ) ) this->_post( servSock, pr.first, pr.second );
                                            }, sock, clientSock, ctx->ep, pr ).post();
                                        }
                                        // 已处理，删除这个请求
                                        it = ioMap.erase(it);
                                        hasEraseInIoMap = true;
                                        // 就绪数-1
                                        rc--;
                                    }
                                    break;
                                case ioConnect:
                                    if ( sel.hasWriteSock(*sock.get()) )
                                    {
                                        IoConnectCtx * ctx = static_cast<IoConnectCtx *>( pr.second.get() );
                                        ctx->costTimeMs = diff;
                                        // 利用线程池处理这个回调
                                        if ( ctx->cbOk ) _pool.task( ctx->cbOk, sock, ctx->costTimeMs ).post();
                                        // 已处理，删除这个请求
                                        it = ioMap.erase(it);
                                        hasEraseInIoMap = true;
                                        // 就绪数-1
                                        rc--;
                                    }
                                    break;
                                case ioRecv:
                                    if ( sel.hasReadSock(*sock.get()) )
                                    {
                                        IoRecvCtx * ctx = static_cast<IoRecvCtx *>( pr.second.get() );

                                        size_t wantBytes = 0;
                                        if ( ctx->targetBytes > 0 )
                                        {
                                            wantBytes = ctx->targetBytes - ctx->hadBytes;
                                        }
                                        else
                                        {
                                            wantBytes = sock->getAvailable();
                                        }

                                        winux::Buffer data = sock->recv(wantBytes);
                                        if ( data ) ctx->data.append(data);
                                        ctx->hadBytes += data.size();
                                        ctx->cnnAvail = data && data.size();

                                        if ( ctx->hadBytes >= ctx->targetBytes || data.size() == 0 )
                                        {
                                            // 利用线程池处理这个回调
                                            if ( ctx->cbOk ) _pool.task( ctx->cbOk, sock, std::move(ctx->data), ctx->cnnAvail ).post();
                                            // 已处理，删除这个请求
                                            it = ioMap.erase(it);
                                            hasEraseInIoMap = true;
                                        }
                                        else
                                        {
                                            ctx->startTime = winux::GetUtcTimeMs();
                                        }
                                        // 就绪数-1
                                        rc--;
                                    }
                                    break;
                                case ioSend:
                                    if ( sel.hasWriteSock(*sock.get()) )
                                    {
                                        IoSendCtx * ctx = static_cast<IoSendCtx *>( pr.second.get() );
                                        ctx->cnnAvail = true;
                                        ctx->costTimeMs += diff;

                                        if ( ctx->hadBytes < ctx->data.size() )
                                        {
                                            size_t wantBytes = ctx->data.size() - ctx->hadBytes;
                                            int sendBytes = sock->send( ctx->data.get<winux::byte>() + ctx->hadBytes, wantBytes );
                                            if ( sendBytes > 0 )
                                            {
                                                ctx->hadBytes += sendBytes;
                                            }
                                            else // sendBytes <= 0
                                            {
                                                ctx->cnnAvail = false;
                                            }
                                        }

                                        if ( ctx->hadBytes == ctx->data.size() || !ctx->cnnAvail )
                                        {
                                            // 利用线程池处理这个回调
                                            if ( ctx->cbOk ) _pool.task( ctx->cbOk, sock, ctx->hadBytes, ctx->costTimeMs, ctx->cnnAvail ).post();
                                            // 已处理，删除这个请求
                                            it = ioMap.erase(it);
                                            hasEraseInIoMap = true;
                                        }
                                        else
                                        {
                                            ctx->startTime = winux::GetUtcTimeMs();
                                        }
                                        // 就绪数-1
                                        rc--;
                                    }
                                    break;
                                case ioRecvFrom:
                                    if ( sel.hasReadSock(*sock.get()) )
                                    {
                                        IoRecvFromCtx * ctx = static_cast<IoRecvFromCtx *>( pr.second.get() );

                                        size_t wantBytes = 0;
                                        if ( ctx->targetBytes > 0 )
                                        {
                                            wantBytes = ctx->targetBytes - ctx->hadBytes;
                                        }
                                        else
                                        {
                                            wantBytes = sock->getAvailable();
                                        }

                                        winux::Buffer data = sock->recvFrom( &ctx->epFrom, wantBytes );
                                        if ( data ) ctx->data.append(data);
                                        ctx->hadBytes += data.size();

                                        if ( ctx->hadBytes >= ctx->targetBytes || data.size() == 0 )
                                        {
                                            // 利用线程池处理这个回调
                                            if ( ctx->cbOk ) _pool.task( ctx->cbOk, sock, std::move(ctx->data), std::move(ctx->epFrom) ).post();
                                            // 已处理，删除这个请求
                                            it = ioMap.erase(it);
                                            hasEraseInIoMap = true;
                                        }
                                        else
                                        {
                                            ctx->startTime = winux::GetUtcTimeMs();
                                        }
                                        // 就绪数-1
                                        rc--;
                                    }
                                    break;
                                case ioSendTo:
                                    if ( sel.hasWriteSock(*sock.get()) )
                                    {
                                        IoSendToCtx * ctx = static_cast<IoSendToCtx *>( pr.second.get() );
                                        bool fail = false;
                                        ctx->costTimeMs += diff;

                                        if ( ctx->hadBytes < ctx->data.size() )
                                        {
                                            size_t wantBytes = ctx->data.size() - ctx->hadBytes;
                                            int sendBytes = sock->sendTo( *ctx->ep.get(), ctx->data.get<winux::byte>() + ctx->hadBytes, wantBytes );
                                            if ( sendBytes > 0 )
                                            {
                                                ctx->hadBytes += sendBytes;
                                            }
                                            else // sendBytes <= 0
                                            {
                                                fail = true;
                                            }
                                        }

                                        if ( ctx->hadBytes == ctx->data.size() || fail )
                                        {
                                            // 利用线程池处理这个回调
                                            if ( ctx->cbOk ) _pool.task( ctx->cbOk, sock, ctx->hadBytes, ctx->costTimeMs ).post();
                                            // 已处理，删除这个请求
                                            it = ioMap.erase(it);
                                            hasEraseInIoMap = true;
                                        }
                                        else
                                        {
                                            ctx->startTime = winux::GetUtcTimeMs();
                                        }
                                        // 就绪数-1
                                        rc--;
                                    }
                                    break;
                                }
                            }
                            else if ( rc == 0 )
                            {
                            }
                            else // rc < 0, select() occur error.
                            {
                            }
                        }

                        // 如果已经是end则不能再++it
                        if ( !hasEraseInIoMap && it != ioMap.end() ) ++it;
                    } // for ( auto it = ioMap.begin(); it != ioMap.end(); )

                    // 如果IO映射表已空，则删除该sock
                    if ( ioMap.empty() )
                    {
                        itMaps = _ioMaps.erase(itMaps);
                        hasEraseInIoMaps = true;
                    }

                    // 如果已经是end则不能再++it
                    if ( !hasEraseInIoMaps && itMaps != _ioMaps.end() ) ++itMaps;
                } // for ( auto itMaps = _ioMaps.begin(); itMaps != _ioMaps.end(); )
            }
        }
    }
    _pool.whenEmptyStopAndWait();
    return 0;
}

// class AsyncSocket --------------------------------------------------------------------------
void AsyncSocket::acceptAsync( IoAcceptCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoAcceptCtx::TimeoutFunction cbTimeout )
{
    auto ctx = winux::MakeShared(new IoAcceptCtx);
    ctx->timeoutMs = timeoutMs;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    _ioServ->_post( this->sharedFromThis(), ioAccept, ctx );
}

void AsyncSocket::connectAsync( EndPoint const & ep, IoConnectCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoConnectCtx::TimeoutFunction cbTimeout )
{
    auto sock = this->sharedFromThis();
    sock->connect(ep);

    auto ctx = winux::MakeShared(new IoConnectCtx);
    ctx->timeoutMs = timeoutMs;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    _ioServ->_post( sock, ioConnect, ctx );
}

void AsyncSocket::recvUntilSizeAsync( size_t targetSize, IoRecvCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoRecvCtx::TimeoutFunction cbTimeout )
{
    auto ctx = winux::MakeShared(new IoRecvCtx);
    ctx->timeoutMs = timeoutMs;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    ctx->targetBytes = targetSize;
    _ioServ->_post( this->sharedFromThis(), ioRecv, ctx );
}

void AsyncSocket::sendAsync( void const * data, size_t size, IoSendCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoSendCtx::TimeoutFunction cbTimeout )
{
    auto ctx = winux::MakeShared(new IoSendCtx);
    ctx->timeoutMs = timeoutMs;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    ctx->data.setBuf( data, size, false );
    _ioServ->_post( this->sharedFromThis(), ioSend, ctx );
}

void AsyncSocket::recvFromUntilSizeAsync( size_t targetSize, IoRecvFromCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoRecvFromCtx::TimeoutFunction cbTimeout )
{
    auto ctx = winux::MakeShared(new IoRecvFromCtx);
    ctx->timeoutMs = timeoutMs;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    ctx->targetBytes = targetSize;
    _ioServ->_post( this->sharedFromThis(), ioRecvFrom, ctx );
}

void AsyncSocket::sendToAsync( EndPoint const & ep, void const * data, size_t size, IoSendToCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoSendToCtx::TimeoutFunction cbTimeout )
{
    if ( !this->_tryCreate( ep.getAddrFamily(), true, sockDatagram, true, protoUnspec, false ) ) return;

    auto ctx = winux::MakeShared(new IoSendToCtx);
    ctx->timeoutMs = timeoutMs;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    ctx->data.setBuf( data, size, false );
    ctx->ep.attachNew( ep.clone() );
    _ioServ->_post( this->sharedFromThis(), ioSendTo, ctx );
}

AsyncSocket * AsyncSocket::onCreateClient( IoService & ioServ, int sock, bool isNewSock )
{
    if ( this->_CreateClientHandler )
    {
        return this->_CreateClientHandler( ioServ, sock, isNewSock );
    }
    else
    {
        return new AsyncSocket( ioServ, sock, isNewSock );
    }
}

namespace async
{
// class Socket -------------------------------------------------------------------------------
Socket::Socket( io::IoService & serv, int sock, bool isNewSock ) : eiennet::Socket( sock, isNewSock ), _serv(&serv), _data(nullptr), _thread(nullptr)
{
    this->setBlocking(false);
    //ColorOutputLine( winux::fgAtrovirens, "Socket(Accept)" );
}

Socket::Socket( io::IoService & serv, AddrFamily af, SockType sockType, Protocol proto ) : eiennet::Socket( af, sockType, proto ), _serv(&serv), _data(nullptr), _thread(nullptr)
{
    this->setBlocking(false);
    //ColorOutputLine( winux::fgAtrovirens, "Socket()" );
}

Socket::~Socket()
{
    // 减小线程负载
    if ( this->_thread ) this->_thread->decWeight();
    //ColorOutputLine( winux::fgTeal, "~Socket() attach thread:", _thread );
}

void Socket::acceptAsync( io::IoAcceptCtx::OkFn cbOk, winux::uint64 timeoutMs, io::IoAcceptCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    this->_serv->postAccept( this->sharedFromThis(), cbOk, timeoutMs, cbTimeout, th );
}

void Socket::connectAsync( EndPoint const & ep, io::IoConnectCtx::OkFn cbOk, winux::uint64 timeoutMs, io::IoConnectCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    this->_serv->postConnect( this->sharedFromThis(), ep, cbOk, timeoutMs, cbTimeout, th );
}

void Socket::recvUntilSizeAsync( size_t targetSize, io::IoRecvCtx::OkFn cbOk, winux::uint64 timeoutMs, io::IoRecvCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    this->_serv->postRecv( this->sharedFromThis(), targetSize, cbOk, timeoutMs, cbTimeout, th );
}

void Socket::sendAsync( void const * data, size_t size, io::IoSendCtx::OkFn cbOk, winux::uint64 timeoutMs, io::IoSendCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    this->_serv->postSend( this->sharedFromThis(), data, size, cbOk, timeoutMs, cbTimeout, th );
}

void Socket::recvFromUntilSizeAsync( size_t targetSize, io::IoRecvFromCtx::OkFn cbOk, winux::uint64 timeoutMs, io::IoRecvFromCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    this->_serv->postRecvFrom( this->sharedFromThis(), targetSize, cbOk, timeoutMs, cbTimeout, th );
}

void Socket::sendToAsync( EndPoint const & ep, void const * data, size_t size, io::IoSendToCtx::OkFn cbOk, winux::uint64 timeoutMs, io::IoSendToCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    this->_serv->postSendTo( this->sharedFromThis(), ep, data, size, cbOk, timeoutMs, cbTimeout, th );
}

Socket * Socket::onCreateClient( io::IoService & serv, int sock, bool isNewSock )
{
    if ( this->_CreateClientHandler )
    {
        return this->_CreateClientHandler( serv, sock, isNewSock );
    }
    else
    {
        return new Socket( serv, sock, isNewSock );
    }
}

// struct Timer_Data --------------------------------------------------------------------------
struct Timer_Data
{
#if defined(OS_WIN)
    static void _TimerCallback( PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER pTpTimer );
    PTP_TIMER _pTpTimer;
#else
    int _timerFd;
#endif

    Timer_Data() :
    #if defined(OS_WIN)
        _pTpTimer(nullptr)
    #else
        _timerFd(-1)
    #endif
    {
    }

};

// class Timer --------------------------------------------------------------------------------
Timer::Timer( io::IoService & serv ) : _posted(false), _serv(&serv), _thread(nullptr)
{
    this->create();

    //ColorOutputLine( winux::fgAtrovirens, "Timer(", this, ")" );
}

Timer::~Timer()
{
    // 减小线程负载
    if ( this->_thread ) this->_thread->decWeight();

    this->destroy();

    //ColorOutputLine( winux::fgTeal, "~Timer(", this, ") thread:", _thread );
}

void Timer::create()
{
    this->destroy();
    {
        winux::ScopeGuard guard(this->_mtx);
    #if defined(OS_WIN)
        _self->_pTpTimer = CreateThreadpoolTimer( &Timer_Data::_TimerCallback, this, nullptr );
    #else
        _self->_timerFd = timerfd_create( CLOCK_MONOTONIC, TFD_NONBLOCK );
    #endif
    }
}

void Timer::destroy()
{
    winux::ScopeGuard guard(this->_mtx);
#if defined(OS_WIN)
    if ( _self->_pTpTimer )
    {
        CloseThreadpoolTimer(_self->_pTpTimer);
        _self->_pTpTimer = nullptr;
    }
#else
    if ( _self->_timerFd != -1 )
    {
        close(_self->_timerFd);
        _self->_timerFd = -1;
    }
#endif
}

void Timer::set( winux::uint64 timeoutMs, bool periodic )
{
    winux::ScopeGuard guard(this->_mtx);
#if defined(OS_WIN)
    if ( _self->_pTpTimer )
    {
        union
        {
            LARGE_INTEGER li;
            FILETIME ft;
        } dueTime;
        dueTime.li.QuadPart = timeoutMs ? -( (winux::int64)timeoutMs * 10000 ) : -1;
        SetThreadpoolTimer( _self->_pTpTimer, &dueTime.ft, ( periodic ? ( timeoutMs ? timeoutMs : 1 ) : 0 ), 0 );
    }
#else
    if ( _self->_timerFd != -1 )
    {
        itimerspec tm = { { 0, 0 }, { 0, 0 } };
        tm.it_value.tv_sec = timeoutMs / 1000;
        tm.it_value.tv_nsec = ( timeoutMs % 1000 ) * 1000000;
        if ( periodic )
        {
            tm.it_interval.tv_sec = timeoutMs / 1000;
            tm.it_interval.tv_nsec = ( timeoutMs % 1000 ) * 1000000;
        }
        timerfd_settime( _self->_timerFd, 0, &tm, nullptr );
    }
#endif
}

void Timer::unset()
{
    winux::ScopeGuard guard(this->_mtx);
#if defined(OS_WIN)
    if ( _self->_pTpTimer )
    {
        SetThreadpoolTimer( _self->_pTpTimer, nullptr, 0, 0 );
    }
#else
    if ( _self->_timerFd != -1 )
    {
        itimerspec tm = { { 0, 0 }, { 0, 0 } };
        timerfd_settime( _self->_timerFd, 0, &tm, nullptr );
    }
#endif
}

void Timer::stop( bool timerCtxDecRef )
{
    winux::ScopeGuard guard(this->_mtx);

    if ( this->_timerCtx )
    {
        auto timerCtx = this->_timerCtx.lock();

        {
            winux::ScopeUnguard unguard(this->_mtx);
            timerCtx->cancel(io::cancelProactive);
            this->destroy();
        }

        timerCtx->periodic = false;
        if ( this->_posted == false )
        {
            this->_timerCtx.reset();
            if ( timerCtxDecRef ) timerCtx->decRef();
        }
    }

}

void Timer::waitAsyncEx( winux::uint64 timeoutMs, bool periodic, io::IoTimerCtx::OkFn cbOk, winux::SharedPointer<io::IoSocketCtx> assocCtx, io::IoServiceThread * th )
{
    this->_serv->postTimer( this->sharedFromThis(), timeoutMs, periodic, cbOk, assocCtx, th );
}

intptr_t Timer::get() const
{
#if defined(OS_WIN)
    return (intptr_t)_self->_pTpTimer;
#else
    return (intptr_t)_self->_timerFd;
#endif
}

#if defined(OS_WIN)
void Timer_Data::_TimerCallback( PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER pTpTimer )
{
    auto timer = ((Timer *)Context)->sharedFromThis();
    {
        winux::ScopeGuard guard(timer->_mtx);

        if ( timer->_timerCtx )
        {
            timer->_posted = true;
            auto timerCtx = timer->_timerCtx.lock();
            if ( timerCtx )
            {
                if ( timer->_thread )
                {
                    timer->_thread->timerTrigger( timerCtx.get() );
                }
                else
                {
                    timer->_serv->timerTrigger( timerCtx.get() );
                }
            }
        }
    }
}
#else

#endif

} // namespace async

} // namespace eiennet
