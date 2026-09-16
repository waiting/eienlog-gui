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


namespace iocp
{
// IoSocketCtx 超时处理 ------------------------------------------------------------------------
static void _IoSocketCtxTimeoutCallback( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx, IoEventsData & ioEvents )
{
    auto * assocCtx = timerCtx->assocCtx;
    if ( assocCtx )
    {
        assocCtx->timerCtx = nullptr; // 取消关联的超时timer场景
        assocCtx->cancel(cancelTimeout); // 超时取消操作
        //assocCtx->changeState(stateCancel);
    }
}

// IoSocketCtx 清空超时事件关联，并停止超时定时器，尝试释放IoTimerCtx -------------------------------
static void _IoSocketCtxClearTimerCtx( io::IoSocketCtx * ctx )
{
    if ( ctx->timerCtx ) // 有超时处理
    {
        auto timer = ctx->timerCtx->timer; // 定时器对象
        ctx->timerCtx->assocCtx = nullptr;
        timer->stop();
        ctx->timerCtx = nullptr;
    }
}

// 获取IoCtx对应的fd
inline static int _GetFdByIoCtx( io::IoCtx * ioCtx )
{
    int fd;
    if ( ioCtx->type == ioTimer )
    {
    #if defined(OS_WIN)
        fd = (int)dynamic_cast<io::iocp::IoTimerCtx *>(ioCtx)->timer->get();
    #else
        fd = dynamic_cast<io::IoTimerCtx *>(ioCtx)->timer->get();
    #endif
    }
    else
    {
        fd = dynamic_cast<io::IoSocketCtx *>(ioCtx)->sock->get();
    }
    return fd;
}

// --------------------------------------------------------------------------------------------
bool _PostAccept( IoService * serv, IoAcceptCtx * ctx );
bool _PostConnect( IoService * serv, IoConnectCtx * ctx, eiennet::EndPoint const & ep );
bool _PostRecv( IoService * serv, IoRecvCtx * ctx );
bool _PostSend( IoService * serv, IoSendCtx * ctx );
bool _PostRecvFrom( IoService * serv, IoRecvFromCtx * ctx );
bool _PostSendTo( IoService * serv, IoSendToCtx * ctx );

// IOCP工作函数 --------------------------------------------------------------------------------
void _IocpWorkerFunc( IoService * serv, IoServiceThread * thread, IoEventsData & ioEvents, bool * stop )
{
    DWORD err = 0;
    LPOVERLAPPED ol;
    BOOL b;

    while ( !*stop )
    {
        ioEvents._handleIoCtxsPost();

        DWORD bytesTransferred;
        ULONG_PTR key;
        SetLastError(0);
        b = GetQueuedCompletionStatus( ioEvents._iocp.get(), &bytesTransferred, &key, &ol, INFINITE );
        err = GetLastError();
        if ( ol )
        {
            IoCtx * ioCtx = CONTAINING_RECORD( ol, IoCtx, ol );

            if ( b )
            {
                //winux::ColorOutputLine( winux::fgFuchsia, "[tid:", winux::GetTid(), "] transferred=", bytesTransferred, ", type=", ioCtx->type, ", weight=", thread ? (winux::ssize_t)thread->getWeight() : -1, ", err=", err );
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
                                        winux::ScopeUnguard unguard( timer->getMutex() );
                                        timer->unset();
                                        // 已处理，完成这个请求
                                        timerCtx->changeState(stateFinish);
                                    }
                                    timer->_timerCtx = nullptr;
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
                            _IoSocketCtxClearTimerCtx(ctx);

                            if ( ctx->cbOk )
                            {
                                eiennet::ip::EndPoint epLocal( ctx->outputBuf.get<winux::byte>(), ctx->localAddrLen );
                                ctx->clientEp.init( ctx->outputBuf.get<winux::byte>() + ctx->localAddrLen, ctx->remoteAddrLen );

                                auto clientSock = ctx->clientSock;
                                ctx->clientSock.reset();

                                if ( ctx->cbOk( ctx->sock, clientSock, ctx->clientEp ) )
                                {
                                    ctx->startTime = winux::GetUtcTimeMs();
                                    _PostAccept( serv, ctx );
                                }
                                else
                                {
                                    // 已处理，完成这个请求
                                    ctx->changeState(stateFinish);
                                }
                            }
                            else
                            {
                                ctx->startTime = winux::GetUtcTimeMs();
                                _PostAccept( serv, ctx );
                            }
                        }
                        break;
                    case ioConnect:
                        {
                            auto * ctx = static_cast<IoConnectCtx *>(ioCtx);
                            _IoSocketCtxClearTimerCtx(ctx);

                            ctx->costTimeMs = winux::GetUtcTimeMs() - ctx->startTime;
                            if ( ctx->cbOk )
                            {
                                ctx->cbOk( ctx->sock, ctx->costTimeMs );
                            }

                            // 已处理，完成这个请求
                            ctx->changeState(stateFinish);
                        }
                        break;
                    case ioRecv:
                        {
                            auto * ctx = static_cast<IoRecvCtx *>(ioCtx);
                            _IoSocketCtxClearTimerCtx(ctx);

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

                                // 已处理，完成这个请求
                                ctx->changeState(stateFinish);
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
                            _IoSocketCtxClearTimerCtx(ctx);

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

                                // 已处理，完成这个请求
                                ctx->changeState(stateFinish);
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
                            _IoSocketCtxClearTimerCtx(ctx);

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

                                // 已处理，完成这个请求
                                ctx->changeState(stateFinish);
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
                            _IoSocketCtxClearTimerCtx(ctx);

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

                                // 已处理，完成这个请求
                                ctx->changeState(stateFinish);
                            }
                            else
                            {
                                ctx->startTime = winux::GetUtcTimeMs();
                                _PostSendTo( serv, ctx );
                            }
                        }
                        break;
                    }
                }
                else // err == ERROR_OPERATION_ABORTED
                {
                    // 操作已经完成，但已取消，设置状态为取消
                    ioCtx->changeState(stateCancel);
                }
            }
            else // b == FALSE
            {
                //winux::ColorOutputLine( winux::fgRed, "[tid:", winux::GetTid(), "] transferred=", bytesTransferred, ", type=", ioCtx->type, ", weight=", thread ? (winux::ssize_t)thread->getWeight() : -1, ", err=", err );

                if ( ioCtx->type != ioTimer ) // 非IoTimerCtx
                {
                    // IoSocketCtx取消关联的IoTimerCtx
                    auto * sockIoCtx = dynamic_cast<IoSocketCtx *>(ioCtx);
                    _IoSocketCtxClearTimerCtx(sockIoCtx);
                }

                if ( err == ERROR_OPERATION_ABORTED ) // 操作取消
                {
                    ioCtx->changeState(stateCancel);
                    switch ( ioCtx->cancelType ) // 移除取消
                    {
                    case cancelRemove:
                        switch ( ioCtx->type )
                        {
                        case ioRecv:
                            {
                                auto * recvCtx = dynamic_cast<IoRecvCtx *>(ioCtx);
                                if ( recvCtx->cnnAvail )
                                {
                                    _IoSocketCtxClearTimerCtx(recvCtx);

                                    recvCtx->cnnAvail = false;
                                    recvCtx->data.free();

                                    // 处理回调
                                    if ( recvCtx->cbOk )
                                    {
                                        recvCtx->cbOk( recvCtx->sock, recvCtx->data, recvCtx->cnnAvail );
                                    }

                                    // 已处理，完成这个请求
                                    recvCtx->changeState(stateFinish);
                                }
                            }
                            break;
                        case ioSend:
                            {
                                auto * sendCtx = dynamic_cast<IoSendCtx *>(ioCtx);
                                if ( sendCtx->cnnAvail )
                                {
                                    _IoSocketCtxClearTimerCtx(sendCtx);

                                    sendCtx->cnnAvail = false;
                                    sendCtx->costTimeMs += winux::GetUtcTimeMs() - sendCtx->startTime;

                                    // 处理回调
                                    if ( sendCtx->cbOk )
                                    {
                                        sendCtx->cbOk( sendCtx->sock, sendCtx->hadBytes, sendCtx->costTimeMs, sendCtx->cnnAvail );
                                    }

                                    // 已处理，完成这个请求
                                    sendCtx->changeState(stateFinish);
                                }
                            }
                            break;
                        }
                        break;
                    }
                }
                else // 其他错误
                {
                    winux::ColorOutputLine( winux::fgRed, "[tid:", winux::GetTid(), "] transferred=", bytesTransferred, ", type=", ioCtx->type, ", weight=", thread ? (winux::ssize_t)thread->getWeight() : -1, ", err=", err );
                    ioCtx->changeState(stateError);
                    switch ( ioCtx->type )
                    {
                    case ioRecv:
                        {
                            auto * recvCtx = dynamic_cast<IoRecvCtx *>(ioCtx);
                            if ( recvCtx->cnnAvail )
                            {
                                _IoSocketCtxClearTimerCtx(recvCtx);

                                recvCtx->cnnAvail = false;
                                recvCtx->data.free();

                                // 处理回调
                                if ( recvCtx->cbOk )
                                {
                                    recvCtx->cbOk( recvCtx->sock, recvCtx->data, recvCtx->cnnAvail );
                                }

                                // 已处理，完成这个请求
                                recvCtx->changeState(stateFinish);
                            }
                        }
                        break;
                    case ioSend:
                        {
                            auto * sendCtx = dynamic_cast<IoSendCtx *>(ioCtx);
                            if ( sendCtx->cnnAvail )
                            {
                                _IoSocketCtxClearTimerCtx(sendCtx);

                                sendCtx->cnnAvail = false;
                                sendCtx->costTimeMs += winux::GetUtcTimeMs() - sendCtx->startTime;

                                // 处理回调
                                if ( sendCtx->cbOk )
                                {
                                    sendCtx->cbOk( sendCtx->sock, sendCtx->hadBytes, sendCtx->costTimeMs, sendCtx->cnnAvail );
                                }

                                // 已处理，完成这个请求
                                sendCtx->changeState(stateFinish);
                            }
                        }
                        break;
                    }
                }
            }
        }
        //else // ol == nullptr
        //{
        //    if ( bytesTransferred == 0 && key == 0 && ol == nullptr ) // 是退出信号
        //    {
        //        break;
        //    }
        //}

        ioEvents._handleIoCtxsTimeoutAndDelete();;
    }

    //while ( b || ol || err != ERROR_ABANDONED_WAIT_0 ); // 如果获取成功或者不是放弃等待，继续进行下一次获取

}

// class IoEventsData -------------------------------------------------------------------------
IoEventsData::IoEventsData() : _mtxPreIoCtxs(true), _sockIoCount(0), _timerIoCount(0)
{
    // 初始化IOCP函数
    this->_iocp.initFuncs();
}

void IoEventsData::_handleIoCtxsPost()
{
    winux::ScopeGuard guard(this->_mtxPreIoCtxs);
    for ( auto * ioCtx : this->_preIoCtxs )
    {
        ioCtx->state = stateNormal; // 投递前状态改为正常状态
        this->post(ioCtx);
    }
    this->_preIoCtxs.clear();
}

void IoEventsData::_handleIoCtxsTimeoutAndDelete()
{
    bool hasEraseInIoVecMap = false;
    for ( auto itVecStruct = this->_ioVecMap.begin(); itVecStruct != this->_ioVecMap.end(); hasEraseInIoVecMap = false )
    {
        auto fd = itVecStruct->first;
        auto & ioVecStruct = itVecStruct->second;
        auto & ioVec = itVecStruct->second.ctxs;

        bool hasEraseInIoVec = false;
        for ( auto it = ioVec.begin(); it != ioVec.end(); hasEraseInIoVec = false )
        {
            auto * ioCtx = *it; // 当前IO事件场景

            if ( ioCtx->state != stateNormal ) // 不是正常状态了，说明要么是取消了，或者是已经完成了，这些都需要删除掉
            {
                if ( ioCtx->type == ioTimer ) // Timer的事件处理
                {
                    it = ioVec.erase(it); // 删除已取消或者已完成的IO事件
                    hasEraseInIoVec = true;
                    // 删除这个IoCtx
                    ioCtx->decRef();

                    this->_timerIoCount--;
                }
                else // Socket的事件处理
                {
                    auto * sockCtx = dynamic_cast<IoSocketCtx *>(ioCtx);
                    if ( ioCtx->state == stateCancel && ioCtx->cancelType == cancelTimeout ) // 超时取消，处理超时响应
                    {
                        switch ( sockCtx->type )
                        {
                        case ioAccept:
                            {
                                auto * ctx = static_cast<IoAcceptCtx *>(sockCtx);
                                if ( ctx->cbTimeout )
                                {
                                    if ( ctx->cbTimeout( ctx->sock, ctx ) )
                                    {
                                        ctx->state = stateNormal; // 重新设置为正常状态
                                        memset( &ctx->ol, 0, sizeof(ctx->ol) );
                                        // 重投这个IO请求和Timer
                                        ctx->startTime = winux::GetUtcTimeMs();
                                        _PostAccept( ctx->sock->getService<IoService>(), ctx );
                                    }
                                    else
                                    {
                                        it = ioVec.erase(it); // 删除已取消或者已完成的IO事件
                                        hasEraseInIoVec = true;
                                        // 删除这个IoCtx
                                        ioCtx->decRef();

                                        this->_sockIoCount--;
                                    }
                                }
                                else
                                {
                                    ctx->state = stateNormal; // 重新设置为正常状态
                                    memset( &ctx->ol, 0, sizeof(ctx->ol) );
                                    // 重投这个IO请求和Timer
                                    ctx->startTime = winux::GetUtcTimeMs();
                                    _PostAccept( ctx->sock->getService<IoService>(), ctx );
                                }
                            }
                            break;
                        case ioConnect:
                            {
                                auto * ctx = static_cast<IoConnectCtx *>(sockCtx);
                                if ( ctx->cbTimeout )
                                {
                                    ctx->cbTimeout( ctx->sock, ctx );
                                }

                                it = ioVec.erase(it); // 删除已取消或者已完成的IO事件
                                hasEraseInIoVec = true;
                                // 删除这个IoCtx
                                ioCtx->decRef();

                                this->_sockIoCount--;
                            }
                            break;
                        case ioRecv:
                            {
                                auto * ctx = static_cast<IoRecvCtx *>(sockCtx);
                                if ( ctx->cbTimeout )
                                {
                                    ctx->cbTimeout( ctx->sock, ctx );
                                }

                                it = ioVec.erase(it); // 删除已取消或者已完成的IO事件
                                hasEraseInIoVec = true;
                                // 删除这个IoCtx
                                ioCtx->decRef();

                                this->_sockIoCount--;
                            }
                            break;
                        case ioSend:
                            {
                                auto * ctx = static_cast<IoSendCtx *>(sockCtx);
                                if ( ctx->cbTimeout )
                                {
                                    ctx->cbTimeout( ctx->sock, ctx );
                                }

                                it = ioVec.erase(it); // 删除已取消或者已完成的IO事件
                                hasEraseInIoVec = true;
                                // 删除这个IoCtx
                                ioCtx->decRef();

                                this->_sockIoCount--;
                            }
                            break;
                        case ioRecvFrom:
                            {
                                auto * ctx = static_cast<IoRecvFromCtx *>(sockCtx);
                                if ( ctx->cbTimeout )
                                {
                                    ctx->cbTimeout( ctx->sock, ctx );
                                }

                                it = ioVec.erase(it); // 删除已取消或者已完成的IO事件
                                hasEraseInIoVec = true;
                                // 删除这个IoCtx
                                ioCtx->decRef();

                                this->_sockIoCount--;
                            }
                            break;
                        case ioSendTo:
                            {
                                auto * ctx = static_cast<IoSendToCtx *>(sockCtx);
                                if ( ctx->cbTimeout )
                                {
                                    ctx->cbTimeout( ctx->sock, ctx );
                                }

                                it = ioVec.erase(it); // 删除已取消或者已完成的IO事件
                                hasEraseInIoVec = true;
                                // 删除这个IoCtx
                                ioCtx->decRef();

                                this->_sockIoCount--;
                            }
                            break;
                        }
                    }
                    else // ioCtx->state != stateCancel || ioCtx->cancelType != cancelTimeout
                    {
                        auto type = sockCtx->type;

                        switch ( type )
                        {
                        case ioAccept:
                            {
                                if ( sockCtx->state == stateError )
                                {
                                    auto * accCtx = static_cast<IoAcceptCtx *>(sockCtx);
                                    accCtx->sock->acceptAsync( accCtx->cbOk, accCtx->timeoutMs, accCtx->cbTimeout, accCtx->sock->getThread() );
                                }
                            }
                            break;
                        }

                        it = ioVec.erase(it); // 删除已取消或者已完成的IO事件
                        hasEraseInIoVec = true;
                        // 删除这个IoCtx
                        ioCtx->decRef();

                        // 统计IO类型数量
                        switch ( type )
                        {
                        case ioTimer:
                            this->_timerIoCount--;
                            break;
                        default:
                            this->_sockIoCount--;
                            break;
                        }
                    }
                }
            } // ioCtx->state != stateNormal

            // 如果已经是end则不能再++it
            if ( !hasEraseInIoVec && it != ioVec.end() ) ++it;
        } // for ( auto it = ioVec.begin(); it != ioVec.end(); hasEraseInIoVec = false )

        // 如果IO表已空，则删除该fd
        if ( ioVec.empty() )
        {
            itVecStruct = this->_ioVecMap.erase(itVecStruct);
            hasEraseInIoVecMap = true;
        }

        // 如果已经是end则不能再++it
        if ( !hasEraseInIoVecMap && itVecStruct != this->_ioVecMap.end() ) ++itVecStruct;
    }
}

void IoEventsData::wakeUpTrigger( WakeUpType type )
{
    this->_iocp.postCustom( 0, type, nullptr );
}

void IoEventsData::prePost( IoCtx * ioCtx )
{
    winux::ScopeGuard guard(this->_mtxPreIoCtxs);
    this->_preIoCtxs.push_back(ioCtx);
}

void IoEventsData::post( IoCtx * ioCtx )
{
    switch ( ioCtx->type )
    {
    case ioTimer:
        {
            auto * timerCtx = dynamic_cast<IoTimerCtx *>(ioCtx);
            auto timerFd = _GetFdByIoCtx(timerCtx);
            auto itVecStruct = this->_ioVecMap.find(timerFd);
            if ( itVecStruct != this->_ioVecMap.end() ) // 已存在此timer
            {
                auto & ioVecStruct = itVecStruct->second;
                auto & ioVec = ioVecStruct.ctxs;
                // 一个定时器只会绑定一个IoTimerCtx，因此判断是否存在
                auto it = std::find( ioVec.begin(), ioVec.end(), ioCtx );
                if ( it != ioVec.end() ) // 已存在此IoCtx
                {
                    //auto * existingTimerCtx = dynamic_cast<IoTimerCtx *>(*it);
                    //auto timer = existingTimerCtx->timer;
                    //if ( timer->stop() )
                    //{
                    //    existingTimerCtx->decRef();
                    //    // 删除已释放的`IoTimerCtx`
                    //    ioVec.erase(it);
                    //}
                }
                else
                {
                    ioVec.push_back(timerCtx);

                    this->_timerIoCount++;
                }
            }
            else // 未存在此timer
            {
                auto & ioVecStruct = this->_ioVecMap[timerFd];
                auto & ioVec = ioVecStruct.ctxs;
                ioVec.push_back(timerCtx);

                this->_timerIoCount++;
            }
        }
        break;
    default:
        {
            auto * sockIoCtx = dynamic_cast<IoSocketCtx *>(ioCtx);
            auto sockFd = _GetFdByIoCtx(sockIoCtx);
            auto itVecStruct = this->_ioVecMap.find(sockFd);
            if ( itVecStruct != this->_ioVecMap.end() ) // 已存在此socket
            {
                auto & ioVecStruct = itVecStruct->second;
                auto & ioVec = ioVecStruct.ctxs;
                auto it = std::find_if(
                    ioVec.begin(),
                    ioVec.end(),
                    [ioCtx] ( IoCtx * e ) {
                        if ( e->type != ioAccept )
                            return e->type == ioCtx->type;
                        else // IOCP可以投递多个ioAccept请求
                            return false;
                    }
                );
                if ( it != ioVec.end() ) // 已存在此类型的IoCtx
                {
                    auto * existingCtx = dynamic_cast<IoSocketCtx *>(*it);
                    if ( existingCtx->timerCtx ) // 如果有超时IO
                    {
                        auto timerFd = _GetFdByIoCtx(existingCtx->timerCtx);
                        auto timer = existingCtx->timerCtx->timer;
                        existingCtx->timerCtx->assocCtx = nullptr; // 解除关联
                        if ( timer->stop() )
                        {
                            existingCtx->timerCtx->decRef();
                            // 删除关联的超时timer
                            this->_ioVecMap.erase(timerFd);
                        }
                        existingCtx->timerCtx = nullptr; // 解除关联
                    }
                    //existingCtx->decRef(); // 释放已存在的IoCtx
                    //ioVec.erase(it);
                    existingCtx->cancel(cancelProactive);

                    ioVec.push_back(ioCtx);
                }
                else // 未存在此类型的IoCtx
                {
                    ioVec.push_back(ioCtx);

                    this->_sockIoCount++;
                }
            }
            else // 未存在此socket
            {
                auto & ioVecStruct = this->_ioVecMap[sockFd];
                auto & ioVec = ioVecStruct.ctxs;
                ioVec.push_back(ioCtx);

                this->_sockIoCount++;
            }
        }
        break;
    }
}

// class IoServiceThread ----------------------------------------------------------------------
void IoServiceThread::run()
{
    _IocpWorkerFunc( this->_serv, this, this->_ioEvents, &this->_stop );
}

void IoServiceThread::timerTrigger( io::IoTimerCtx * timerCtx )
{
    auto myTimerCtx = static_cast<IoTimerCtx *>(timerCtx);
    auto timer = myTimerCtx->timer;
    this->_ioEvents._iocp.postCustom( 0, (ULONG_PTR)timer->get(), &myTimerCtx->ol );
}


// class IoService ----------------------------------------------------------------------------
IoService::IoService( size_t threadCount ) : _stop(false)
{
    // 创建工作线程组
    this->_group.create<IoServiceThread>( threadCount, this );
    // 设置模型类型
    this->_model = modelIocp;
}

void IoService::stop()
{
    this->_stop = true;
    this->_ioEvents.wakeUpTrigger(IoEventsData::wutWantStop);
    for ( size_t i = 0; i < _group.count(); i++ )
    {
        // 给每个线程投递退出信号
        auto * th = this->getThread<IoServiceThread>(i);
        th->_stop = true;
        th->_ioEvents.wakeUpTrigger(IoEventsData::wutWantStop);
    }
}

int IoService::run()
{
    this->_group.startup();
    _IocpWorkerFunc( this, nullptr, this->_ioEvents, &this->_stop );
    this->_group.wait();

    return 0;
}

bool _PostAccept( IoService * serv, IoAcceptCtx * ctx )
{
    // 获取绑定IP
    eiennet::ip::EndPoint ep;
    ctx->sock->getBoundEp(&ep);

    // 创建现成的socket
    ctx->clientSock = ctx->sock->onCreateClient( *serv, -1, false );
    ctx->clientSock->create( ep.getAddrFamily(), eiennet::async::Socket::sockStream, eiennet::async::Socket::protoUnspec );

    DWORD dw;
    BOOL b;
    IoEventsData & ioEvents = ctx->sock->getThread() ? ctx->sock->getThread<IoServiceThread>()->_ioEvents : serv->_ioEvents;

    // 投递 IO
    b = ioEvents._iocp._self->AcceptEx(
        ctx->sock->get(),
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
            // 其他错误，主动取消IoCtx
            ctx->cancel(cancelProactive);
            return false;
        }
    }

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*serv)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, ctx->sock->getThread() );
    }

    return true;
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

    // 设置地址数据大小与输出缓冲区
    ctx->localAddrLen = ep.size() + 16;
    ctx->remoteAddrLen = ep.size() + 16;
    ctx->outputBuf.alloc( ctx->localAddrLen + ctx->remoteAddrLen );

    IoEventsData & ioEvents = ctx->sock->getThread() ? ctx->sock->getThread<IoServiceThread>()->_ioEvents : this->_ioEvents;

    if ( _PostAccept( this, ctx ) )
    {
        // 投递 IO
        ioEvents.prePost(ctx);

        // 唤醒更新
        ioEvents.wakeUpTrigger(IoEventsData::wutWantUpdate);
    }
    else
    {
        // 释放投递到iocp失败的IoCtx
        ctx->decRef();
    }
}

bool _PostConnect( IoService * serv, IoConnectCtx * ctx, eiennet::EndPoint const & ep )
{
    IoEventsData & ioEvents = ctx->sock->getThread() ? ctx->sock->getThread<IoServiceThread>()->_ioEvents : serv->_ioEvents;

    BOOL b;
    // 投递 IO
    b = ioEvents._iocp._self->ConnectEx(
        ctx->sock->get(),
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
            // 其他错误，主动取消IoCtx
            ctx->cancel(cancelProactive);
            return false;
        }
    }

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*serv)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, ctx->sock->getThread() );
    }

    return true;
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

    // 绑定一个本机端点
    sock->bind( eiennet::ip::EndPoint( ep.getAddrFamily() ) );

    IoEventsData & ioEvents = ctx->sock->getThread() ? ctx->sock->getThread<IoServiceThread>()->_ioEvents : this->_ioEvents;

    if ( _PostConnect( this, ctx, ep ) )
    {
        // 投递 IO
        ioEvents.prePost(ctx);

        // 唤醒更新
        ioEvents.wakeUpTrigger(IoEventsData::wutWantUpdate);
    }
    else
    {
        // 释放投递到iocp失败的IoCtx
        ctx->decRef();
    }
}

bool _PostRecv( IoService * serv, IoRecvCtx * ctx )
{
    DWORD dwBytes = 0;
    DWORD flag = 0;

    ctx->wsabuf.len = (ULONG)( ctx->data.capacity() - ctx->hadBytes );
    ctx->wsabuf.buf = ctx->data.getAt<CHAR>(ctx->hadBytes);

    IoEventsData & ioEvents = ctx->sock->getThread() ? ctx->sock->getThread<IoServiceThread>()->_ioEvents : serv->_ioEvents;

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
            // 其他错误，主动取消IoCtx
            ctx->cancel(cancelProactive);
            return false;
        }
    }

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*serv)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, ctx->sock->getThread() );
    }

    return true;
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

    IoEventsData & ioEvents = ctx->sock->getThread() ? ctx->sock->getThread<IoServiceThread>()->_ioEvents : this->_ioEvents;

    if ( _PostRecv( this, ctx ) )
    {
        // 投递 IO
        ioEvents.prePost(ctx);

        // 唤醒更新
        ioEvents.wakeUpTrigger(IoEventsData::wutWantUpdate);
    }
    else
    {
        // 释放投递到iocp失败的IoCtx
        ctx->decRef();
    }
}

bool _PostSend( IoService * serv, IoSendCtx * ctx )
{
    DWORD dwBytes = 0;
    DWORD flag = 0;

    ctx->wsabuf.len = (ULONG)( ctx->data.size() - ctx->hadBytes );
    ctx->wsabuf.buf = ctx->data.getAt<CHAR>(ctx->hadBytes);

    IoEventsData & ioEvents = ctx->sock->getThread() ? ctx->sock->getThread<IoServiceThread>()->_ioEvents : serv->_ioEvents;

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
            // 其他错误，主动取消IoCtx
            ctx->cancel(cancelProactive);
            return false;
        }
    }

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*serv)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, ctx->sock->getThread() );
    }

    return true;
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

    IoEventsData & ioEvents = ctx->sock->getThread() ? ctx->sock->getThread<IoServiceThread>()->_ioEvents : this->_ioEvents;

    if ( _PostSend( this, ctx ) )
    {
        // 投递 IO
        ioEvents.prePost(ctx);

        // 唤醒更新
        ioEvents.wakeUpTrigger(IoEventsData::wutWantUpdate);
    }
    else
    {
        // 释放投递到iocp失败的IoCtx
        ctx->decRef();
    }
}

bool _PostRecvFrom( IoService * serv, IoRecvFromCtx * ctx )
{
    DWORD dwBytes = 0;
    DWORD flag = 0;

    ctx->wsabuf.len = (ULONG)( ctx->data.capacity() - ctx->hadBytes );
    ctx->wsabuf.buf = ctx->data.getAt<CHAR>(ctx->hadBytes);

    IoEventsData & ioEvents = ctx->sock->getThread() ? ctx->sock->getThread<IoServiceThread>()->_ioEvents : serv->_ioEvents;

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
            // 其他错误，主动取消IoCtx
            ctx->cancel(cancelProactive);
            return false;
        }
    }

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*serv)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, ctx->sock->getThread() );
    }

    return true;
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

    IoEventsData & ioEvents = ctx->sock->getThread() ? ctx->sock->getThread<IoServiceThread>()->_ioEvents : this->_ioEvents;

    if ( _PostRecvFrom( this, ctx ) )
    {
        // 投递 IO
        ioEvents.prePost(ctx);

        // 唤醒更新
        ioEvents.wakeUpTrigger(IoEventsData::wutWantUpdate);
    }
    else
    {
        // 释放投递到iocp失败的IoCtx
        ctx->decRef();
    }
}

bool _PostSendTo( IoService * serv, IoSendToCtx * ctx )
{
    DWORD dwBytes = 0;
    DWORD flag = 0;

    ctx->wsabuf.len = (ULONG)( ctx->data.size() - ctx->hadBytes );
    ctx->wsabuf.buf = ctx->data.getAt<CHAR>(ctx->hadBytes);

    IoEventsData & ioEvents = ctx->sock->getThread() ? ctx->sock->getThread<IoServiceThread>()->_ioEvents : serv->_ioEvents;

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
            // 其他错误，主动取消IoCtx
            ctx->cancel(cancelProactive);
            return false;
        }
    }

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*serv)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, ctx->sock->getThread() );
    }

    return true;
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

    IoEventsData & ioEvents = ctx->sock->getThread() ? ctx->sock->getThread<IoServiceThread>()->_ioEvents : this->_ioEvents;

    if ( _PostSendTo( this, ctx ) )
    {
        // 投递 IO
        ioEvents.prePost(ctx);

        // 唤醒更新
        ioEvents.wakeUpTrigger(IoEventsData::wutWantUpdate);
    }
    else
    {
        // 释放投递到iocp失败的IoCtx
        ctx->decRef();
    }
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
    if ( !timer->getThread() )
    {
        timer->setThread( th != (IoServiceThread *)-1 ? th : this->getMinWeightThread() );
        if ( timer->getThread() ) timer->getThread()->incWeight(); // 增加线程负载权重
    }

    IoEventsData & ioEvents = timer->getThread() ? timer->getThread<IoServiceThread>()->_ioEvents : this->_ioEvents;
    // 投递 IO
    ioEvents.prePost(timerCtx);

    timer->set( timeoutMs, periodic );

    // 如果有关联的IoSocketCtx就不必唤醒更新，因为IoSocketCtx投递时会唤醒
    if ( !assocCtx )
    {
        ioEvents.wakeUpTrigger(IoEventsData::wutWantUpdate);
    }
}

void IoService::timerTrigger( io::IoTimerCtx * timerCtx )
{
    auto myTimerCtx = static_cast<IoTimerCtx *>(timerCtx);
    auto timer = myTimerCtx->timer;
    this->_ioEvents._iocp.postCustom( 0, (ULONG_PTR)timer->get(), &myTimerCtx->ol );
}

void IoService::removeSock( winux::SharedPointer<eiennet::async::Socket> sock )
{
    IoEventsData & ioEvents = sock->getThread() ? sock->getThread<IoServiceThread>()->_ioEvents : this->_ioEvents;
    auto & ioVecMap = ioEvents._ioVecMap;
    auto itVecStruct = ioVecMap.find( sock->get() );
    if ( itVecStruct != ioVecMap.end() )
    {
        for ( auto * ioCtx : itVecStruct->second.ctxs )
        {
            _IoSocketCtxClearTimerCtx( dynamic_cast<IoSocketCtx *>(ioCtx) );
            ioCtx->cancel(cancelRemove);
            //ioCtx->changeState(stateCancel);
        }
    }
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
            return th1->_ioEvents._iocp.associate( (HANDLE)(INT_PTR)sock->get(), sock->get() );
        }
        else // th1 == nullptr
        {
            this->_ioEvents._iocp.associate( (HANDLE)(INT_PTR)sock->get(), sock->get() );
        }
    }
    return true;
}


} // namespace iocp


} // namespace io
