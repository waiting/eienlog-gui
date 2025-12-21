#include "system_detection.inl"

#if defined(OS_WIN)

    #define FD_SETSIZE 1024
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <mswsock.h>

    #define socket_errno (WSAGetLastError())
    // in6_addr
    #define s6_addr16 s6_words
#else

    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>

    #include <pthread.h>

    #define ioctlsocket ::ioctl
    #define closesocket ::close
    #define socket_errno (errno)
    #define SOCKET_ERROR (-1)
    #define INVALID_SOCKET (~0)
#endif

#include "eiennet_base.hpp"
#include "eiennet_socket.hpp"
#include "eiennet_io.hpp"
#include "eiennet_async.hpp"
#include "eiennet_io_iocp.hpp"

namespace io
{
namespace iocp
{
// IoSocketCtx 超时处理 ------------------------------------------------------------------------
void _IoSocketCtxTimeoutCallback( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx )
{
    auto * ctx = timerCtx->assocCtx;
    if ( ctx )
    {
        ctx->timerCtx = nullptr; // 取消关联的超时timer场景
        ctx->changeState(stateTimeoutCancel); // 超时取消操作
    }
}

// IoSocketCtx 断开超时事件关联，并取消超时定时器IO ------------------------------------------------
void _IoSocketCtxResetTimerCtx( io::IoSocketCtx * ctx )
{
    if ( ctx->timerCtx ) // 有超时处理
    {
        auto timer = ctx->timerCtx->timer; // 定时器对象
        ctx->timerCtx->assocCtx = nullptr;
        if ( timer->stop() ) ctx->timerCtx->decRef();
        ctx->timerCtx = nullptr;
    }
}

// 处理取消的IO操作 -----------------------------------------------------------------------------
void _ProcessCanceledIoCtx( IoCtx * assocCtx )
{
    if ( assocCtx->state == stateTimeoutCancel ) // 超时导致的操作取消，调用超时回调函数
    {
        switch ( assocCtx->type )
        {
        case ioAccept:
            {
                auto * ctx = static_cast<IoAcceptCtx *>(assocCtx);
                if ( ctx->cbTimeout )
                {
                    if ( ctx->cbTimeout( ctx->sock, ctx ) )
                    {
                        ctx->sock->acceptAsync( ctx->cbOk, ctx->timeoutMs, ctx->cbTimeout, ctx->sock->getThread() );
                    }
                }
                else
                {
                    ctx->sock->acceptAsync( ctx->cbOk, ctx->timeoutMs, ctx->cbTimeout, ctx->sock->getThread() );
                }
            }
            break;
        case ioConnect:
            {
                auto * ctx = static_cast<IoConnectCtx *>(assocCtx);
                if ( ctx->cbTimeout ) ctx->cbTimeout( ctx->sock, ctx );
            }
            break;
        case ioRecv:
            {
                auto * ctx = static_cast<IoRecvCtx *>(assocCtx);
                if ( ctx->cbTimeout ) ctx->cbTimeout( ctx->sock, ctx );
            }
            break;
        case ioSend:
            {
                auto * ctx = static_cast<IoSendCtx *>(assocCtx);
                if ( ctx->cbTimeout ) ctx->cbTimeout( ctx->sock, ctx );
            }
            break;
        case ioRecvFrom:
            {
                auto * ctx = static_cast<IoRecvFromCtx *>(assocCtx);
                if ( ctx->cbTimeout ) ctx->cbTimeout( ctx->sock, ctx );
            }
            break;
        case ioSendTo:
            {
                auto * ctx = static_cast<IoSendToCtx *>(assocCtx);
                if ( ctx->cbTimeout ) ctx->cbTimeout( ctx->sock, ctx );
            }
            break;
        }
    }
}

// --------------------------------------------------------------------------------------------
void _PostRecv( IoService * serv, IoRecvCtx * ctx );
void _PostSend( IoService * serv, IoSendCtx * ctx );
void _PostRecvFrom( IoService * serv, IoRecvFromCtx * ctx );
void _PostSendTo( IoService * serv, IoSendToCtx * ctx );

// IOCP工作函数 --------------------------------------------------------------------------------
void _IocpWorkerFunc( IoService * serv, IoServiceThread * thread, Iocp * iocp )
{
    DWORD err = 0;
    LPOVERLAPPED ol;
    BOOL b;
    do
    {
        DWORD bytesTransferred;
        ULONG_PTR key;
        b = GetQueuedCompletionStatus( iocp->get(), &bytesTransferred, &key, &ol, INFINITE );
        err = GetLastError();
        if ( ol )
        {
            IoCtx * ioCtx = CONTAINING_RECORD( ol, IoCtx, ol );
            if ( b )
            {
                winux::ColorOutputLine( winux::fgFuchsia, "[tid:", winux::GetTid(), "] transferred=", bytesTransferred, ", type=", ioCtx->type, ", weight=", thread ? (winux::ssize_t)thread->getWeight() : -1, ", err=", err );
                if ( err != ERROR_OPERATION_ABORTED )
                {
                    switch ( ioCtx->type )
                    {
                    case ioTimer:
                        {
                            auto * timerCtx = static_cast<IoTimerCtx *>(ioCtx);
                            auto timer = timerCtx->timer; // 定时器对象
                            if ( timerCtx->cbOk )
                            {
                                timerCtx->cbOk( timer, timerCtx ); // 调用回调函数
                            }

                            {
                                winux::ScopeGuard guard( timer->getMutex() );
                                if ( timerCtx->periodic == false ) // 非周期
                                {
                                    {
                                        winux::ScopeGuard unguard( timer->getMutex() );
                                        timer->unset();
                                    }
                                    timer->_timerCtx = nullptr;

                                    // 已处理，释放
                                    timerCtx->decRef();
                                }
                                else
                                {
                                    timer->_posted = false;
                                }
                            }
                        }
                        break;
                    case ioAccept:
                        {
                            auto * ctx = static_cast<IoAcceptCtx *>(ioCtx);
                            _IoSocketCtxResetTimerCtx(ctx);

                            if ( ctx->cbOk )
                            {
                                eiennet::ip::EndPoint ep0( ctx->outputBuf.get<winux::byte>(), ctx->localAddrLen );
                                eiennet::ip::EndPoint ep1( ctx->outputBuf.get<winux::byte>() + ctx->localAddrLen, ctx->remoteAddrLen );
                                //winux::ColorOutputLine( winux::fgFuchsia, ep0, ", ", ep1 );
                                if ( ctx->cbOk( ctx->sock, ctx->clientSock, ep1 ) )
                                {
                                    ctx->sock->acceptAsync( ctx->cbOk, ctx->timeoutMs, ctx->cbTimeout, ctx->sock->getThread() );
                                }
                            }
                            else
                            {
                                ctx->sock->acceptAsync( ctx->cbOk, ctx->timeoutMs, ctx->cbTimeout, ctx->sock->getThread() );
                            }

                            ctx->decRef();
                        }
                        break;
                    case ioConnect:
                        {
                            auto * ctx = static_cast<IoConnectCtx *>(ioCtx);
                            _IoSocketCtxResetTimerCtx(ctx);

                            ctx->costTimeMs = winux::GetUtcTimeMs() - ctx->startTime;
                            if ( ctx->cbOk )
                            {
                                ctx->cbOk( ctx->sock, ctx->costTimeMs );
                            }

                            ctx->decRef();
                        }
                        break;
                    case ioRecv:
                        {
                            auto * ctx = static_cast<IoRecvCtx *>(ioCtx);
                            _IoSocketCtxResetTimerCtx(ctx);

                            // 因为是完成端口，数据自动存放到ctx->wsabuf中，不用手动接收

                            ctx->cnnAvail = bytesTransferred > 0;
                            if ( ctx->cnnAvail )
                            {
                                ctx->hadBytes += bytesTransferred;
                                ctx->data._setSize(ctx->hadBytes);
                            }

                            if ( ctx->hadBytes >= ctx->targetBytes || !ctx->cnnAvail )
                            {
                                // 处理回调
                                if ( ctx->cbOk )
                                {
                                    ctx->cbOk( ctx->sock, ctx->data, ctx->cnnAvail );
                                }

                                ctx->decRef();
                            }
                            else
                            {
                                ctx->startTime = winux::GetUtcTimeMs();
                                _PostRecv( serv, ctx );
                            }
                        }
                        break;
                    case ioSend:
                        {
                            auto * ctx = static_cast<IoSendCtx *>(ioCtx);
                            _IoSocketCtxResetTimerCtx(ctx);

                            // 因为是完成端口，不用手动发送

                            ctx->cnnAvail = true;
                            ctx->costTimeMs += winux::GetUtcTimeMs() - ctx->startTime;

                            if ( ctx->hadBytes < ctx->data.size() )
                            {
                                // `bytesTransferred`为已发送数据量
                                if ( bytesTransferred > 0 )
                                {
                                    ctx->hadBytes += bytesTransferred;
                                }
                                else
                                {
                                    ctx->cnnAvail = false;
                                }
                            }

                            if ( ctx->hadBytes >= ctx->data.size() || !ctx->cnnAvail )
                            {
                                // 处理回调
                                if ( ctx->cbOk )
                                {
                                    ctx->cbOk( ctx->sock, ctx->hadBytes, ctx->costTimeMs, ctx->cnnAvail );
                                }

                                ctx->decRef();
                            }
                            else
                            {
                                ctx->startTime = winux::GetUtcTimeMs();
                                _PostSend( serv, ctx );
                            }
                        }
                        break;
                    case ioRecvFrom:
                        {
                            auto * ctx = static_cast<IoRecvFromCtx *>(ioCtx);
                            _IoSocketCtxResetTimerCtx(ctx);

                            // 因为是完成端口，数据自动存放到ctx->wsabuf中，不用手动接收

                            bool ok = bytesTransferred > 0;
                            if ( ok )
                            {
                                ctx->hadBytes += bytesTransferred;
                                ctx->data._setSize(ctx->hadBytes);
                            }

                            if ( ctx->hadBytes >= ctx->targetBytes || !ok )
                            {
                                // 处理回调
                                if ( ctx->cbOk )
                                {
                                    ctx->cbOk( ctx->sock, ctx->data, ctx->epFrom );
                                }

                                ctx->decRef();
                            }
                            else
                            {
                                ctx->startTime = winux::GetUtcTimeMs();
                                _PostRecvFrom( serv, ctx );
                            }
                        }
                        break;
                    case ioSendTo:
                        {
                            auto * ctx = static_cast<IoSendToCtx *>(ioCtx);
                            _IoSocketCtxResetTimerCtx(ctx);

                            // 因为是完成端口，不用手动发送

                            bool ok = true;
                            ctx->costTimeMs += winux::GetUtcTimeMs() - ctx->startTime;

                            if ( ctx->hadBytes < ctx->data.size() )
                            {
                                // `bytesTransferred`为已发送数据量
                                if ( bytesTransferred > 0 )
                                {
                                    ctx->hadBytes += bytesTransferred;
                                }
                                else
                                {
                                    ok = false;
                                }
                            }

                            if ( ctx->hadBytes >= ctx->data.size() || !ok )
                            {
                                // 处理回调
                                if ( ctx->cbOk )
                                {
                                    ctx->cbOk( ctx->sock, ctx->hadBytes, ctx->costTimeMs );
                                }

                                ctx->decRef();
                            }
                            else
                            {
                                ctx->startTime = winux::GetUtcTimeMs();
                                _PostSendTo( serv, ctx );
                            }
                        }
                        break;
                    default:
                        {
                            ColorOutputLine( winux::fgRed, "Unknown" );
                            ioCtx->decRef();
                        }
                        break;
                    }
                }
                else // err == ERROR_OPERATION_ABORTED
                {
                    _ProcessCanceledIoCtx(ioCtx);

                    // 操作已经完成，但已取消，释放
                    ioCtx->decRef();
                }
            }
            else // b == FALSE
            {
                winux::ColorOutputLine( winux::fgRed, "[tid:", winux::GetTid(), "] transferred=", bytesTransferred, ", type=", ioCtx->type, ", weight=", thread ? (winux::ssize_t)thread->getWeight() : -1, ", err=", err );

                if ( ioCtx->type != ioTimer ) // 非IoTimerCtx
                {
                    // IoSocketCtx取消关联的IoTimerCtx
                    auto * sockIoCtx = dynamic_cast<IoSocketCtx *>(ioCtx);
                    _IoSocketCtxResetTimerCtx(sockIoCtx);
                }

                if ( err == ERROR_OPERATION_ABORTED ) // 操作取消
                {
                    _ProcessCanceledIoCtx(ioCtx);
                }

                // 操作取消或者其他错误，直接释放
                ioCtx->decRef();
            }
        }
        else // ol == nullptr
        {
            if ( bytesTransferred == 0 && key == 0 && ol == nullptr ) // 是退出信号
            {
                break;
            }
        }
    }
    while ( b || ol || err != ERROR_ABANDONED_WAIT_0 ); // 如果获取成功或者不是放弃等待，继续进行下一次获取
}

// struct Iocp_Data ---------------------------------------------------------------------------
struct Iocp_Data
{
    LPFN_ACCEPTEX AcceptEx;
    LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockAddrs;
    LPFN_CONNECTEX ConnectEx;
    LPFN_DISCONNECTEX DisconnectEx;
    HANDLE _hIocp;

    Iocp_Data() : AcceptEx(nullptr), GetAcceptExSockAddrs(nullptr), ConnectEx(nullptr), DisconnectEx(nullptr), _hIocp(nullptr)
    {
    }
};

// class Iocp ---------------------------------------------------------------------------------
Iocp::Iocp()
{
    _self->_hIocp = CreateIoCompletionPort( INVALID_HANDLE_VALUE, nullptr, 0, 0 );
}

Iocp::~Iocp()
{
    if ( _self->_hIocp ) CloseHandle(_self->_hIocp);
}

bool Iocp::initFuncs()
{
    winux::SimpleHandle<SOCKET> sockHandle( WSASocket( AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED ), INVALID_SOCKET, closesocket ); // 仅用于初始化函数指针
    DWORD dwBytes = 0;
    // 获取AcceptEx函数指针
    GUID guidFnAcceptEx = WSAID_ACCEPTEX;
    if ( SOCKET_ERROR == WSAIoctl(
        sockHandle.get(),
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidFnAcceptEx,
        sizeof(guidFnAcceptEx),
        &this->_self->AcceptEx,
        sizeof(this->_self->AcceptEx),
        &dwBytes,
        nullptr,
        nullptr
    ) )
    {
        DWORD err = WSAGetLastError();
        winux::ColorOutputLine( winux::fgRed, "WSAIoctl() fails to get AcceptEx() function pointer, err:", err );
        return false;
    }

    // 获取GetAcceptExSockAddrs函数指针
    GUID guidFnGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;
    if ( SOCKET_ERROR == WSAIoctl(
        sockHandle.get(),
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidFnGetAcceptExSockAddrs,
        sizeof(guidFnGetAcceptExSockAddrs),
        &this->_self->GetAcceptExSockAddrs,
        sizeof(this->_self->GetAcceptExSockAddrs),
        &dwBytes,
        nullptr,
        nullptr
    ) )
    {
        DWORD err = WSAGetLastError();
        winux::ColorOutputLine( winux::fgRed, "WSAIoctl() fails to get GuidGetAcceptExSockAddrs() function pointer, err:", err );
        return false;
    }

    // 获取ConnectEx函数指针
    GUID guidFnConnectEx = WSAID_CONNECTEX;
    if ( SOCKET_ERROR == WSAIoctl(
        sockHandle.get(),
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidFnConnectEx,
        sizeof(guidFnConnectEx),
        &this->_self->ConnectEx,
        sizeof(this->_self->ConnectEx),
        &dwBytes,
        nullptr,
        nullptr
    ) )
    {
        DWORD err = WSAGetLastError();
        winux::ColorOutputLine( winux::fgRed, "WSAIoctl() fails to get ConnectEx() function pointer, err:", err );
        return false;
    }

    // 获取DisconnectEx函数指针
    GUID guidFnDisconnectEx = WSAID_DISCONNECTEX;
    if ( SOCKET_ERROR == WSAIoctl(
        sockHandle.get(),
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidFnDisconnectEx,
        sizeof(guidFnDisconnectEx),
        &this->_self->DisconnectEx,
        sizeof(this->_self->DisconnectEx),
        &dwBytes,
        nullptr,
        nullptr
    ) )
    {
        DWORD err = WSAGetLastError();
        winux::ColorOutputLine( winux::fgRed, "WSAIoctl() fails to get DisconnectEx() function pointer, err:", err );
        return false;
    }
    return true;
}

bool Iocp::associate( HANDLE h, ULONG_PTR key )
{
    return CreateIoCompletionPort( h, _self->_hIocp, key, 0 ) == _self->_hIocp;
}

void Iocp::postCustom( DWORD bytesTransferred, ULONG_PTR key, LPOVERLAPPED ol )
{
    PostQueuedCompletionStatus( _self->_hIocp, bytesTransferred, key, ol );
}

HANDLE Iocp::get() const
{
    return _self->_hIocp;
}

Iocp::operator bool() const
{
    return _self->_hIocp != nullptr;
}


// class IoServiceThread ----------------------------------------------------------------------
void IoServiceThread::run()
{
    _IocpWorkerFunc( this->_serv, this, &this->_iocp );
}

void IoServiceThread::timerTrigger( io::IoTimerCtx * timerCtx )
{
    auto myTimerCtx = static_cast<IoTimerCtx *>(timerCtx);
    auto timer = myTimerCtx->timer;
    this->_iocp.postCustom( 0, (ULONG_PTR)timer->get(), &myTimerCtx->ol );
}


// class IoService ----------------------------------------------------------------------------
IoService::IoService( size_t groupThread )
{
    // 初始化IOCP函数
    this->_iocp.initFuncs();
    // 创建工作线程组
    this->_group.create<IoServiceThread>( groupThread, this );

}

void IoService::stop()
{
    this->_iocp.postCustom( 0, 0, nullptr );
    for ( size_t i = 0; i < _group.count(); i++ )
    {
        // 给每个线程投递退出信号
        auto * th = this->getGroupThread(i);
        th->_iocp.postCustom( 0, 0, nullptr );
    }
}

int IoService::run()
{
    this->_group.startup();
    _IocpWorkerFunc( this, nullptr, &this->_iocp );
    this->_group.wait();

    return 0;
}

void IoService::postAccept( winux::SharedPointer<eiennet::async::Socket> sock, IoAcceptCtx::OkFn cbOk, winux::uint64 timeoutMs, IoAcceptCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    if ( !this->associate( sock, th ) ) return;

    auto * ctx = IoAcceptCtx::New();
    ctx->timeoutMs = timeoutMs;
    ctx->sock = sock;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;

    // 获取绑定IP
    eiennet::ip::EndPoint ep;
    sock->getBoundEp(&ep);

    // 创建现成的socket
    ctx->clientSock = eiennet::async::Socket::New( *this, ep.getAddrFamily(), eiennet::async::Socket::sockStream, eiennet::async::Socket::protoUnspec );
    ctx->clientSock->create();

    DWORD dw;
    BOOL b;
    ctx->localAddrLen = ep.size() + 16;
    ctx->remoteAddrLen = ep.size() + 16;
    ctx->outputBuf.alloc( ctx->localAddrLen + ctx->remoteAddrLen );

    Iocp & iocp = sock->getThread() ? ((IoServiceThread*)sock->getThread())->_iocp : this->_iocp;
    // 投递 IO
    b = iocp._self->AcceptEx(
        sock->get(),
        ctx->clientSock->get(),
        ctx->outputBuf.get(),
        0,
        ctx->localAddrLen,
        ctx->remoteAddrLen,
        &dw,
        &ctx->ol
    );
    // 检测是否真的投递IO到iocp
    if ( b == FALSE )
    {
        DWORD err = WSAGetLastError();
        if ( err && err != ERROR_IO_PENDING )
        {
            // 其他错误，释放IoCtx
            ctx->decRef();
            return;
        }
    }
    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*this)->waitAsyncEx(
            timeoutMs,
            false,
            _IoSocketCtxTimeoutCallback,
            ctx,
            ctx->sock->getThread()
        );
    }
}

void IoService::postConnect( winux::SharedPointer<eiennet::async::Socket> sock, eiennet::EndPoint const & ep, IoConnectCtx::OkFn cbOk, winux::uint64 timeoutMs, IoConnectCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    if ( !sock->_tryCreate( ep.getAddrFamily(), true, eiennet::async::Socket::sockStream, true, eiennet::async::Socket::protoUnspec, false ) ) return;

    if ( !this->associate( sock, th ) ) return;

    auto * ctx = IoConnectCtx::New();
    ctx->timeoutMs = timeoutMs;
    ctx->sock = sock;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    // 绑定一个IP
    sock->bind( eiennet::ip::EndPoint( ep.getAddrFamily() ) );
    BOOL b;
    Iocp & iocp = sock->getThread() ? ((IoServiceThread*)sock->getThread())->_iocp : this->_iocp;
    // 投递 IO
    b = iocp._self->ConnectEx(
        sock->get(),
        ep.get<sockaddr>(),
        ep.size(),
        nullptr,
        0,
        nullptr,
        &ctx->ol
    );
    // 检测是否真的投递IO到iocp
    if ( b == FALSE )
    {
        DWORD err = WSAGetLastError();
        if ( err && err != ERROR_IO_PENDING )
        {
            // 其他错误，释放IoCtx
            ctx->decRef();
            return;
        }
    }
    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*this)->waitAsyncEx(
            timeoutMs,
            false,
            _IoSocketCtxTimeoutCallback,
            ctx,
            ctx->sock->getThread()
        );
    }
}

void _PostRecv( IoService * serv, IoRecvCtx * ctx )
{
    DWORD dwBytes = 0;
    DWORD flag = 0;

    ctx->wsabuf.len = (ULONG)( ctx->data.capacity() - ctx->hadBytes );
    ctx->wsabuf.buf = ctx->data.getAt<CHAR>(ctx->hadBytes);

    // 投递 IO
    int rc;
    rc = WSARecv(
        ctx->sock->get(),
        &ctx->wsabuf,
        1,
        &dwBytes,
        &flag,
        &ctx->ol,
        nullptr
    );

    // 检测是否真的投递IO到iocp
    if ( rc == SOCKET_ERROR )
    {
        DWORD err = WSAGetLastError();
        if ( err && err != WSA_IO_PENDING )
        {
            // 其他错误，释放IoCtx
            ctx->decRef();
            return;
        }
    }

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*serv)->waitAsyncEx(
            ctx->timeoutMs,
            false,
            _IoSocketCtxTimeoutCallback,
            ctx,
            ctx->sock->getThread()
        );
    }
}

void IoService::postRecv( winux::SharedPointer<eiennet::async::Socket> sock, size_t targetSize, IoRecvCtx::OkFn cbOk, winux::uint64 timeoutMs, IoRecvCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    if ( !this->associate( sock, th ) ) return;

    auto * ctx = IoRecvCtx::New();
    ctx->timeoutMs = timeoutMs;
    ctx->sock = sock;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    ctx->targetBytes = targetSize;

    if ( ctx->targetBytes > 0 )
    {
        ctx->data.alloc( ctx->targetBytes, false );
    }
    else
    {
        ctx->data.alloc( 4096, false );
    }

    _PostRecv( this, ctx );
}

void _PostSend( IoService * serv, IoSendCtx * ctx )
{
    DWORD dwBytes = 0;
    DWORD flag = 0;

    ctx->wsabuf.len = (ULONG)( ctx->data.size() - ctx->hadBytes );
    ctx->wsabuf.buf = ctx->data.getAt<CHAR>(ctx->hadBytes);

    // 投递 IO
    int rc;
    rc = WSASend(
        ctx->sock->get(),
        &ctx->wsabuf,
        1,
        &dwBytes,
        flag,
        &ctx->ol,
        nullptr
    );

    // 检测是否真的投递IO到iocp
    if ( rc == SOCKET_ERROR )
    {
        DWORD err = WSAGetLastError();
        if ( err && err != WSA_IO_PENDING )
        {
            // 其他错误，释放IoCtx
            ctx->decRef();
            return;
        }
    }

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*serv)->waitAsyncEx(
            ctx->timeoutMs,
            false,
            _IoSocketCtxTimeoutCallback,
            ctx,
            ctx->sock->getThread()
        );
    }
}

void IoService::postSend( winux::SharedPointer<eiennet::async::Socket> sock, void const * data, size_t size, IoSendCtx::OkFn cbOk, winux::uint64 timeoutMs, IoSendCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    if ( !this->associate( sock, th ) ) return;

    auto * ctx = IoSendCtx::New();
    ctx->timeoutMs = timeoutMs;
    ctx->sock = sock;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    ctx->data.setBuf( data, size, false );

    _PostSend( this, ctx );
}

void _PostRecvFrom( IoService * serv, IoRecvFromCtx * ctx )
{
    DWORD dwBytes = 0;
    DWORD flag = 0;

    ctx->wsabuf.len = (ULONG)( ctx->data.capacity() - ctx->hadBytes );
    ctx->wsabuf.buf = ctx->data.getAt<CHAR>(ctx->hadBytes);

    // 投递 IO
    int rc;
    rc = WSARecvFrom(
        ctx->sock->get(),
        &ctx->wsabuf,
        1,
        &dwBytes,
        &flag,
        ctx->epFrom.get<sockaddr>(),
        (LPINT)&ctx->epFrom.size(),
        &ctx->ol,
        nullptr
    );

    // 检测是否真的投递IO到iocp
    if ( rc == SOCKET_ERROR )
    {
        DWORD err = WSAGetLastError();
        if ( err && err != WSA_IO_PENDING )
        {
            // 其他错误，释放IoCtx
            ctx->decRef();
            return;
        }
    }

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*serv)->waitAsyncEx(
            ctx->timeoutMs,
            false,
            _IoSocketCtxTimeoutCallback,
            ctx,
            ctx->sock->getThread()
        );
    }
}

void IoService::postRecvFrom( winux::SharedPointer<eiennet::async::Socket> sock, size_t targetSize, IoRecvFromCtx::OkFn cbOk, winux::uint64 timeoutMs, IoRecvFromCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    if ( !this->associate( sock, th ) ) return;

    auto * ctx = IoRecvFromCtx::New();
    ctx->timeoutMs = timeoutMs;
    ctx->sock = sock;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    ctx->targetBytes = targetSize;

    if ( ctx->targetBytes > 0 )
    {
        ctx->data.alloc( ctx->targetBytes, false );
    }
    else
    {
        ctx->data.alloc( 4096, false );
    }

    _PostRecvFrom( this, ctx );
}

void _PostSendTo( IoService * serv, IoSendToCtx * ctx )
{
    DWORD dwBytes = 0;
    DWORD flag = 0;

    ctx->wsabuf.len = (ULONG)( ctx->data.size() - ctx->hadBytes );
    ctx->wsabuf.buf = ctx->data.getAt<CHAR>(ctx->hadBytes);

    // 投递 IO
    int rc;
    rc = WSASendTo(
        ctx->sock->get(),
        &ctx->wsabuf,
        1,
        &dwBytes,
        flag,
        ctx->epTo->get<sockaddr>(),
        ctx->epTo->size(),
        &ctx->ol,
        nullptr
    );

    // 检测是否真的投递IO到iocp
    if ( rc == SOCKET_ERROR )
    {
        DWORD err = WSAGetLastError();
        if ( err && err != WSA_IO_PENDING )
        {
            // 其他错误，释放IoCtx
            ctx->decRef();
            return;
        }
    }

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*serv)->waitAsyncEx(
            ctx->timeoutMs,
            false,
            _IoSocketCtxTimeoutCallback,
            ctx,
            ctx->sock->getThread()
        );
    }
}

void IoService::postSendTo( winux::SharedPointer<eiennet::async::Socket> sock, eiennet::EndPoint const & ep, void const * data, size_t size, IoSendToCtx::OkFn cbOk, winux::uint64 timeoutMs, IoSendToCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    if ( !this->associate( sock, th ) ) return;

    auto * ctx = IoSendToCtx::New();
    ctx->timeoutMs = timeoutMs;
    ctx->sock = sock;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    ctx->data.setBuf( data, size, false );
    ctx->epTo.attachNew( ep.clone() );

    _PostSendTo( this, ctx );
}

void IoService::postTimer( winux::SharedPointer<eiennet::async::Timer> timer, winux::uint64 timeoutMs, bool periodic, IoTimerCtx::OkFn cbOk, IoSocketCtx * assocCtx, io::IoServiceThread * th )
{
    IoTimerCtx * timerCtx = nullptr;
    {
        winux::ScopeGuard guard( timer->getMutex() );
        timer->_posted = false;
        if ( timer->_timerCtx )
        {
            timerCtx = static_cast<IoTimerCtx*>(timer->_timerCtx);
        }
        else
        {
            timerCtx = IoTimerCtx::New();
            timer->_timerCtx = timerCtx;
        }
    }

    if ( !timerCtx ) return;

    timerCtx->timeoutMs = timeoutMs;
    timerCtx->cbOk = cbOk; // 回调函数
    timerCtx->timer = timer;
    timerCtx->periodic = periodic;

    if ( assocCtx ) // 关联的IoSocketCtx
    {
        timerCtx->assocCtx = assocCtx;
        assocCtx->timerCtx = timerCtx;
    }

    // 分配处理线程
    timer->setThread( th != (IoServiceThread *)-1 ? th : this->getMinWeightThread() );
    //if ( timer->getThread() ) timer->getThread()->incWeight(); // 增加线程负载权重

    timer->set( timeoutMs, periodic );
}

void IoService::timerTrigger( io::IoTimerCtx * timerCtx )
{
    auto myTimerCtx = static_cast<IoTimerCtx *>(timerCtx);
    auto timer = myTimerCtx->timer;
    this->_iocp.postCustom( 0, (ULONG_PTR)timer->get(), &myTimerCtx->ol );
}

bool IoService::associate( winux::SharedPointer<eiennet::async::Socket> sock, io::IoServiceThread * th )
{
    if ( sock->getThread() == nullptr )
    {
        sock->setThread( th != (IoServiceThread *)-1 ? th : this->getMinWeightThread() );
        auto * th1 = static_cast<IoServiceThread *>( sock->getThread() );
        if ( th1 != nullptr )
        {
            th1->incWeight(); // 增加线程负载权重
            return th1->_iocp.associate( (HANDLE)(INT_PTR)sock->get(), sock->get() );
        }
        else // th1 == nullptr
        {
            this->_iocp.associate( (HANDLE)(INT_PTR)sock->get(), sock->get() );
        }
    }
    return true;
}

IoServiceThread * IoService::getMinWeightThread() const
{
    if ( _group.count() > 0 )
    {
        auto th0 = this->getGroupThread(0);
        for ( size_t i = 1; i < _group.count(); i++ )
        {
            auto th = this->getGroupThread(i);
            if ( th->getWeight() < th0->getWeight() )
            {
                th0 = th;
            }
        }
        return th0;
    }
    return nullptr;
}

IoServiceThread * IoService::getGroupThread( size_t i ) const
{
    return static_cast<IoServiceThread *>( _group.threadAt(i).get() );
}


} // namespace iocp


} // namespace io
