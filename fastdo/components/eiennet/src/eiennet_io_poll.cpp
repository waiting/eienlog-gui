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
    #include <sys/poll.h>
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
#include "eiennet_io_poll.hpp"

namespace io
{
// struct Poll_Data ---------------------------------------------------------------------------
struct Poll_Data
{
    std::vector<pollfd> pollFds;

    Poll_Data()
    {
    }
};

// class Poll ---------------------------------------------------------------------------------
short const Poll::PollIn = POLLIN;
short const Poll::PollPri = POLLPRI;
short const Poll::PollOut = POLLOUT;
short const Poll::PollErr = POLLERR;
short const Poll::PollHup = POLLHUP;
short const Poll::PollNVal = POLLNVAL;

Poll::Poll()
{

}

Poll::~Poll()
{

}

size_t Poll::find( int fd ) const
{
    auto it = std::find_if( _self->pollFds.begin(), _self->pollFds.end(), [fd] ( pollfd const & e ) { return e.fd == fd; } );
    if ( it != _self->pollFds.end() )
    {
        return std::distance( _self->pollFds.begin(), it );
    }
    return winux::npos;
}

bool Poll::add( int fd, short events )
{
    size_t i = _self->pollFds.size();
    _self->pollFds.emplace_back();

    pollfd & e = _self->pollFds[i];
    e.fd = fd;
    e.events = events;
    e.revents = 0;
    return true;
}

bool Poll::set( size_t i, int fd, short events )
{
    if ( i >= _self->pollFds.size() )
    {
        i = _self->pollFds.size();
        _self->pollFds.emplace_back();
    }

    pollfd & e = _self->pollFds[i];
    e.fd = fd;
    e.events = events;
    return true;
}

bool Poll::del( int fd )
{
    auto it = std::find_if( _self->pollFds.begin(), _self->pollFds.end(), [fd] ( pollfd const & e ) { return e.fd == fd; } );
    if ( it != _self->pollFds.end() )
    {
        _self->pollFds.erase(it);
        return true;
    }
    return false;
}

void Poll::clear()
{
    _self->pollFds.clear();
}

int Poll::wait( double sec )
{
    int timeout = -1;
    if ( sec >= 0 )
    {
        timeout = (int)( sec * 1000 );
    }

#if defined(OS_WIN)
    return WSAPoll( _self->pollFds.data(), (ULONG)_self->pollFds.size(), timeout );
#else
    return ::poll( _self->pollFds.data(), (nfds_t)_self->pollFds.size(), timeout );
#endif
}

size_t Poll::traverseEvents( EvtFn cbEvt )
{
    size_t c = 0;
    bool hasEraseInPollFds = false;
    for ( auto it = _self->pollFds.begin(); it != _self->pollFds.end(); hasEraseInPollFds = false )
    {
        if ( it->revents != 0 )
        {
            if ( it->revents & POLLNVAL ) // 如果是无效的fd，则删除这个fd
            {
                it = _self->pollFds.erase(it);
                hasEraseInPollFds = true;
            }
            else
            {
                cbEvt( (int)it->fd, it->revents );
                c++;
            }
        }

        if ( !hasEraseInPollFds && it != _self->pollFds.end() ) it++;
    }
    return c;
}

///////////////////////////////////////////////////////////////////////////////////////////////
namespace poll
{
// IoSocketCtx 超时处理
static void _IoSocketCtxTimeoutCallback( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx, IoEventsData & ioEvents )
{
    auto * assocCtx = timerCtx->assocCtx;
    if ( assocCtx )
    {
        assocCtx->timerCtx = nullptr; // 取消关联的超时timer场景
        assocCtx->cancel(cancelTimeout); // 超时取消操作
        assocCtx->changeState(stateCancel);
    }
}

// IoSocketCtx 清空超时事件关联，并停止超时定时器
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

// 取消`IoVecStruct`中的IoCtxs
static void _CancelIoCtxs( IoEventsData::IoVecStruct * ioVecStruct )
{
    bool hasEraseInIoVec = false;
    for ( auto it = ioVecStruct->ctxs.begin(); it != ioVecStruct->ctxs.end(); hasEraseInIoVec = false )
    {
        auto * ioCtx = *it;
        switch ( ioCtx->type )
        {
        case ioTimer:
            {
                auto * timerCtx = dynamic_cast<IoTimerCtx *>(ioCtx);
                auto timer = timerCtx->timer;
                timer->stop();
            }
            break;
        default:
            {
                auto * sockCtx = dynamic_cast<IoSocketCtx *>(ioCtx);
                if ( sockCtx->timerCtx )
                {
                    auto timer = sockCtx->timerCtx->timer;
                    timer->stop();
                }
                sockCtx->cancel(cancelProactive);
                sockCtx->changeState(stateCancel);
            }
            break;
        }

        // 如果已经是end则不能再++it
        if ( !hasEraseInIoVec && it != ioVecStruct->ctxs.end() ) ++it;
    } // for ( auto it = ioVecStruct->ctxs.begin(); it != ioVecStruct->ctxs.end(); hasEraseInIoVec = false )
}

// 获取IoCtx对应的fd
inline static int _GetFdByIoCtx( io::IoCtx * ioCtx )
{
    int fd;
    if ( ioCtx->type == ioTimer )
    {
    #if defined(OS_WIN)
        fd = dynamic_cast<io::poll::IoTimerCtx *>(ioCtx)->_sockSignal.get();
    #else
        fd = dynamic_cast<io::poll::IoTimerCtx *>(ioCtx)->timer->get();
    #endif
    }
    else
    {
        fd = dynamic_cast<io::IoSocketCtx *>(ioCtx)->sock->get();
    }
    return fd;
}

// Poll工作函数 --------------------------------------------------------------------------------
void _PollWorkerFunc( IoService * serv, IoServiceThread * thread, IoEventsData & ioEvents, bool * stop )
{
    while ( !*stop )
    {
        ioEvents._handleIoCtxsPost();

        int rc = ioEvents._poll.wait();
        if ( rc < 0 )
        {
            if ( errno == EINTR ) continue;

        }
        else // rc >= 0
        {
            ioEvents._handleIoCtxsCallback(rc);
        }
        ioEvents._handleIoCtxsTimeoutAndDelete();
    }
}

// class IoEventsData -------------------------------------------------------------------------
IoEventsData::IoEventsData() : _mtxPreIoCtxs(true), _portSockWakeUp(0), _sockIoCount(0), _timerIoCount(0)
{
    if ( _sockWakeUp.bind( eiennet::ip::EndPoint( "", 0 ) ) )
    {
        eiennet::ip::EndPoint ep;
        _sockWakeUp.getBoundEp(&ep);
        _portSockWakeUp = ep.getPort();

        // 监听wake up事件
        this->_poll.add( this->_sockWakeUp.get(), Poll::PollIn );
    }
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

void IoEventsData::_handleIoCtxsCallback( int rc )
{
    this->_poll.traverseEvents( [this] ( int fd, short events ) {
        if ( fd == this->_sockWakeUp.get() ) // 处理wake up事件
        {
            eiennet::ip::EndPoint ep;
            this->_sockWakeUp.recvFrom( &ep, sizeof(winux::ushort) * 16 );
        }
        else // 处理IO事件
        {
            auto & ioVecStruct = this->_ioVecMap[fd];
            if ( events & Poll::PollIn )
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

                            goto EXIT_POLLIN_IOCTX_LOOP;
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

                            goto EXIT_POLLIN_IOCTX_LOOP;
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

                            goto EXIT_POLLIN_IOCTX_LOOP;
                        }
                        break;
                    case ioTimer:
                        {
                            auto * timerCtx = dynamic_cast<IoTimerCtx *>(*it);
                            auto timer = timerCtx->timer;
                            // 读取timer
                        #if defined(OS_WIN)
                            eiennet::ip::EndPoint ep;
                            timerCtx->_sockSignal.recvFrom( &ep, sizeof(winux::uint64) );
                        #else
                            uint64_t cnt = 0;
                            read( timer->get(), &cnt, sizeof(uint64_t) );
                        #endif

                            if ( timerCtx->cbOk )
                            {
                                timerCtx->cbOk( timer, timerCtx );
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

                            goto EXIT_POLLIN_IOCTX_LOOP;
                        }
                        break;
                    }
                }
            EXIT_POLLIN_IOCTX_LOOP:
                ;
            }

            if ( events & Poll::PollOut )
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
                                cnnCtx->cbOk( cnnCtx->sock, cnnCtx->costTimeMs );
                            }

                            // 已处理，完成这个请求
                            cnnCtx->changeState(stateFinish);

                            goto EXIT_POLLOUT_IOCTX_LOOP;
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

                            goto EXIT_POLLOUT_IOCTX_LOOP;
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

                            goto EXIT_POLLOUT_IOCTX_LOOP;
                        }
                        break;
                    }
                }
            EXIT_POLLOUT_IOCTX_LOOP:
                ;
            }

            if ( events & Poll::PollHup ) // 连接断开
            {
                for ( auto it = ioVecStruct.ctxs.begin(); it != ioVecStruct.ctxs.end(); it++ )
                {
                    switch ( (*it)->type )
                    {
                    case ioRecv:
                        {
                            auto * recvCtx = dynamic_cast<IoRecvCtx *>(*it);
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

                            goto EXIT_POLLHUP_IOCTX_LOOP;
                        }
                        break;
                    case ioSend:
                        {
                            auto * sendCtx = dynamic_cast<IoSendCtx *>(*it);
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

                            goto EXIT_POLLHUP_IOCTX_LOOP;
                        }
                        break;
                    }
                }
            EXIT_POLLHUP_IOCTX_LOOP:
                ;
            }

            if ( events & Poll::PollErr ) // 错误事件，清理相关IoCtx
            {
                auto it = ioVecStruct.ctxs.begin();
                if ( it != ioVecStruct.ctxs.end() )
                {
                    if ( (*it)->type != ioTimer ) // 非定时器的错误事件处理
                    {
                        auto * sockCtx = dynamic_cast<IoSocketCtx *>(*it);
                        // Socket出错处理
                        auto sock = sockCtx->sock;
                        sock->onError(sock);
                    }
                }

                // 删除此fd的所有IoCtxs
                _CancelIoCtxs(&ioVecStruct);
            }
        }
    } );
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

            if ( ioCtx->state != stateNormal ) // 不是正常状态了，说明要么是超时取消了，要么是主动取消了，或者是已经完成了，这些都需要删除掉
            {
                if ( ioCtx->type == ioTimer ) // Timer的事件处理
                {
                    ioVecStruct.events &= ~Poll::PollIn; // 从事件掩码中删除POLLIN事件
                    this->_poll.mod( fd, ioVecStruct.events ); // 更新这个fd的事件监听

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
                                        ioVecStruct.events &= ~Poll::PollIn; // 从事件掩码中删除POLLIN事件
                                        this->_poll.mod( fd, ioVecStruct.events ); // 更新这个fd的事件监听

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
                                auto * ctx = static_cast<IoConnectCtx *>(sockCtx);
                                if ( ctx->cbTimeout )
                                {
                                    ctx->cbTimeout( ctx->sock, ctx );
                                }

                                ioVecStruct.events &= ~Poll::PollOut; // 从事件掩码中删除POLLOUT事件
                                this->_poll.mod( fd, ioVecStruct.events ); // 更新这个fd的事件监听

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

                                ioVecStruct.events &= ~Poll::PollIn; // 从事件掩码中删除POLLIN事件
                                this->_poll.mod( fd, ioVecStruct.events ); // 更新这个fd的事件监听

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

                                ioVecStruct.events &= ~Poll::PollOut; // 从事件掩码中删除POLLOUT事件
                                this->_poll.mod( fd, ioVecStruct.events ); // 更新这个fd的事件监听

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

                                ioVecStruct.events &= ~Poll::PollIn; // 从事件掩码中删除POLLIN事件
                                this->_poll.mod( fd, ioVecStruct.events ); // 更新这个fd的事件监听

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

                                ioVecStruct.events &= ~Poll::PollOut; // 从事件掩码中删除POLLOUT事件
                                this->_poll.mod( fd, ioVecStruct.events ); // 更新这个fd的事件监听

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
                        // 清除sockIoCtx类型对应的poll事件
                        switch ( type )
                        {
                        case ioAccept:
                        case ioRecv:
                        case ioRecvFrom:
                        case ioTimer:
                            ioVecStruct.events &= ~Poll::PollIn;
                            break;
                        case ioConnect:
                        case ioSend:
                        case ioSendTo:
                            ioVecStruct.events &= ~Poll::PollOut;
                            break;
                        }
                        this->_poll.mod( fd, ioVecStruct.events ); // 更新这个fd的事件监听

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
            this->_poll.del(fd); // 从poll中删除这个fd
            itVecStruct = this->_ioVecMap.erase(itVecStruct);
            hasEraseInIoVecMap = true;
        }

        // 如果已经是end则不能再++it
        if ( !hasEraseInIoVecMap && itVecStruct != this->_ioVecMap.end() ) ++itVecStruct;
    }
}

void IoEventsData::wakeUpTrigger( WakeUpType type )
{
    winux::ushort t = (winux::ushort)type;
    _sockWakeUp.sendTo( eiennet::ip::EndPoint( "127.0.0.1", _portSockWakeUp ), &t, sizeof(t) );
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

                    ioVecStruct.events |= Poll::PollIn;
                    this->_poll.mod( timerFd, ioVecStruct.events );

                    this->_timerIoCount++;
                }
            }
            else // 未存在此timer
            {
                auto & ioVecStruct = this->_ioVecMap[timerFd];
                auto & ioVec = ioVecStruct.ctxs;

                ioVec.push_back(timerCtx);

                ioVecStruct.events |= Poll::PollIn;
                this->_poll.add( timerFd, ioVecStruct.events );

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
                auto it = std::find_if( ioVec.begin(), ioVec.end(), [ioCtx] ( IoCtx * e ) { return e->type == ioCtx->type; } );
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
                            this->_poll.del(timerFd); // 从poll中删除这个timerFd
                            existingCtx->timerCtx->decRef();
                            // 删除关联的超时timer
                            this->_ioVecMap.erase(timerFd);
                        }
                        existingCtx->timerCtx = nullptr; // 解除关联
                    }
                    existingCtx->decRef(); // 释放已存在的IoCtx
                    ioVec.erase(it);

                    ioVec.push_back(sockIoCtx);
                }
                else // 未存在此类型的IoCtx
                {
                    ioVec.push_back(sockIoCtx);

                    switch ( sockIoCtx->type )
                    {
                    case ioAccept:
                    case ioRecv:
                    case ioRecvFrom:
                        ioVecStruct.events |= Poll::PollIn;
                        break;
                    case ioConnect:
                    case ioSend:
                    case ioSendTo:
                        ioVecStruct.events |= Poll::PollOut;
                        break;
                    }
                    this->_poll.mod( sockFd, ioVecStruct.events );

                    this->_sockIoCount++;

                }
            }
            else // 未存在此socket
            {
                auto & ioVecStruct = this->_ioVecMap[sockFd];
                auto & ioVec = ioVecStruct.ctxs;

                ioVec.push_back(sockIoCtx);

                switch ( sockIoCtx->type )
                {
                case ioAccept:
                case ioRecv:
                case ioRecvFrom:
                    ioVecStruct.events |= Poll::PollIn;
                    break;
                case ioConnect:
                case ioSend:
                case ioSendTo:
                    ioVecStruct.events |= Poll::PollOut;
                    break;
                }
                this->_poll.add( sockFd, ioVecStruct.events );

                this->_sockIoCount++;
            }
        }
        break;
    }
}

// class IoServiceThread ----------------------------------------------------------------------
void IoServiceThread::run()
{
    _PollWorkerFunc( this->_serv, this, this->_ioEvents, &this->_stop );
}

void IoServiceThread::timerTrigger( io::IoTimerCtx * timerCtx )
{
    auto myTimerCtx = static_cast<IoTimerCtx *>(timerCtx);
#if defined(OS_WIN)
    winux::uint64 cnt = 1;
    myTimerCtx->_sockSignal.sendTo( eiennet::ip::EndPoint( "127.0.0.1", myTimerCtx->_portSockSignal ), &cnt, sizeof(cnt) );
#endif
}

// class IoService ----------------------------------------------------------------------------
IoService::IoService( size_t threadCount ) : _stop(false)
{
    // 创建工作线程组
    this->_group.create<IoServiceThread>( threadCount, this );
    // 设置模型类型
    this->_model = modelPoll;
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
    _PollWorkerFunc( this, nullptr, this->_ioEvents, &this->_stop );
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

    // 唤醒更新
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

    // 唤醒更新
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

    // 唤醒更新
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

    // 唤醒更新
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

    // 唤醒更新
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

    // 唤醒更新
    ioEvents.wakeUpTrigger(IoEventsData::wutWantUpdate);
}

void IoService::postTimer( winux::SharedPointer<eiennet::async::Timer> timer, winux::uint64 timeoutMs, bool periodic, IoTimerCtx::OkFn cbOk, IoSocketCtx * assocCtx, io::IoServiceThread * th )
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
#if defined(OS_WIN)
    winux::uint64 cnt = 1;
    myTimerCtx->_sockSignal.sendTo( eiennet::ip::EndPoint( "127.0.0.1", myTimerCtx->_portSockSignal ), &cnt, sizeof(cnt) );
#endif
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
            ioCtx->changeState(stateCancel);
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


} // namespace poll


} // namespace io
