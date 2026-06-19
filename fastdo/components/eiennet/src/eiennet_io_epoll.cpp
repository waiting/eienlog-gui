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
    }
}

// IoSocketCtx 清空超时事件关联，并停止超时定时器，尝试释放IoTimerCtx --------------------------------
static void _IoSocketCtxClearTimerCtx( io::IoSocketCtx * ctx )
{
    if ( ctx->timerCtx )
    {
        auto timer = ctx->timerCtx->timer;
        ctx->timerCtx->assocCtx = nullptr;
        // IoEventsData & ioEvents = ctx->sock->getThread() ?
        //     ctx->sock->getThread<IoServiceThread>()->getIoEvents() :
        //     ctx->sock->getService<IoService>()->getIoEvents();
        // if ( timer->stop() )
        // {
        //     winux::ScopeUnguard unguard( ioEvents.getMutex() );
        //     ioEvents.delIoCtx(ctx->timerCtx); // 删除这个TimerIO场景
        //     //ctx->timerCtx->decRef();
        // }
        timer->stop();
        ctx->timerCtx = nullptr;
    }
}

// EPOLL工作函数 ----------------------------------------------------------------------------------
void _EpollWorkerFunc( IoService * serv, IoServiceThread * thread, IoEventsData & ioEvents, bool * stop )
{
    while ( !*stop )
    {
        ioEvents._handleIoEventsPost();

        int rc = ioEvents.getEpoll().wait();
        if ( rc < 0 )
        {
            if ( errno == EINTR ) continue;
            //*stop = false;
        }
        else
        {
            ioEvents._handleIoEventsCallback(rc);
        }
        ioEvents._handleIoEventsTimeoutAndDelete();
    }
}

// class IoEventsData -------------------------------------------------------------------------
IoEventsData::IoEventsData( Epoll * epoll ) : _mtx(true), _epoll(epoll), _sockIoCount(0), _timerIoCount(0)
{
    // 创建eventfd
    this->_wakeUpEventFd.attachNew( eventfd( 0, 0 ), -1, close );
    // 监听stop event
    this->_epoll->add( this->_wakeUpEventFd.get(), EPOLLIN );
}

void IoEventsData::_handleIoEventsPost()
{
    winux::ScopeGuard guard(this->_mtxPreIoCtxs);
    for ( auto it = this->_preIoCtxs.begin(); it != this->_preIoCtxs.end(); it++ )
    {
        (*it)->state = stateNormal; // 设置为正常状态
        this->setIoCtx(*it);
    }
    this->_preIoCtxs.clear();
}

void IoEventsData::_handleIoEventsCallback( int rc )
{
    for ( int i = 0; i < rc; i++ )
    {
        auto & evt = this->_epoll->evt(i);
        if ( evt.data.fd == this->_wakeUpEventFd.get() ) // wake up事件
        {
            uint64_t value;
            read( this->_wakeUpEventFd.get(), &value, sizeof(value) );
        }
        else // 处理其他IO事件
        {
            winux::ScopeGuard guard(this->_mtx);
            auto & ioVecStruct = this->_ioVecMap[evt.data.fd];
            if ( evt.events & EPOLLIN )
            {
                for ( auto it = ioVecStruct.ctxs.begin(); it != ioVecStruct.ctxs.end(); it++ )
                {
                    switch ( (*it)->type )
                    {
                    case ioAccept:
                        {
                            auto * accCtx = dynamic_cast<IoAcceptCtx *>(*it);
                            _IoSocketCtxClearTimerCtx(accCtx);

                            // 接受客户连接
                            auto clientSock = accCtx->sock->accept(&accCtx->clientEp);
                            // 处理回调
                            if ( accCtx->cbOk )
                            {
                                winux::ScopeUnguard unguard(this->_mtx);
                                if ( accCtx->cbOk( accCtx->sock, clientSock, accCtx->clientEp ) )
                                {
                                    // 重投这个IO请求和Timer
                                    accCtx->startTime = winux::GetUtcTimeMs();
                                    if ( accCtx->timeoutMs != -1 )
                                    {
                                        eiennet::async::Timer::New( *accCtx->sock->getService() )->waitAsyncEx( accCtx->timeoutMs, false, [this] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                            _IoSocketCtxTimeoutCallback( timer, timerCtx, *this );
                                        }, accCtx, accCtx->sock->getThread() );
                                    }
                                }
                                else
                                {
                                    // 已处理，完成这个请求
                                    accCtx->changeState(stateFinish);
                                }
                            }
                            else
                            {
                                // 重投这个IO请求和Timer
                                accCtx->startTime = winux::GetUtcTimeMs();
                                if ( accCtx->timeoutMs != -1 )
                                {
                                    eiennet::async::Timer::New( *accCtx->sock->getService() )->waitAsyncEx( accCtx->timeoutMs, false, [this] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                        _IoSocketCtxTimeoutCallback( timer, timerCtx, *this );
                                    }, accCtx, accCtx->sock->getThread() );
                                }
                            }

                            goto EXIT_EPOLLIN_IOMAP_LOOP;
                        }
                        break;
                    case ioRecv:
                        {
                            auto * recvCtx = dynamic_cast<IoRecvCtx *>(*it);
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
                                    winux::ScopeUnguard unguard(this->_mtx);
                                    recvCtx->cbOk( recvCtx->sock, recvCtx->data, recvCtx->cnnAvail );
                                }

                                // 已处理，完成这个请求
                                recvCtx->changeState(stateFinish);
                            }
                            else
                            {
                                // 重投这个IO请求和Timer
                                recvCtx->startTime = winux::GetUtcTimeMs();
                                if ( recvCtx->timeoutMs != -1 )
                                {
                                    eiennet::async::Timer::New( *recvCtx->sock->getService() )->waitAsyncEx( recvCtx->timeoutMs, false, [this] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                        _IoSocketCtxTimeoutCallback( timer, timerCtx, *this );
                                    }, recvCtx, recvCtx->sock->getThread() );
                                }
                            }

                            goto EXIT_EPOLLIN_IOMAP_LOOP;
                        }
                        break;
                    case ioRecvFrom:
                        {
                            auto * recvFromCtx = dynamic_cast<IoRecvFromCtx *>(*it);
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
                                    winux::ScopeUnguard unguard(this->_mtx);
                                    recvFromCtx->cbOk( recvFromCtx->sock, recvFromCtx->data, recvFromCtx->epFrom );
                                }

                                // 已处理，完成这个请求
                                recvFromCtx->changeState(stateFinish);
                            }
                            else
                            {
                                // 重投这个IO请求和Timer
                                recvFromCtx->startTime = winux::GetUtcTimeMs();
                                if ( recvFromCtx->timeoutMs != -1 )
                                {
                                    eiennet::async::Timer::New( *recvFromCtx->sock->getService() )->waitAsyncEx( recvFromCtx->timeoutMs, false, [this] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                        _IoSocketCtxTimeoutCallback( timer, timerCtx, *this );
                                    }, recvFromCtx, recvFromCtx->sock->getThread() );
                                }
                            }

                            goto EXIT_EPOLLIN_IOMAP_LOOP;
                        }
                        break;
                    case ioTimer:
                        {
                            auto * timerCtx = dynamic_cast<IoTimerCtx *>(*it);
                            auto timer = timerCtx->timer;
                            // 读取timer
                            uint64_t cnt = 0;
                            read( timer->get(), &cnt, sizeof(uint64_t) );

                            if ( timerCtx->cbOk )
                            {
                                winux::ScopeUnguard unguard(this->_mtx);
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

            if ( evt.events & EPOLLOUT )
            {
                for ( auto it = ioVecStruct.ctxs.begin(); it != ioVecStruct.ctxs.end(); it++ )
                {
                    switch ( (*it)->type )
                    {
                    case ioConnect:
                        {
                            auto * cnnCtx = dynamic_cast<IoConnectCtx *>(*it);
                            _IoSocketCtxClearTimerCtx(cnnCtx);

                            cnnCtx->costTimeMs = winux::GetUtcTimeMs() - cnnCtx->startTime;
                            // 处理回调
                            if ( cnnCtx->cbOk )
                            {
                                winux::ScopeUnguard unguard(this->_mtx);
                                cnnCtx->cbOk( cnnCtx->sock, cnnCtx->costTimeMs );
                            }

                            // 已处理，完成这个请求
                            cnnCtx->changeState(stateFinish);

                            goto EXIT_EPOLLOUT_IOMAP_LOOP;
                        }
                        break;
                    case ioSend:
                        {
                            auto * sendCtx = dynamic_cast<IoSendCtx *>(*it);
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
                                    winux::ScopeUnguard unguard(this->_mtx);
                                    sendCtx->cbOk( sendCtx->sock, sendCtx->hadBytes, sendCtx->costTimeMs, sendCtx->cnnAvail );
                                }

                                // 已处理，完成这个请求
                                sendCtx->changeState(stateFinish);
                            }
                            else
                            {
                                // 重投这个IO请求和Timer
                                sendCtx->startTime = winux::GetUtcTimeMs();
                                if ( sendCtx->timeoutMs != -1 )
                                {
                                    eiennet::async::Timer::New( *sendCtx->sock->getService() )->waitAsyncEx( sendCtx->timeoutMs, false, [this] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                        _IoSocketCtxTimeoutCallback( timer, timerCtx, *this );
                                    }, sendCtx, sendCtx->sock->getThread() );
                                }
                            }

                            goto EXIT_EPOLLOUT_IOMAP_LOOP;
                        }
                        break;
                    case ioSendTo:
                        {
                            auto * sendToCtx = dynamic_cast<IoSendToCtx *>(*it);
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
                                    winux::ScopeUnguard unguard(this->_mtx);
                                    sendToCtx->cbOk( sendToCtx->sock, sendToCtx->hadBytes, sendToCtx->costTimeMs );
                                }

                                // 已处理，完成这个请求
                                sendToCtx->changeState(stateFinish);
                            }
                            else
                            {
                                // 重投这个IO请求和Timer
                                sendToCtx->startTime = winux::GetUtcTimeMs();
                                if ( sendToCtx->timeoutMs != -1 )
                                {
                                    eiennet::async::Timer::New( *sendToCtx->sock->getService() )->waitAsyncEx( sendToCtx->timeoutMs, false, [this] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                        _IoSocketCtxTimeoutCallback( timer, timerCtx, *this );
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

            if ( evt.events & EPOLLERR ) // 错误事件，清理相关IoCtx
            {
                auto it = ioVecStruct.ctxs.begin();
                if ( it != ioVecStruct.ctxs.end() )
                {
                    if ( (*it)->type != ioTimer ) // 非定时器的错误事件处理
                    {
                        auto * sockCtx = dynamic_cast<IoSocketCtx *>(*it);
                        auto sock = sockCtx->sock;
                        {
                            winux::ScopeUnguard unguard(this->_mtx);
                            sock->onError(sock);
                        }
                    }
                }

                // 删除此fd的所有IoCtxs
                for ( it = ioVecStruct.ctxs.begin(); it != ioVecStruct.ctxs.end(); it++ )
                {
                    //winux::ScopeUnguard unguard(this->_mtx);
                    //this->delIoCtxs(evt.data.fd);
                    (*it)->changeState(stateProactiveCancel); // 主动取消
                }
            }
        }
    } // for ( int i = 0; i < rc; i++ )
}

void IoEventsData::_handleIoEventsTimeoutAndDelete()
{
    winux::ScopeGuard guard(this->_mtx);
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

            if ( ioCtx->state != stateNormal ) // 不是正常状态了，说明要么是超时取消了，要么是主动取消了，或者是已经完成了，这些都需要删除掉
            {
                if ( ioCtx->type == ioTimer ) // Timer的事件处理
                {
                    //auto * timerCtx = dynamic_cast<IoTimerCtx *>(ioCtx);
                    //auto timer = timerCtx->timer;
                    //this->_epoll->del(fd); // 从epoll中删除这个定时器事件

                    ioVecStruct.events &= ~EPOLLIN; // 从事件掩码中删除EPOLLIN事件
                    this->_epoll->mod( fd, ioVecStruct.events ); // 更新这个fd的事件监听

                    it = ioVec.erase(it); // 删除已取消或者已完成的IO事件
                    hasEraseInIoVec = true;
                    // 删除这个IoCtx
                    ioCtx->decRef();
                }
                else // Socket的事件处理
                {
                    auto * sockIoCtx = dynamic_cast<IoSocketCtx *>(ioCtx);
                    if ( ioCtx->state == stateTimeoutCancel ) // 超时取消，处理超时响应
                    {
                        switch ( sockIoCtx->type )
                        {
                        case ioAccept:
                            {
                                auto * ctx = static_cast<IoAcceptCtx *>(sockIoCtx);
                                if ( ctx->cbTimeout )
                                {
                                    winux::ScopeUnguard unguard(this->_mtx);
                                    if ( ctx->cbTimeout( ctx->sock, ctx ) )
                                    {
                                        ctx->state = stateNormal; // 重新设置为正常状态
                                        // 重投这个IO请求和Timer
                                        ctx->startTime = winux::GetUtcTimeMs();
                                        if ( ctx->timeoutMs != -1 )
                                        {
                                            eiennet::async::Timer::New( *ctx->sock->getService() )->waitAsyncEx( ctx->timeoutMs, false, [this] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                                _IoSocketCtxTimeoutCallback( timer, timerCtx, *this );
                                            }, ctx, ctx->sock->getThread() );
                                        }
                                    }
                                    else
                                    {
                                        ioVecStruct.events &= ~EPOLLIN; // 从事件掩码中删除EPOLLIN事件
                                        this->_epoll->mod( fd, ioVecStruct.events ); // 更新这个fd的事件监听

                                        it = ioVec.erase(it); // 删除已取消或者已完成的IO事件
                                        hasEraseInIoVec = true;
                                        // 删除这个IoCtx
                                        ioCtx->decRef();
                                    }
                                }
                                else
                                {
                                    ctx->state = stateNormal; // 重新设置为正常状态
                                    // 重投这个IO请求和Timer
                                    ctx->startTime = winux::GetUtcTimeMs();
                                    if ( ctx->timeoutMs != -1 )
                                    {
                                        eiennet::async::Timer::New( *ctx->sock->getService() )->waitAsyncEx( ctx->timeoutMs, false, [this] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                            _IoSocketCtxTimeoutCallback( timer, timerCtx, *this );
                                        }, ctx, ctx->sock->getThread() );
                                    }
                                }
                            }
                            break;
                        case ioConnect:
                            {
                                auto * ctx = static_cast<IoConnectCtx *>(sockIoCtx);
                                if ( ctx->cbTimeout )
                                {
                                    winux::ScopeUnguard unguard(this->_mtx);
                                    ctx->cbTimeout( ctx->sock, ctx );
                                }
                                ioVecStruct.events &= ~EPOLLOUT; // 从事件掩码中删除EPOLLOUT事件
                                this->_epoll->mod( fd, ioVecStruct.events ); // 更新这个fd的事件监听

                                it = ioVec.erase(it); // 删除已取消或者已完成的IO事件
                                hasEraseInIoVec = true;
                                // 删除这个IoCtx
                                ioCtx->decRef();
                            }
                            break;
                        case ioRecv:
                            {
                                auto * ctx = static_cast<IoRecvCtx *>(sockIoCtx);
                                if ( ctx->cbTimeout )
                                {
                                    winux::ScopeUnguard unguard(this->_mtx);
                                    ctx->cbTimeout( ctx->sock, ctx );
                                }
                                ioVecStruct.events &= ~EPOLLIN; // 从事件掩码中删除EPOLLIN事件
                                this->_epoll->mod( fd, ioVecStruct.events ); // 更新这个fd的事件监听

                                it = ioVec.erase(it); // 删除已取消或者已完成的IO事件
                                hasEraseInIoVec = true;
                                // 删除这个IoCtx
                                ioCtx->decRef();
                            }
                            break;
                        case ioSend:
                            {
                                auto * ctx = static_cast<IoSendCtx *>(sockIoCtx);
                                if ( ctx->cbTimeout )
                                {
                                    winux::ScopeUnguard unguard(this->_mtx);
                                    ctx->cbTimeout( ctx->sock, ctx );
                                }
                                ioVecStruct.events &= ~EPOLLOUT; // 从事件掩码中删除EPOLLOUT事件
                                this->_epoll->mod( fd, ioVecStruct.events ); // 更新这个fd的事件监听

                                it = ioVec.erase(it); // 删除已取消或者已完成的IO事件
                                hasEraseInIoVec = true;
                                // 删除这个IoCtx
                                ioCtx->decRef();
                            }
                            break;
                        case ioRecvFrom:
                            {
                                auto * ctx = static_cast<IoRecvFromCtx *>(sockIoCtx);
                                if ( ctx->cbTimeout )
                                {
                                    winux::ScopeUnguard unguard(this->_mtx);
                                    ctx->cbTimeout( ctx->sock, ctx );
                                }
                                ioVecStruct.events &= ~EPOLLIN; // 从事件掩码中删除EPOLLIN事件
                                this->_epoll->mod( fd, ioVecStruct.events ); // 更新这个fd的事件监听

                                it = ioVec.erase(it); // 删除已取消或者已完成的IO事件
                                hasEraseInIoVec = true;
                                // 删除这个IoCtx
                                ioCtx->decRef();
                            }
                            break;
                        case ioSendTo:
                            {
                                auto * ctx = static_cast<IoSendToCtx *>(sockIoCtx);
                                if ( ctx->cbTimeout )
                                {
                                    winux::ScopeUnguard unguard(this->_mtx);
                                    ctx->cbTimeout( ctx->sock, ctx );
                                }
                                ioVecStruct.events &= ~EPOLLOUT; // 从事件掩码中删除EPOLLOUT事件
                                this->_epoll->mod( fd, ioVecStruct.events ); // 更新这个fd的事件监听

                                it = ioVec.erase(it); // 删除已取消或者已完成的IO事件
                                hasEraseInIoVec = true;
                                // 删除这个IoCtx
                                ioCtx->decRef();
                            }
                            break;
                        }
                    }
                    else // ioCtx->state != stateTimeoutCancel
                    {
                        // 清除sockIoCtx类型对应的epoll事件
                        switch ( sockIoCtx->type )
                        {
                        case ioAccept:
                        case ioRecv:
                        case ioRecvFrom:
                        case ioTimer:
                            ioVecStruct.events &= ~EPOLLIN;
                            break;
                        case ioConnect:
                        case ioSend:
                        case ioSendTo:
                            ioVecStruct.events &= ~EPOLLOUT;
                            break;
                        }
                        this->_epoll->mod( fd, ioVecStruct.events ); // 更新这个fd的事件监听

                        it = ioVec.erase(it); // 删除已取消或者已完成的IO事件
                        hasEraseInIoVec = true;
                        // 删除这个IoCtx
                        ioCtx->decRef();
                    }
                }
            } // ioCtx->state != stateNormal

            // 如果已经是end则不能再++it
            if ( !hasEraseInIoVec && it != ioVec.end() ) ++it;
        } // for ( auto it = ioVec.begin(); it != ioVec.end(); hasEraseInIoVec = false )

        // 如果IO表已空，则删除该fd
        if ( ioVec.empty() )
        {
            this->_epoll->del(fd); // 从epoll中删除这个fd
            itVecStruct = this->_ioVecMap.erase(itVecStruct);
            hasEraseInIoVecMap = true;
        }

        // 如果已经是end则不能再++it
        if ( !hasEraseInIoVecMap && itVecStruct != this->_ioVecMap.end() ) ++itVecStruct;
    }
}

void IoEventsData::wakeUpTrigger( WakeUpType type )
{
    uint64_t t = (uint64_t)type;
    write( this->_wakeUpEventFd.get(), &t, sizeof(t) );
}

void IoEventsData::prePost( IoCtx * ioCtx )
{
    winux::ScopeGuard guard(this->_mtxPreIoCtxs);
    this->_preIoCtxs.push_back(ioCtx);
}

void IoEventsData::setIoCtx( IoCtx * ioCtx )
{
    winux::ScopeGuard guard(this->_mtx);
    int fd = ioCtx->type == ioTimer ?
        dynamic_cast<IoTimerCtx *>(ioCtx)->timer->get() :
        dynamic_cast<IoSocketCtx *>(ioCtx)->sock->get();

    if ( winux::IsSet( this->_ioVecMap, fd ) )
    {
        this->_ioVecMap[fd].set(ioCtx);
        auto & events = this->_ioVecMap[fd].events;
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
        this->_ioVecMap[fd].set(ioCtx);
        auto & events = this->_ioVecMap[fd].events;
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

void IoEventsData::delIoCtx( IoCtx * ioCtx )
{
    winux::ScopeGuard guard(this->_mtx);
    int fd = ioCtx->type == ioTimer ?
        dynamic_cast<IoTimerCtx *>(ioCtx)->timer->get() :
        dynamic_cast<IoSocketCtx *>(ioCtx)->sock->get();

    auto & ioVecStruct = this->_ioVecMap[fd];
    ioVecStruct.remove(ioCtx);

    if ( ioVecStruct.ctxs.empty() )
    {
        // 移除对应fd所有IO
        this->_ioVecMap.erase(fd);
        // 从epoll中删除这个fd
        this->_epoll->del(fd);
    }
    else
    {
        auto & events = ioVecStruct.events;
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

void IoEventsData::delIoCtxs( int fd )
{
    winux::ScopeGuard guard(this->_mtx);
    // 应该先从epoll中DEL相应的fd
    this->_epoll->del(fd);

    if ( winux::IsSet( this->_ioVecMap, fd ) )
    {
        std::vector<int> wantRemoveTimerFds;
        auto & ioVec = this->_ioVecMap[fd].ctxs;
        for ( auto * ioCtx : ioVec )
        {
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
                    int timerfd = timer->get();
                    // 先从epoll中DEL相应的timerfd
                    this->_epoll->del(timerfd);
                    if ( timer->stop() )
                    {
                        sockIoCtx->timerCtx->decRef();
                    }
                    this->_timerIoCount--;
                    // 再从ioVecMap中移除对应的timerfd
                    wantRemoveTimerFds.push_back(timerfd);
                }
                sockIoCtx->changeState(stateProactiveCancel);
                sockIoCtx->decRef();
                this->_sockIoCount--;
            }
        }
        // 移除对应fd所有IO
        this->_ioVecMap.erase(fd);
        // 删除sock io的超时timer io
        for ( auto fd1 : wantRemoveTimerFds )
        {
            this->_ioVecMap.erase(fd1);
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
}

void IoService::stop()
{
    this->_stop = true;
    // 投递停止信号
    this->_ioEvents.wakeUpTrigger(IoEventsData::wutWantStop);
    for ( size_t i = 0; i < _group.count(); i++ )
    {
        // 给每个线程投递退出信号
        auto * th = this->getGroupThread<IoServiceThread>(i);
        th->_stop = true;
        // 投递停止信号
        th->_ioEvents.wakeUpTrigger(IoEventsData::wutWantStop);
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

    IoEventsData & ioEvents = sock->getThread() ? sock->getThread<IoServiceThread>()->_ioEvents : this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*this)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents.prePost(ctx);

    ioEvents.wakeUpTrigger(IoEventsData::wutWantUpdate);
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

    IoEventsData & ioEvents = sock->getThread() ? sock->getThread<IoServiceThread>()->_ioEvents : this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*this)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents.prePost(ctx);

    ioEvents.wakeUpTrigger(IoEventsData::wutWantUpdate);
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

    IoEventsData & ioEvents = sock->getThread() ? sock->getThread<IoServiceThread>()->_ioEvents : this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*this)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents.prePost(ctx);

    ioEvents.wakeUpTrigger(IoEventsData::wutWantUpdate);
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

    IoEventsData & ioEvents = sock->getThread() ? sock->getThread<IoServiceThread>()->_ioEvents : this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*this)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents.prePost(ctx);

    ioEvents.wakeUpTrigger(IoEventsData::wutWantUpdate);
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

    IoEventsData & ioEvents = sock->getThread() ? sock->getThread<IoServiceThread>()->_ioEvents : this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*this)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents.prePost(ctx);

    ioEvents.wakeUpTrigger(IoEventsData::wutWantUpdate);
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

    IoEventsData & ioEvents = sock->getThread() ? sock->getThread<IoServiceThread>()->_ioEvents : this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        eiennet::async::Timer::New(*this)->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents.prePost(ctx);

    ioEvents.wakeUpTrigger(IoEventsData::wutWantUpdate);
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

    IoEventsData & ioEvents = timer->getThread() ? timer->getThread<IoServiceThread>()->_ioEvents : this->_ioEvents;
    // 投递 IO
    ioEvents.prePost(timerCtx);

    timer->set( timeoutMs, periodic );

    // 如果有关联的IoSocketCtx就不必唤醒更新，因为IoSocketCtx投递时会唤醒
    if ( !assocCtx)
    {
        ioEvents.wakeUpTrigger(IoEventsData::wutWantUpdate);
    }
}

void IoService::timerTrigger( io::IoTimerCtx * timerCtx )
{
}

void IoService::removeSock( winux::SharedPointer<eiennet::async::Socket> sock )
{
    IoEventsData & ioEvents = sock->getThread() ? sock->getThread<IoServiceThread>()->_ioEvents : this->_ioEvents;
    winux::ScopeGuard guard(ioEvents._mtx);
    auto & ioVecMap = ioEvents._ioVecMap;
    auto itVecStruct = ioVecMap.find( sock->get() );
    if ( itVecStruct != ioVecMap.end() )
    {
        for ( auto * ioCtx : itVecStruct->second.ctxs )
        {
            auto * sockIoCtx = dynamic_cast<IoSocketCtx *>(ioCtx);
            if ( sockIoCtx->timerCtx )
            {
                auto timer = sockIoCtx->timerCtx->timer;
                timer->stop();
            }
            sockIoCtx->changeState(stateProactiveCancel);
        }
    }
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
