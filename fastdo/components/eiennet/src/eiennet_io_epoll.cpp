#include "system_detection.inl"

#if defined(OS_WIN)

    #define FD_SETSIZE 1024
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>

    #define socket_errno (WSAGetLastError())
    // in6_addr
    #define s6_addr16 s6_words
#else

    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <sys/epoll.h>
    #include <sys/eventfd.h>
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
#include "eiennet_io_epoll.hpp"

namespace io
{
namespace epoll
{
// IoSocketCtx 超时处理 ---------------------------------------------------------------------------
static void _IoSocketCtxTimeoutCallback( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx, IoEventsData & ioEvents )
{
    auto * assocCtx = timerCtx->assocCtx;
    if ( assocCtx )
    {
        assocCtx->timerCtx = nullptr; // 取消关联的超时timer场景
        assocCtx->changeState(stateTimeoutCancel); // 超时取消操作
        // 触发超时回调
        switch ( assocCtx->type )
        {
        case ioAccept:
            {
                auto * ctx = static_cast<IoAcceptCtx *>(assocCtx);
                if ( ctx->cbTimeout )
                {
                    if ( ctx->cbTimeout( ctx->sock, ctx ) )
                    {
                        // 重投这个IO请求和Timer
                        ctx->startTime = winux::GetUtcTimeMs();
                        if ( ctx->timeoutMs != -1 )
                        {
                            eiennet::async::Timer::New( *ctx->sock->getService() )->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
                            }, ctx, ctx->sock->getThread() );
                        }
                    }
                    else
                    {
                        ioEvents.delIoCtx(ctx); // 删除这个IO场景
                    }
                }
                else
                {
                    // 重投这个IO请求和Timer
                    ctx->startTime = winux::GetUtcTimeMs();
                    if ( ctx->timeoutMs != -1 )
                    {
                        eiennet::async::Timer::New( *ctx->sock->getService() )->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
                        }, ctx, ctx->sock->getThread() );
                    }
                }
            }
            break;
        case ioConnect:
            {
                auto * ctx = static_cast<IoConnectCtx *>(assocCtx);
                if ( ctx->cbTimeout )
                {
                    ctx->cbTimeout( ctx->sock, ctx );
                }
                ioEvents.delIoCtx(ctx); // 删除这个IO场景
            }
            break;
        case ioRecv:
            {
                auto * ctx = static_cast<IoRecvCtx *>(assocCtx);
                if ( ctx->cbTimeout )
                {
                    ctx->cbTimeout( ctx->sock, ctx );
                }
                ioEvents.delIoCtx(ctx); // 删除这个IO场景
            }
            break;
        case ioSend:
            {
                auto * ctx = static_cast<IoSendCtx *>(assocCtx);
                if ( ctx->cbTimeout )
                {
                    ctx->cbTimeout( ctx->sock, ctx );
                }
                ioEvents.delIoCtx(ctx); // 删除这个IO场景
            }
            break;
        case ioRecvFrom:
            {
                auto * ctx = static_cast<IoRecvFromCtx *>(assocCtx);
                if ( ctx->cbTimeout )
                {
                    ctx->cbTimeout( ctx->sock, ctx );
                }
                ioEvents.delIoCtx(ctx); // 删除这个IO场景
            }
            break;
        case ioSendTo:
            {
                auto * ctx = static_cast<IoSendToCtx *>(assocCtx);
                if ( ctx->cbTimeout )
                {
                    ctx->cbTimeout( ctx->sock, ctx );
                }
                ioEvents.delIoCtx(ctx); // 删除这个IO场景
            }
            break;
        }
    }
}

// IoSocketCtx 清空超时事件关联，并停止超时定时器，尝试释放IoTimerCtx --------------------------------
static void _IoSocketCtxClearTimerCtx( io::IoSocketCtx * ctx )
{
    if ( ctx->timerCtx )
    {
        auto timer = ctx->timerCtx->timer;
        ctx->timerCtx->assocCtx = nullptr;
        IoEventsData & ioEvents = ctx->sock->getThread() ?
            static_cast<IoServiceThread *>( ctx->sock->getThread() )->getIoEvents() :
            static_cast<IoService *>( ctx->sock->getService() )->getIoEvents();
        if ( timer->stop() )
        {
            winux::ScopeUnguard unguard( ioEvents.getMutex() );
            ioEvents.delIoCtx(ctx->timerCtx); // 删除这个TimerIO场景
            //ctx->timerCtx->decRef();
        }
        ctx->timerCtx = nullptr;
    }
}

// EPOLL工作函数 ----------------------------------------------------------------------------------
void _EpollWorkerFunc( IoService * serv, IoServiceThread * thread, IoEventsData & ioEvents, bool * stop )
{
    while ( !*stop )
    {
        int rc = ioEvents.getEpoll().wait();
        if ( rc < 0 )
        {
            if ( errno == EINTR ) continue;
            //*stop = false;
        }
        else
        {
            // stop signal eventfd
            auto & stopEventFd = thread ? thread->_stopEventFd : serv->_stopEventFd;

            for ( int i = 0; i < rc; i++ )
            {
                auto & evt = ioEvents.getEpoll().evt(i);
                if ( evt.data.fd == stopEventFd.get() )
                {
                    // stop signal事件，清理 eventfd
                    uint64_t value;
                    read( stopEventFd.get(), &value, sizeof(value) );
                }
                else
                {
                    // 处理其他IO事件
                    winux::ScopeGuard guard(ioEvents._mtx);
                    auto & ioMap = ioEvents._fdToIoCtxs[evt.data.fd];
                    if ( evt.events & EPOLLIN )
                    {
                        for ( auto it = ioMap.begin(); it != ioMap.end(); it++ )
                        {
                            switch ( it->first )
                            {
                            case ioAccept:
                                {
                                    auto * accCtx = dynamic_cast<IoAcceptCtx *>(it->second);
                                    _IoSocketCtxClearTimerCtx(accCtx);

                                    // 接受客户连接
                                    auto clientSock = accCtx->sock->accept(&accCtx->clientEp);
                                    // 处理回调
                                    if ( accCtx->cbOk )
                                    {
                                        winux::ScopeUnguard unguard(ioEvents._mtx);
                                        if ( accCtx->cbOk( accCtx->sock, clientSock, accCtx->clientEp ) )
                                        {
                                            // 重投这个IO请求和Timer
                                            accCtx->startTime = winux::GetUtcTimeMs();
                                            if ( accCtx->timeoutMs != -1 )
                                            {
                                                eiennet::async::Timer::New( *accCtx->sock->getService() )->waitAsyncEx( accCtx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                                    _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
                                                }, accCtx, accCtx->sock->getThread() );
                                            }
                                        }
                                        else
                                        {
                                            // 已处理，删除这个请求
                                            accCtx->changeState(stateFinish);
                                            ioEvents.delIoCtx(accCtx);
                                        }
                                    }
                                    else
                                    {
                                        // 重投这个IO请求和Timer
                                        accCtx->startTime = winux::GetUtcTimeMs();
                                        if ( accCtx->timeoutMs != -1 )
                                        {
                                            eiennet::async::Timer::New( *accCtx->sock->getService() )->waitAsyncEx( accCtx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                                _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
                                            }, accCtx, accCtx->sock->getThread() );
                                        }
                                    }

                                    goto EXIT_EPOLLIN_IOMAP_LOOP;
                                }
                                break;
                            case ioRecv:
                                {
                                    auto * recvCtx = dynamic_cast<IoRecvCtx *>(it->second);
                                    _IoSocketCtxClearTimerCtx(recvCtx);

                                    size_t wantBytes = 0;
                                    if ( recvCtx->targetBytes > 0 )
                                    {
                                        wantBytes = recvCtx->targetBytes - recvCtx->hadBytes;
                                    }
                                    else
                                    {
                                        wantBytes = recvCtx->sock->getAvailable();
                                    }

                                    winux::Buffer data = recvCtx->sock->recv(wantBytes);
                                    recvCtx->cnnAvail = data && data.size();
                                    if ( recvCtx->cnnAvail )
                                    {
                                        recvCtx->data.append(data);
                                        recvCtx->hadBytes += data.size();
                                    }

                                    if ( recvCtx->hadBytes >= recvCtx->targetBytes || data.size() == 0 )
                                    {
                                        // 处理回调
                                        if ( recvCtx->cbOk )
                                        {
                                            winux::ScopeUnguard unguard(ioEvents._mtx);
                                            recvCtx->cbOk( recvCtx->sock, recvCtx->data, recvCtx->cnnAvail );
                                        }

                                        // 已处理，删除这个请求
                                        {
                                            winux::ScopeUnguard unguard(ioEvents._mtx);
                                            recvCtx->changeState(stateFinish);
                                            ioEvents.delIoCtx(recvCtx);
                                        }
                                    }
                                    else
                                    {
                                        // 重投这个IO请求和Timer
                                        recvCtx->startTime = winux::GetUtcTimeMs();
                                        if ( recvCtx->timeoutMs != -1 )
                                        {
                                            eiennet::async::Timer::New( *recvCtx->sock->getService() )->waitAsyncEx( recvCtx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                                _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
                                            }, recvCtx, recvCtx->sock->getThread() );
                                        }
                                    }

                                    goto EXIT_EPOLLIN_IOMAP_LOOP;
                                }
                                break;
                            case ioRecvFrom:
                                {
                                    auto * recvFromCtx = dynamic_cast<IoRecvFromCtx *>(it->second);
                                    _IoSocketCtxClearTimerCtx(recvFromCtx);

                                    size_t wantBytes = 0;
                                    if ( recvFromCtx->targetBytes > 0 )
                                    {
                                        wantBytes = recvFromCtx->targetBytes - recvFromCtx->hadBytes;
                                    }
                                    else
                                    {
                                        wantBytes = recvFromCtx->sock->getAvailable();
                                    }

                                    winux::Buffer data = recvFromCtx->sock->recvFrom( &recvFromCtx->epFrom, wantBytes );
                                    if ( data ) recvFromCtx->data.append(data);
                                    recvFromCtx->hadBytes += data.size();

                                    if ( recvFromCtx->hadBytes >= recvFromCtx->targetBytes || data.size() == 0 )
                                    {
                                        // 处理回调
                                        if ( recvFromCtx->cbOk )
                                        {
                                            winux::ScopeUnguard unguard(ioEvents._mtx);
                                            recvFromCtx->cbOk( recvFromCtx->sock, recvFromCtx->data, recvFromCtx->epFrom );
                                        }

                                        // 已处理，删除这个请求
                                        {
                                            winux::ScopeUnguard unguard(ioEvents._mtx);
                                            recvFromCtx->changeState(stateFinish);
                                            ioEvents.delIoCtx(recvFromCtx);
                                        }
                                    }
                                    else
                                    {
                                        // 重投这个IO请求和Timer
                                        recvFromCtx->startTime = winux::GetUtcTimeMs();
                                        if ( recvFromCtx->timeoutMs != -1 )
                                        {
                                            eiennet::async::Timer::New( *recvFromCtx->sock->getService() )->waitAsyncEx( recvFromCtx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                                _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
                                            }, recvFromCtx, recvFromCtx->sock->getThread() );
                                        }
                                    }

                                    goto EXIT_EPOLLIN_IOMAP_LOOP;
                                }
                                break;
                            case ioTimer:
                                {
                                    auto * timerCtx = dynamic_cast<IoTimerCtx *>(it->second);
                                    auto timer = timerCtx->timer;
                                    // 读取timer
                                    uint64_t cnt = 0;
                                    read( timer->get(), &cnt, sizeof(uint64_t) );

                                    if ( timerCtx->cbOk )
                                    {
                                        winux::ScopeUnguard unguard(ioEvents._mtx);
                                        timerCtx->cbOk( timer, timerCtx );
                                    }

                                    {
                                        winux::ScopeGuard guard( timer->getMutex() );
                                        if ( timerCtx->periodic == false ) // 非周期
                                        {
                                            {
                                                winux::ScopeUnguard unguard( timer->getMutex() );
                                                // 已处理，完成这个请求
                                                timerCtx->changeState(stateFinish);
                                            }
                                            timer->_timerCtx = nullptr;

                                            // 已处理，删除IoTimerCtx
                                            {
                                                winux::ScopeUnguard unguard(ioEvents._mtx);
                                                ioEvents.delIoCtx(timerCtx);
                                            }
                                        }
                                        else
                                        {
                                            timer->_posted = false;
                                        }
                                    }

                                    goto EXIT_EPOLLIN_IOMAP_LOOP;
                                }
                                break;
                            }
                        }
                    EXIT_EPOLLIN_IOMAP_LOOP:
                        ;
                    }
                    else if ( evt.events & EPOLLOUT )
                    {
                        for ( auto it = ioMap.begin(); it != ioMap.end(); it++ )
                        {
                            switch ( it->first )
                            {
                            case ioConnect:
                                {
                                    auto * cnnCtx = dynamic_cast<IoConnectCtx *>(it->second);
                                    _IoSocketCtxClearTimerCtx(cnnCtx);

                                    cnnCtx->costTimeMs = winux::GetUtcTimeMs() - cnnCtx->startTime;
                                    // 处理回调
                                    if ( cnnCtx->cbOk )
                                    {
                                        winux::ScopeUnguard unguard(ioEvents._mtx);
                                        cnnCtx->cbOk( cnnCtx->sock, cnnCtx->costTimeMs );
                                    }

                                    // 已处理，删除这个请求
                                    {
                                        winux::ScopeUnguard unguard(ioEvents._mtx);
                                        cnnCtx->changeState(stateFinish);
                                        ioEvents.delIoCtx(cnnCtx);
                                    }

                                    goto EXIT_EPOLLOUT_IOMAP_LOOP;
                                }
                                break;
                            case ioSend:
                                {
                                    auto * sendCtx = dynamic_cast<IoSendCtx *>(it->second);
                                    _IoSocketCtxClearTimerCtx(sendCtx);

                                    sendCtx->cnnAvail = true;
                                    sendCtx->costTimeMs += winux::GetUtcTimeMs() - sendCtx->startTime;

                                    if ( sendCtx->hadBytes < sendCtx->data.size() )
                                    {
                                        size_t wantBytes = sendCtx->data.size() - sendCtx->hadBytes;
                                        int sendBytes = sendCtx->sock->send( sendCtx->data.getAt<winux::byte>(sendCtx->hadBytes), wantBytes );
                                        if ( sendBytes > 0 )
                                        {
                                            sendCtx->hadBytes += sendBytes;
                                        }
                                        else // sendBytes <= 0
                                        {
                                            sendCtx->cnnAvail = false;
                                        }
                                    }

                                    if ( sendCtx->hadBytes >= sendCtx->data.size() || !sendCtx->cnnAvail )
                                    {
                                        // 处理回调
                                        if ( sendCtx->cbOk )
                                        {
                                            winux::ScopeUnguard unguard(ioEvents._mtx);
                                            sendCtx->cbOk( sendCtx->sock, sendCtx->hadBytes, sendCtx->costTimeMs, sendCtx->cnnAvail );
                                        }

                                        // 已处理，删除这个请求
                                        {
                                            winux::ScopeUnguard unguard(ioEvents._mtx);
                                            sendCtx->changeState(stateFinish);
                                            ioEvents.delIoCtx(sendCtx);
                                        }
                                    }
                                    else
                                    {
                                        // 重投这个IO请求和Timer
                                        sendCtx->startTime = winux::GetUtcTimeMs();
                                        if ( sendCtx->timeoutMs != -1 )
                                        {
                                            eiennet::async::Timer::New( *sendCtx->sock->getService() )->waitAsyncEx( sendCtx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                                _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
                                            }, sendCtx, sendCtx->sock->getThread() );
                                        }
                                    }

                                    goto EXIT_EPOLLOUT_IOMAP_LOOP;
                                }
                                break;
                            case ioSendTo:
                                {
                                    auto * sendToCtx = dynamic_cast<IoSendToCtx *>(it->second);
                                    _IoSocketCtxClearTimerCtx(sendToCtx);

                                    bool fail = false;
                                    sendToCtx->costTimeMs += winux::GetUtcTimeMs() - sendToCtx->startTime;

                                    if ( sendToCtx->hadBytes < sendToCtx->data.size() )
                                    {
                                        size_t wantBytes = sendToCtx->data.size() - sendToCtx->hadBytes;
                                        int sendBytes = sendToCtx->sock->sendTo( *sendToCtx->epTo.get(), sendToCtx->data.getAt<winux::byte>(sendToCtx->hadBytes), wantBytes );
                                        if ( sendBytes > 0 )
                                        {
                                            sendToCtx->hadBytes += sendBytes;
                                        }
                                        else // sendBytes <= 0
                                        {
                                            fail = true;
                                        }
                                    }

                                    if ( sendToCtx->hadBytes >= sendToCtx->data.size() || fail )
                                    {
                                        // 处理回调
                                        if ( sendToCtx->cbOk )
                                        {
                                            winux::ScopeUnguard unguard(ioEvents._mtx);
                                            sendToCtx->cbOk( sendToCtx->sock, sendToCtx->hadBytes, sendToCtx->costTimeMs );
                                        }

                                        // 已处理，删除这个请求
                                        {
                                            winux::ScopeUnguard unguard(ioEvents._mtx);
                                            sendToCtx->changeState(stateFinish);
                                            ioEvents.delIoCtx(sendToCtx);
                                        }
                                    }
                                    else
                                    {
                                        // 重投这个IO请求和Timer
                                        sendToCtx->startTime = winux::GetUtcTimeMs();
                                        if ( sendToCtx->timeoutMs != -1 )
                                        {
                                            eiennet::async::Timer::New( *sendToCtx->sock->getService() )->waitAsyncEx( sendToCtx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                                _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
                                            }, sendToCtx, sendToCtx->sock->getThread() );
                                        }
                                    }

                                    goto EXIT_EPOLLOUT_IOMAP_LOOP;
                                }
                                break;
                            }
                        }
                    EXIT_EPOLLOUT_IOMAP_LOOP:
                        ;
                    }
                    else if ( evt.events & EPOLLERR ) // 错误事件，清理相关IoCtx
                    {
                        if ( !ioMap.empty() )
                        {
                            auto & pr = *ioMap.begin();
                            if ( pr.first != ioTimer ) // 非定时器的错误事件处理
                            {
                                auto * sockCtx = dynamic_cast<IoSocketCtx *>(pr.second);
                                auto sock = sockCtx->sock;
                                {
                                    winux::ScopeUnguard unguard(ioEvents._mtx);
                                    sock->onError(sock);
                                }
                            }
                        }

                        // 删除此fd的所有IoCtxs
                        {
                            winux::ScopeUnguard unguard(ioEvents._mtx);
                            ioEvents.delIoCtxs(evt.data.fd);
                        }
                    }
                }
            }
        }
    }
}

// class IoEventsData -------------------------------------------------------------------------
IoEventsData::IoEventsData( Epoll * epoll ) : _mtx(true), _epoll(epoll), _sockIoCount(0), _timerIoCount(0)
{

}

void IoEventsData::setIoCtx( IoCtx * ioCtx )
{
    winux::ScopeGuard guard(this->_mtx);
    int fd = ioCtx->type == ioTimer ?
        dynamic_cast<IoTimerCtx *>(ioCtx)->timer->get() :
        dynamic_cast<IoSocketCtx *>(ioCtx)->sock->get();

    this->_fdToIoCtxs[fd][ioCtx->type] = ioCtx;

    if ( winux::IsSet( this->_fdToEvents, fd ) )
    {
        auto & events = this->_fdToEvents[fd];
        switch ( ioCtx->type )
        {
        case ioAccept:
        case ioRecv:
        case ioRecvFrom:
        case ioTimer:
            events |= EPOLLIN;
            break;
        case ioConnect:
        case ioSend:
        case ioSendTo:
            events |= EPOLLOUT;
            break;
        }

        int rc = this->_epoll->mod( fd, events );
        if ( rc < 0 )
        {
            // 修改失败，如果是因为之前没有添加过这个fd，尝试添加
            if ( errno == ENOENT )
            {
                rc = this->_epoll->add( fd, events );
            }
        }
    }
    else
    {
        auto & events = this->_fdToEvents[fd];
        switch ( ioCtx->type )
        {
        case ioAccept:
        case ioRecv:
        case ioRecvFrom:
        case ioTimer:
            events = EPOLLIN;
            break;
        case ioConnect:
        case ioSend:
        case ioSendTo:
            events = EPOLLOUT;
            break;
        }
        events |= EPOLLERR; // 错误事件
        this->_epoll->add( fd, events );
    }

    // 统计IO类型数量
    switch ( ioCtx->type )
    {
    case ioTimer:
        this->_timerIoCount++;
        break;
    default:
        this->_sockIoCount++;
        break;
    }
}

IoCtx * IoEventsData::getIoCtx( int fd, IoType type, uint32_t * events )
{
    winux::ScopeGuard guard(this->_mtx);
    if ( winux::IsSet( this->_fdToIoCtxs, fd ) )
    {
        auto & ioMap = this->_fdToIoCtxs[fd];
        if ( winux::IsSet( ioMap, type ) )
        {
            return ioMap[type];
        }
    }
    if ( events )
    {
        *events = winux::IsSet( this->_fdToEvents, fd ) ? this->_fdToEvents[fd] : 0;
    }
    return nullptr;
}

void IoEventsData::delIoCtx( IoCtx * ioCtx )
{
    winux::ScopeGuard guard(this->_mtx);
    int fd = ioCtx->type == ioTimer ?
        dynamic_cast<IoTimerCtx *>(ioCtx)->timer->get() :
        dynamic_cast<IoSocketCtx *>(ioCtx)->sock->get();

    auto & ioMap = this->_fdToIoCtxs[fd];
    ioMap.erase(ioCtx->type);
    if ( ioMap.empty() )
    {
        // 从fdToIoCtxs中移除对应fd所有IO
        this->_fdToIoCtxs.erase(fd);
        // 从fdToEvents中移除这个fd的事件掩码
        this->_fdToEvents.erase(fd);
        // 从epoll中删除这个fd
        this->_epoll->del(fd);
    }
    else
    {
        auto & events = this->_fdToEvents[fd];
        // 清除ioCtx类型对应的epoll事件
        switch ( ioCtx->type )
        {
        case ioAccept:
        case ioRecv:
        case ioRecvFrom:
        case ioTimer:
            events &= ~EPOLLIN;
            break;
        case ioConnect:
        case ioSend:
        case ioSendTo:
            events &= ~EPOLLOUT;
            break;
        }

        // 更新epoll监听事件
        this->_epoll->mod( fd, events );
    }

    switch ( ioCtx->type )
    {
    case ioTimer:
        this->_timerIoCount--;
        break;
    default:
        this->_sockIoCount--;
        break;
    }

    // 释放ioCtx
    ioCtx->decRef();
}

bool IoEventsData::hasIoCtxs( int fd ) const
{
    winux::ScopeGuard guard( const_cast<winux::Mutex &>(this->_mtx) );
    return winux::IsSet( this->_fdToIoCtxs, fd );
}

IoEventsData::IoMap & IoEventsData::getIoCtxs( int fd )
{
    winux::ScopeGuard guard(this->_mtx);
    return this->_fdToIoCtxs[fd];
}

void IoEventsData::delIoCtxs( int fd )
{
    winux::ScopeGuard guard(this->_mtx);
    // 应该先从epoll中DEL相应的fd
    this->_epoll->del(fd);

    if ( winux::IsSet( this->_fdToIoCtxs, fd ) )
    {
        std::vector<int> wantRemoveTimerFds;
        auto & ioMap = this->_fdToIoCtxs[fd];
        for ( auto & pr : ioMap )
        {
            auto * ioCtx = pr.second;
            if ( ioCtx->type == ioTimer ) // timer io
            {
                auto * timerCtx = dynamic_cast<IoTimerCtx *>(ioCtx);
                auto timer = timerCtx->timer;
                if ( timer->stop() )
                {
                    timerCtx->decRef();
                }
                this->_timerIoCount--;
            }
            else // socket io
            {
                auto * sockIoCtx = dynamic_cast<IoSocketCtx *>(ioCtx);
                if ( sockIoCtx->timerCtx )
                {
                    auto timer = sockIoCtx->timerCtx->timer;
                    // 先从epoll中DEL相应的timerfd
                    int timerfd = timer->get();
                    this->_epoll->del(timerfd);
                    if ( timer->stop() )
                    {
                        sockIoCtx->timerCtx->decRef();
                    }
                    this->_timerIoCount--;
                    // 再从fdToIoCtxs中移除对应的timerfd
                    wantRemoveTimerFds.push_back(timerfd);
                }
                sockIoCtx->changeState(stateProactiveCancel);
                sockIoCtx->decRef();
                this->_sockIoCount--;
            }
        }
        // 从FdIoCtxsMap中移除对应fd所有IO
        this->_fdToIoCtxs.erase(fd);
        this->_fdToEvents.erase(fd);
        // 删除sock io的超时timer io
        for ( auto fd1 : wantRemoveTimerFds )
        {
            this->_fdToIoCtxs.erase(fd1);
            this->_fdToEvents.erase(fd1);
        }
    }
}

// class Epoll --------------------------------------------------------------------------------
Epoll::Epoll( size_t maxEvents, bool mts ) : _epollFd(-1), _maxEvents(maxEvents), _mtx(mts), _mts(mts)
{
    _epollFd = epoll_create1(0);
    _evts.resize(_maxEvents);
}

Epoll::~Epoll()
{
    if ( _epollFd != -1 ) close(_epollFd);
}

int Epoll::add( int fd, uint32_t events, void * data )
{
    struct epoll_event ev;
    ev.events = events;

    if ( !data )
        ev.data.fd = fd;
    else
        ev.data.ptr = data;

    if ( _mts )
    {
        winux::ScopeGuard guard(_mtx);
        return epoll_ctl( _epollFd, EPOLL_CTL_ADD, fd, &ev );
    }
    else
    {
        return epoll_ctl( _epollFd, EPOLL_CTL_ADD, fd, &ev );
    }
}

int Epoll::mod( int fd, uint32_t events, void * data )
{
    struct epoll_event ev;
    ev.events = events;

    if ( !data )
        ev.data.fd = fd;
    else
        ev.data.ptr = data;

    if ( _mts )
    {
        winux::ScopeGuard guard(_mtx);
        return epoll_ctl( _epollFd, EPOLL_CTL_MOD, fd, &ev );
    }
    else
    {
        return epoll_ctl( _epollFd, EPOLL_CTL_MOD, fd, &ev );
    }
}

int Epoll::del( int fd )
{
    if ( _mts )
    {
        winux::ScopeGuard guard(_mtx);
        return epoll_ctl( _epollFd, EPOLL_CTL_DEL, fd, nullptr );
    }
    else
    {
        return epoll_ctl( _epollFd, EPOLL_CTL_DEL, fd, nullptr );
    }
}

int Epoll::wait( int timeout )
{
    return epoll_wait( _epollFd, _evts.data(), _maxEvents, timeout );
}

size_t Epoll::evtsCount() const
{
    return _evts.size();
}

struct epoll_event * Epoll::evts()
{
    return _evts.data();
}

struct epoll_event & Epoll::evt( int i )
{
    return _evts[i];
}


// class IoServiceThread ----------------------------------------------------------------------
IoServiceThread::IoServiceThread( IoService * serv ) : _epoll( 64, true ), _ioEvents(&_epoll), _serv(serv), _stop(false)
{
    // 创建eventfd
    this->_stopEventFd.attachNew( eventfd( 0, 0 ), -1, close );
    // 监听stop event
    this->_epoll.add( this->_stopEventFd.get(), EPOLLIN );
}

void IoServiceThread::run()
{
    _EpollWorkerFunc( this->_serv, this, this->_ioEvents, &this->_stop );
}

void IoServiceThread::timerTrigger( io::IoTimerCtx * timerCtx )
{
}


// class IoService ----------------------------------------------------------------------------
IoService::IoService( size_t groupThread ) : _epoll( 64, true ), _ioEvents(&_epoll), _stop(false)
{
    // 创建工作线程组
    this->_group.create<IoServiceThread>( groupThread, this );
    // 创建eventfd
    this->_stopEventFd.attachNew( eventfd( 0, 0 ), -1, close );
    // 监听stop event
    this->_epoll.add( this->_stopEventFd.get(), EPOLLIN );
}

void IoService::stop()
{
    winux::uint64 u = 1;
    this->_stop = true;
    // 投递停止信号
    write( _stopEventFd.get(), &u, sizeof(winux::uint64) );
    for ( size_t i = 0; i < _group.count(); i++ )
    {
        // 给每个线程投递退出信号
        auto * th = static_cast<IoServiceThread *>( this->getGroupThread(i) );
        th->_stop = true;
        // 投递停止信号
        write( th->_stopEventFd.get(), &u, sizeof(winux::uint64) );
    }
}

int IoService::run()
{
    this->_group.startup();
    _EpollWorkerFunc( this, nullptr, this->_ioEvents, &this->_stop );
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

    IoEventsData & ioEvents = sock->getThread() ? static_cast<IoServiceThread *>( sock->getThread() )->_ioEvents : this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*this)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents.setIoCtx(ctx);
}

void IoService::postConnect( winux::SharedPointer<eiennet::async::Socket> sock, eiennet::EndPoint const & ep, IoConnectCtx::OkFn cbOk, winux::uint64 timeoutMs, IoConnectCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    if ( !this->associate( sock, th ) ) return;

    sock->connect(ep);

    auto * ctx = IoConnectCtx::New();
    ctx->timeoutMs = timeoutMs;
    ctx->sock = sock;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;

    IoEventsData & ioEvents = sock->getThread() ? static_cast<IoServiceThread *>( sock->getThread() )->_ioEvents : this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*this)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents.setIoCtx(ctx);
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

    IoEventsData & ioEvents = sock->getThread() ? static_cast<IoServiceThread *>( sock->getThread() )->_ioEvents : this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*this)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents.setIoCtx(ctx);
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

    IoEventsData & ioEvents = sock->getThread() ? static_cast<IoServiceThread *>( sock->getThread() )->_ioEvents : this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*this)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents.setIoCtx(ctx);
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

    IoEventsData & ioEvents = sock->getThread() ? static_cast<IoServiceThread *>( sock->getThread() )->_ioEvents : this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*this)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents.setIoCtx(ctx);
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

    IoEventsData & ioEvents = sock->getThread() ? static_cast<IoServiceThread *>( sock->getThread() )->_ioEvents : this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*this)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents.setIoCtx(ctx);
}

void IoService::postTimer( winux::SharedPointer<eiennet::async::Timer> timer, winux::uint64 timeoutMs, bool periodic, IoTimerCtx::OkFn cbOk, io::IoSocketCtx * assocCtx, io::IoServiceThread * th )
{
    IoTimerCtx * timerCtx = nullptr;
    {
        winux::ScopeGuard guard( timer->getMutex() );
        timer->_posted = false;
        if ( timer->_timerCtx )
        {
            timerCtx = static_cast<IoTimerCtx *>(timer->_timerCtx);
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
    if ( timer->getThread() ) timer->getThread()->incWeight(); // 增加线程负载权重

    IoEventsData & ioEvents = timer->getThread() ? ((IoServiceThread *)timer->getThread())->_ioEvents : this->_ioEvents;
    // 投递 IO
    ioEvents.setIoCtx(timerCtx);

    timer->set( timeoutMs, periodic );
}

void IoService::timerTrigger( io::IoTimerCtx * timerCtx )
{
}

void IoService::removeSock( winux::SharedPointer<eiennet::async::Socket> sock )
{
    IoEventsData & ioEvents = sock->getThread() ? static_cast<IoServiceThread *>( sock->getThread() )->_ioEvents : this->_ioEvents;
    ioEvents.delIoCtxs( sock->get() );
}

bool IoService::associate( winux::SharedPointer<eiennet::async::Socket> sock, io::IoServiceThread * th )
{
    if ( sock->getThread() == nullptr )
    {
        sock->setThread( th != (IoServiceThread *)-1 ? th : this->getMinWeightThread() );
        if ( sock->getThread() != nullptr )
        {
            sock->getThread()->incWeight(); // 增加线程负载权重
            return true;
        }
        else // sock->getThread() == nullptr
        {
        }
    }
    return true;
}


} // namespace epoll


} // namespace io
