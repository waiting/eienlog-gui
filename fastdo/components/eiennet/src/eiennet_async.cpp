﻿#include "eiennet_base.hpp"
#include "eiennet_socket.hpp"
#include "eiennet_async.hpp"

namespace eiennet
{
// class IoService --------------------------------------------------------------------------
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

void IoService::stop( bool b )
{
    static_cast<volatile bool &>(_stop) = b;
}

void IoService::postAccept( winux::SharedPointer<AsyncSocket> sock, IoAcceptCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoAcceptCtx::TimeoutFunction cbTimeout )
{
    auto ctx = winux::MakeShared(new IoAcceptCtx);
    ctx->timeoutMs = timeoutMs;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    this->_post( sock, ioAccept, ctx );
}

void IoService::postConnect( winux::SharedPointer<AsyncSocket> sock, EndPoint const & ep, IoConnectCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoConnectCtx::TimeoutFunction cbTimeout )
{
    sock->connect(ep);

    auto ctx = winux::MakeShared(new IoConnectCtx);
    ctx->timeoutMs = timeoutMs;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    this->_post( sock, ioConnect, ctx );
}

void IoService::postRecv( winux::SharedPointer<AsyncSocket> sock, size_t targetSize, IoRecvCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoRecvCtx::RecvTimeoutFunction cbTimeout )
{
    auto ctx = winux::MakeShared(new IoRecvCtx);
    ctx->timeoutMs = timeoutMs;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    ctx->targetBytes = targetSize;
    this->_post( sock, ioRecv, ctx );
}

void IoService::postSend( winux::SharedPointer<AsyncSocket> sock, void const * data, size_t size, IoSendCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoSendCtx::TimeoutFunction cbTimeout )
{
    auto ctx = winux::MakeShared(new IoSendCtx);
    ctx->timeoutMs = timeoutMs;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    ctx->data.setBuf( data, size, false );
    this->_post( sock, ioSend, ctx );
}

void IoService::postRecvFrom( winux::SharedPointer<AsyncSocket> sock, size_t targetSize, IoRecvFromCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoRecvFromCtx::TimeoutFunction cbTimeout )
{
    auto ctx = winux::MakeShared(new IoRecvFromCtx);
    ctx->timeoutMs = timeoutMs;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    ctx->targetBytes = targetSize;
    this->_post( sock, ioRecvFrom, ctx );
}

void IoService::postSendTo( winux::SharedPointer<AsyncSocket> sock, EndPoint const & ep, void const * data, size_t size, IoSendToCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoSendToCtx::TimeoutFunction cbTimeout )
{
    auto ctx = winux::MakeShared(new IoSendToCtx);
    ctx->timeoutMs = timeoutMs;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    ctx->data.setBuf( data, size, false );
    ctx->ep.attachNew( ep.clone() );
    this->_post( sock, ioSendTo, ctx );
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
    this->_pool.startup(_threadCount);
    while ( !_stop )
    {
        // Hook1
        this->onRunBeforeJoin();

        io::Select sel;
        bool hasRequest = false;
        {
            // 监听相应的事件操作
            winux::ScopeGuard guard(_mtx);
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
                        if ( isTimeout )
                        {
                            switch ( pr.first )
                            {
                            case ioAccept:
                                {
                                    IoAcceptCtx * ctx = static_cast<IoAcceptCtx*>( pr.second.get() );
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
                                    IoConnectCtx * ctx = static_cast<IoConnectCtx*>( pr.second.get() );
                                    ctx->costTimeMs += diff;
                                    // 利用线程池处理这个回调
                                    if ( ctx->cbTimeout ) _pool.task( ctx->cbTimeout, sock, ctx, pr.second ).post();
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
                                }
                                break;
                            case ioRecv:
                                {
                                    IoRecvCtx * ctx = static_cast<IoRecvCtx*>( pr.second.get() );
                                    // 利用线程池处理这个回调
                                    if ( ctx->cbTimeout ) _pool.task( ctx->cbTimeout, sock, ctx, pr.second ).post();
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
                                }
                                break;
                            case ioSend:
                                {
                                    IoSendCtx * ctx = static_cast<IoSendCtx*>( pr.second.get() );
                                    ctx->costTimeMs += diff;
                                    // 利用线程池处理这个回调
                                    if ( ctx->cbTimeout ) _pool.task( ctx->cbTimeout, sock, ctx, pr.second ).post();
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
                                }
                                break;
                            case ioRecvFrom:
                                {
                                    IoRecvFromCtx * ctx = static_cast<IoRecvFromCtx*>( pr.second.get() );
                                    // 利用线程池处理这个回调
                                    if ( ctx->cbTimeout ) _pool.task( ctx->cbTimeout, sock, ctx, pr.second ).post();
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
                                }
                                break;
                            case ioSendTo:
                                {
                                    IoSendToCtx * ctx = static_cast<IoSendToCtx*>( pr.second.get() );
                                    ctx->costTimeMs += diff;
                                    // 利用线程池处理这个回调
                                    if ( ctx->cbTimeout ) _pool.task( ctx->cbTimeout, sock, ctx, pr.second ).post();
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
                                }
                                break;
                            } // switch()
                        }
                        else // isTimeout == false
                        {
                            if ( rc > 0 )
                            {
                                switch ( pr.first )
                                {
                                case ioAccept:
                                    if ( sel.hasReadSock(*sock.get()) )
                                    {
                                        IoAcceptCtx * ctx = static_cast<IoAcceptCtx*>( pr.second.get() );
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
                                        IoConnectCtx * ctx = static_cast<IoConnectCtx*>( pr.second.get() );
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
                                        IoRecvCtx * ctx = static_cast<IoRecvCtx*>( pr.second.get() );

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
                                        ctx->hadBytes += data.getSize();
                                        ctx->cnnAvail = data && data.getSize();

                                        if ( ctx->hadBytes >= ctx->targetBytes || data.getSize() == 0 )
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
                                        IoSendCtx * ctx = static_cast<IoSendCtx*>( pr.second.get() );
                                        ctx->cnnAvail = true;
                                        ctx->costTimeMs += diff;

                                        if ( ctx->hadBytes < ctx->data.getSize() )
                                        {
                                            size_t wantBytes = ctx->data.getSize() - ctx->hadBytes;
                                            int sendBytes = sock->send( ctx->data.getBuf<winux::byte>() + ctx->hadBytes, wantBytes );
                                            if ( sendBytes > 0 )
                                            {
                                                ctx->hadBytes += sendBytes;
                                            }
                                            else // sendBytes <= 0
                                            {
                                                ctx->cnnAvail = false;
                                            }
                                        }

                                        if ( ctx->hadBytes == ctx->data.getSize() || !ctx->cnnAvail )
                                        {
                                            // 利用线程池处理这个回调
                                            if ( ctx->cbOk ) _pool.task( ctx->cbOk, sock, ctx->costTimeMs, ctx->cnnAvail ).post();
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
                                        IoRecvFromCtx * ctx = static_cast<IoRecvFromCtx*>( pr.second.get() );

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
                                        ctx->hadBytes += data.getSize();

                                        if ( ctx->hadBytes >= ctx->targetBytes || data.getSize() == 0 )
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
                                        IoSendToCtx * ctx = static_cast<IoSendToCtx*>( pr.second.get() );
                                        bool fail = false;
                                        ctx->costTimeMs += diff;

                                        if ( ctx->hadBytes < ctx->data.getSize() )
                                        {
                                            size_t wantBytes = ctx->data.getSize() - ctx->hadBytes;
                                            int sendBytes = sock->sendTo( *ctx->ep.get(), ctx->data.getBuf<winux::byte>() + ctx->hadBytes, wantBytes );
                                            if ( sendBytes > 0 )
                                            {
                                                ctx->hadBytes += sendBytes;
                                            }
                                            else // sendBytes <= 0
                                            {
                                                fail = true;
                                            }
                                        }

                                        if ( ctx->hadBytes == ctx->data.getSize() || fail )
                                        {
                                            // 利用线程池处理这个回调
                                            if ( ctx->cbOk ) _pool.task( ctx->cbOk, sock, ctx->costTimeMs ).post();
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
                    if ( itMaps != this->_ioMaps.end() ) ++itMaps;
                }
            }
        }
    }
    _pool.whenEmptyStopAndWait();
    return 0;
}

// class AsyncSocket ------------------------------------------------------------------------
void AsyncSocket::acceptAsync( IoAcceptCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoAcceptCtx::TimeoutFunction cbTimeout )
{
    _ioServ->postAccept( this->sharedFromThis(), cbOk, timeoutMs, cbTimeout );
}

void AsyncSocket::connectAsync( EndPoint const & ep, IoConnectCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoConnectCtx::TimeoutFunction cbTimeout )
{
    _ioServ->postConnect( this->sharedFromThis(), ep, cbOk, timeoutMs, cbTimeout );
}

void AsyncSocket::recvUntilSizeAsync( size_t targetSize, IoRecvCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoRecvCtx::RecvTimeoutFunction cbTimeout )
{
    _ioServ->postRecv( this->sharedFromThis(), targetSize, cbOk, timeoutMs, cbTimeout );
}

void AsyncSocket::sendAsync( void const * data, size_t size, IoSendCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoSendCtx::TimeoutFunction cbTimeout )
{
    _ioServ->postSend( this->sharedFromThis(), data, size, cbOk, timeoutMs, cbTimeout );
}

void AsyncSocket::recvFromUntilSizeAsync( size_t targetSize, IoRecvFromCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoRecvFromCtx::TimeoutFunction cbTimeout )
{
    _ioServ->postRecvFrom( this->sharedFromThis(), targetSize, cbOk, timeoutMs, cbTimeout );
}

void AsyncSocket::sendToAsync( EndPoint const & ep, void const * data, size_t size, IoSendToCtx::OkFunction cbOk, winux::uint64 timeoutMs, IoSendToCtx::TimeoutFunction cbTimeout )
{
    if ( !this->_tryCreate( ep.getAddrFamily(), true, sockDatagram, true, protoUnspec, false ) ) return;
    _ioServ->postSendTo( this->sharedFromThis(), ep, data, size, cbOk, timeoutMs, cbTimeout );
}

} // namespace eiennet