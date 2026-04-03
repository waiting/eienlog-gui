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
// IoSocketCtx 清空超时事件关联，并停止超时定时器，尝试释放IoTimerCtx ------------------------------------------------
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

// EPOLL工作函数 ------------------------------------------------------------------------------
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
                    // 控制事件，清理 eventfd
                    uint64_t value;
                    read( stopEventFd.get(), &value, sizeof(value) );
                    continue;
                }
                
                // 处理其他事件
                auto * ioCtx = reinterpret_cast<IoCtx *>(evt.data.ptr);
                switch ( ioCtx->type )
                {
                case ioAccept:
                    break;
                case ioConnect:
                    break;
                case ioRecv:
                    break;
                case ioSend:
                    break;
                case ioRecvFrom:
                    break;
                case ioSendTo:
                    break;
                case ioTimer:
                    break;
                }

                ioEvents.delIoCtx(ioCtx);
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
    if ( ioCtx->type == ioTimer )
    {
        auto * ctx = dynamic_cast<IoTimerCtx *>(ioCtx);
        this->_fdToIoCtxs[ctx->timer->get()][ctx->type] = ctx;
    }
    else
    {
        auto * ctx = dynamic_cast<IoSocketCtx *>(ioCtx);
        this->_fdToIoCtxs[ctx->sock->get()][ctx->type] = ctx;
    }
}

IoCtx * IoEventsData::getIoCtx( int fd, IoType type )
{
    winux::ScopeGuard guard(this->_mtx);
    if ( winux::isset( this->_fdToIoCtxs, fd ) )
    {
        auto & ioMap = this->_fdToIoCtxs[fd];
        if ( winux::isset( ioMap, type ) )
        {
            return ioMap[type];
        }
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
    }
    else
    {
        auto & events = this->_fdToEvents[fd];
        // 清除ioCtx类型对应的epoll事件
        switch ( ioCtx->type )
        {
        case ioAccept:
            events &= ~EPOLLIN;
            break;
        case ioConnect:
            events &= ~EPOLLOUT;
            break;
        case ioRecv:
            events &= ~EPOLLIN;
            break;
        case ioSend:
            events &= ~EPOLLOUT;
            break;
        case ioRecvFrom:
            events &= ~EPOLLIN;
            break;
        case ioSendTo:
            events &= ~EPOLLOUT;
            break;
        case ioTimer:
            events &= ~EPOLLIN;
            break;
        }

        // 更新epoll监听事件
        for ( auto & pr : ioMap )
        {
            auto * ctx = pr.second;
            switch ( ctx->type )
            {
            case ioAccept:
                events |= EPOLLIN;
                break;
            case ioConnect:
                events |= EPOLLOUT;
                break;
            case ioRecv:
                events |= EPOLLIN;
                break;
            case ioSend:
                events |= EPOLLOUT;
                break;
            case ioRecvFrom:
                events |= EPOLLIN;
                break;
            case ioSendTo:
                events |= EPOLLOUT;
                break;
            case ioTimer:
                events |= EPOLLIN;
                break;
            }
            this->_epoll->mod( fd, events, ctx );
        }
    }
    ioCtx->decRef();
}

bool IoEventsData::hasIoCtxs( int fd ) const
{
    winux::ScopeGuard guard( const_cast<winux::Mutex &>(this->_mtx) );
    return winux::isset( this->_fdToIoCtxs, fd );
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
    if ( winux::isset( this->_fdToIoCtxs, fd ) )
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
        // 从FdIoCtxsMap中移除对应fd所有io
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
    this->_epoll.add( this->_stopEventFd.get(), EPOLLIN, nullptr );
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
    this->_epoll.add( this->_stopEventFd.get(), EPOLLIN, nullptr );
}

void IoService::stop()
{
    winux::uint64 u = 1;
    this->_stop = true;
    write( _stopEventFd.get(), &u, sizeof(winux::uint64) );
    //close( this->_epoll.get() );
    for ( size_t i = 0; i < _group.count(); i++ )
    {
        // 给每个线程投递退出信号
        auto * th = this->getGroupThread(i);
        th->_stop = true;
        write( th->_stopEventFd.get(), &u, sizeof(winux::uint64) );
        //close( th->_epoll.get() );
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
