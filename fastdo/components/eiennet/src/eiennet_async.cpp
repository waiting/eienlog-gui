#include "eiennet_base.hpp"
#include "eiennet_socket.hpp"
#include "eiennet_io.hpp"
#include "eiennet_io_select.hpp"
#include "eiennet_async.hpp"

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
                for ( auto itMaps = _ioMaps.begin(); itMaps != _ioMaps.end(); )
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
                            // 就绪数-1
                            rc--;

                            // 如果已经是end则不能再++it
                            if ( itMaps != this->_ioMaps.end() ) ++itMaps;
                            continue;
                        }
                    }

                    // 处理该sock的IO事件
                    for ( auto it = ioMap.begin(); it != ioMap.end(); )
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
                                }
                                break;
                            case ioRecv:
                                {
                                    IoRecvCtx * ctx = static_cast<IoRecvCtx *>( pr.second.get() );
                                    // 利用线程池处理这个回调
                                    if ( ctx->cbTimeout ) _pool.task( ctx->cbTimeout, sock, ctx, pr.second ).post();
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
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
                                }
                                break;
                            case ioRecvFrom:
                                {
                                    IoRecvFromCtx * ctx = static_cast<IoRecvFromCtx *>( pr.second.get() );
                                    // 利用线程池处理这个回调
                                    if ( ctx->cbTimeout ) _pool.task( ctx->cbTimeout, sock, ctx, pr.second ).post();
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
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
                        if ( it != ioMap.end() ) ++it;
                    } // for ( auto it = ioMap.begin(); it != ioMap.end(); )

                    // 如果IO映射表已空，则删除该sock
                    if ( ioMap.empty() ) itMaps = _ioMaps.erase(itMaps);

                    // 如果已经是end则不能再++it
                    if ( itMaps != _ioMaps.end() ) ++itMaps;
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


} // namespace eiennet
