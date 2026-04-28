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
static void _IoSocketCtxTimeoutCallback( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx )
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
        if ( timer->stop() ) ctx->timerCtx->decRef();
        ctx->timerCtx = nullptr;
    }
}

// EPOLL工作函数 ----------------------------------------------------------------------------------
void _EpollWorkerFunc( IoService * serv, IoServiceThread * thread, Epoll * epoll, bool * stop )
{
    while ( !*stop )
    {
        int rc = epoll->wait();
        if ( rc < 0 )
        {
            if ( errno == EINTR ) continue;
            //*stop = false;
        }
        else
        {
            // stop signal eventfd
            auto & stopEventFd = thread ? thread->_stopEventFd : serv->_stopEventFd;
            // ioEvents
            auto & ioEvents = thread ? thread->_ioEvents : serv->_ioEvents;

            for ( int i = 0; i < rc; i++ )
            {
                auto & evt = epoll->evt(i);
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
                                break;
                            case ioRecv:
                                break;
                            case ioRecvFrom:
                                break;
                            case ioTimer:
                                {
                                    auto * timerCtx = dynamic_cast<IoTimerCtx *>(it->second);
                                    auto timer = timerCtx->timer;
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
                                                // 已处理，取消这个请求
                                                timerCtx->changeState(stateFinish);
                                            }
                                            timer->_timerCtx = nullptr;

                                            // 已处理，释放
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
                            auto * ctx = it->second;
                            switch ( ctx->type )
                            {
                            case ioConnect:
                                break;
                            case ioSend:
                                break;
                            case ioSendTo:
                                break;
                            }
                        }
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
                                    winux::ScopeUnguard guard(ioEvents._mtx);
                                    sock->onError(sock);
                                }
                            }
                        }

                        // 删除此fd的所有IoCtxs
                        {
                            winux::ScopeUnguard guard(ioEvents._mtx);
                            ioEvents.delIoCtxs(evt.data.fd);
                        }
                    }
                }
            }
        }
    }
}

// class IoEventsData -------------------------------------------------------------------------
IoEventsData::IoEventsData( Epoll * epoll ) : _mtx(true), _epoll(epoll)
{

}

void IoEventsData::setIoCtx( IoCtx * ioCtx )
{
    winux::ScopeGuard guard(this->_mtx);
    int fd;
    if ( ioCtx->type == ioTimer )
    {
        fd = dynamic_cast<IoTimerCtx *>(ioCtx)->timer->get();
    }
    else
    {
        fd = dynamic_cast<IoSocketCtx *>(ioCtx)->sock->get();
    }
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
        this->_epoll->mod( fd, events );
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
        this->_epoll->add( fd, events );
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
    int fd;
    if ( ioCtx->type == ioTimer )
    {
        auto * ctx = dynamic_cast<IoTimerCtx *>(ioCtx);
        fd = ctx->timer->get();
    }
    else
    {
        auto * ctx = dynamic_cast<IoSocketCtx *>(ioCtx);
        fd = ctx->sock->get();
    }

    auto & ioMap = this->_fdToIoCtxs[fd];
    ioMap.erase(ioCtx->type);
    if ( ioMap.empty() )
    {
        this->_fdToIoCtxs.erase(fd);
        this->_fdToEvents.erase(fd);
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
                    // 再从fdToIoCtxs中移除对应的timerfd
                    wantRemoveTimerFds.push_back(timerfd);
                }
                sockIoCtx->changeState(stateProactiveCancel);
                sockIoCtx->decRef();
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
    _EpollWorkerFunc( this->_serv, this, &this->_epoll, &this->_stop );
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
        auto * th = this->getGroupThread(i);
        th->_stop = true;
        // 投递停止信号
        write( th->_stopEventFd.get(), &u, sizeof(winux::uint64) );
    }
}

int IoService::run()
{
    this->_group.startup();
    _EpollWorkerFunc( this, nullptr, &this->_epoll, &this->_stop );
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
        eiennet::async::Timer::New(*this)->waitAsyncEx( ctx->timeoutMs, false, [] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx );
        }, ctx, sock->getThread() );
    }

    // 投递IO
    ioEvents.setIoCtx(ctx);
}

void IoService::postConnect( winux::SharedPointer<eiennet::async::Socket> sock, eiennet::EndPoint const & ep, IoConnectCtx::OkFn cbOk, winux::uint64 timeoutMs, IoConnectCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{

}

void IoService::postRecv( winux::SharedPointer<eiennet::async::Socket> sock, size_t targetSize, IoRecvCtx::OkFn cbOk, winux::uint64 timeoutMs, IoRecvCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{

}

void IoService::postSend( winux::SharedPointer<eiennet::async::Socket> sock, void const * data, size_t size, IoSendCtx::OkFn cbOk, winux::uint64 timeoutMs, IoSendCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{

}

void IoService::postRecvFrom( winux::SharedPointer<eiennet::async::Socket> sock, size_t targetSize, IoRecvFromCtx::OkFn cbOk, winux::uint64 timeoutMs, IoRecvFromCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{

}

void IoService::postSendTo( winux::SharedPointer<eiennet::async::Socket> sock, eiennet::EndPoint const & ep, void const * data, size_t size, IoSendToCtx::OkFn cbOk, winux::uint64 timeoutMs, IoSendToCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{

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


} // namespace epoll


} // namespace io
