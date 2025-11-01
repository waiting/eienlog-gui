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
#include "eiennet_io_select.hpp"
#include "eiennet_async.hpp"

namespace io
{
// struct SelectRead_Data ---------------------------------------------------------------------
struct SelectRead_Data
{
    int readFdsCount;
    int maxFd;
    fd_set readFds;

    SelectRead_Data() { this->zeroInit(); }

    void zeroInit()
    {
        FD_ZERO(&this->readFds);
        this->readFdsCount = 0;

        this->maxFd = -2;
    }
};

// class SelectRead ---------------------------------------------------------------------------
SelectRead::SelectRead()
{
}

SelectRead::SelectRead( Socket const & sock )
{
    this->setReadSock(sock);
}

SelectRead::SelectRead( int fd )
{
    this->setReadFd(fd);
}

SelectRead::SelectRead( winux::Mixed const & fds )
{
    this->setReadFds(fds);
}

SelectRead::~SelectRead()
{
}

bool SelectRead::setReadFd( int fd )
{
    if ( Select::ValidFd(fd) )
    {
        FD_SET( fd, &_self->readFds );
        if ( fd > _self->maxFd ) _self->maxFd = fd;
        _self->readFdsCount++;
        return true;
    }
    return false;
}

bool SelectRead::delReadFd( int fd )
{
    if ( Select::ValidFd(fd) )
    {
        FD_CLR( fd, &_self->readFds );
        _self->readFdsCount--;
        return true;
    }
    return false;
}

bool SelectRead::setReadFds( winux::Mixed const & fds )
{
    bool b = true;
    if ( fds.isArray() )
    {
        size_t n = fds.getCount();
        for ( size_t i = 0; i < n; i++ )
        {
            if ( !this->setReadFd( fds[i].toInt() ) )
                b = false;
        }
    }
    else
    {
        b = this->setReadFd( fds.toInt() );
    }

    return b;
}

void SelectRead::clear()
{
    FD_ZERO(&_self->readFds);
    _self->maxFd = -2;
    _self->readFdsCount = 0;
}

int SelectRead::hasReadFd( int fd ) const
{
    return FD_ISSET( fd, &_self->readFds );
}

int SelectRead::getReadFdsCount() const
{
    return _self->readFdsCount;
}

int SelectRead::getReadMaxFd() const
{
    return _self->maxFd;
}

int SelectRead::wait( double sec )
{
    fd_set * pReadFds = NULL;

    if ( _self->readFdsCount > 0 ) pReadFds = &_self->readFds;

    if ( sec < 0 )
    {
        return ::select( _self->maxFd + 1, pReadFds, NULL, NULL, NULL );
    }
    else
    {
        long intsec = (long)sec;
        long microsec = (long)( ( sec - (double)intsec ) * 1e6 );
        struct timeval tv = { intsec, microsec };
        return ::select( _self->maxFd + 1, pReadFds, NULL, NULL, &tv );
    }
}

// struct SelectWrite_Data --------------------------------------------------------------------
struct SelectWrite_Data
{
    int writeFdsCount;
    int maxFd;
    fd_set writeFds;

    SelectWrite_Data() { this->zeroInit(); }

    void zeroInit()
    {
        FD_ZERO(&this->writeFds);
        this->writeFdsCount = 0;

        this->maxFd = -2;
    }
};

// class SelectWrite --------------------------------------------------------------------------
SelectWrite::SelectWrite()
{
}

SelectWrite::SelectWrite( Socket const & sock )
{
    this->setWriteSock(sock);
}

SelectWrite::SelectWrite( int fd )
{
    this->setWriteFd(fd);
}

SelectWrite::SelectWrite( winux::Mixed const & fds )
{
    this->setWriteFds(fds);
}

SelectWrite::~SelectWrite()
{
}

bool SelectWrite::setWriteFd( int fd )
{
    if ( Select::ValidFd(fd) )
    {
        FD_SET( fd, &_self->writeFds );
        if ( fd > _self->maxFd ) _self->maxFd = fd;
        _self->writeFdsCount++;
        return true;
    }
    return false;
}

bool SelectWrite::delWriteFd( int fd )
{
    if ( Select::ValidFd(fd) )
    {
        FD_CLR( fd, &_self->writeFds );
        _self->writeFdsCount--;
        return true;
    }
    return false;
}

bool SelectWrite::setWriteFds( winux::Mixed const & fds )
{
    bool b = true;
    if ( fds.isArray() )
    {
        size_t n = fds.getCount();
        for ( size_t i = 0; i < n; i++ )
        {
            if ( !this->setWriteFd( fds[i].toInt() ) )
                b = false;
        }
    }
    else
    {
        b = this->setWriteFd( fds.toInt() );
    }

    return b;
}

void SelectWrite::clear()
{
    FD_ZERO(&_self->writeFds);
    _self->maxFd = -2;
    _self->writeFdsCount = 0;
}

int SelectWrite::hasWriteFd( int fd ) const
{
    return FD_ISSET( fd, &_self->writeFds );
}

int SelectWrite::getWriteFdsCount() const
{
    return _self->writeFdsCount;
}

int SelectWrite::getWriteMaxFd() const
{
    return _self->maxFd;
}

int SelectWrite::wait( double sec )
{
    fd_set * pWriteFds = NULL;

    if ( _self->writeFdsCount > 0 ) pWriteFds = &_self->writeFds;

    if ( sec < 0 )
    {
        return ::select( _self->maxFd + 1, NULL, pWriteFds, NULL, NULL );
    }
    else
    {
        long intsec = (long)sec;
        long microsec = (long)( ( sec - (double)intsec ) * 1e6 );
        struct timeval tv = { intsec, microsec };
        return ::select( _self->maxFd + 1, NULL, pWriteFds, NULL, &tv );
    }
}

// struct SelectExcept_Data -------------------------------------------------------------------
struct SelectExcept_Data
{
    int exceptFdsCount;
    int maxFd;
    fd_set exceptFds;

    SelectExcept_Data() { this->zeroInit(); }

    void zeroInit()
    {
        FD_ZERO(&this->exceptFds);
        this->exceptFdsCount = 0;

        this->maxFd = -2;
    }
};

// class SelectExcept -------------------------------------------------------------------------
SelectExcept::SelectExcept()
{
}

SelectExcept::SelectExcept( Socket const & sock )
{
    this->setExceptSock(sock);
}

SelectExcept::SelectExcept( int fd )
{
    this->setExceptFd(fd);
}

SelectExcept::SelectExcept( winux::Mixed const & fds )
{
    this->setExceptFds(fds);
}

SelectExcept::~SelectExcept()
{
}

bool SelectExcept::setExceptFd( int fd )
{
    if ( Select::ValidFd(fd) )
    {
        FD_SET( fd, &_self->exceptFds );
        if ( fd > _self->maxFd ) _self->maxFd = fd;
        _self->exceptFdsCount++;
        return true;
    }
    return false;
}

bool SelectExcept::delExceptFd( int fd )
{
    if ( Select::ValidFd(fd) )
    {
        FD_CLR( fd, &_self->exceptFds );
        _self->exceptFdsCount--;
        return true;
    }
    return false;
}

bool SelectExcept::setExceptFds( winux::Mixed const & fds )
{
    bool b = true;
    if ( fds.isArray() )
    {
        size_t n = fds.getCount();
        for ( size_t i = 0; i < n; i++ )
        {
            if ( !this->setExceptFd( fds[i].toInt() ) )
                b = false;
        }
    }
    else
    {
        b = this->setExceptFd( fds.toInt() );
    }

    return b;
}

void SelectExcept::clear()
{
    FD_ZERO(&_self->exceptFds);
    _self->maxFd = -2;
    _self->exceptFdsCount = 0;
}

int SelectExcept::hasExceptFd( int fd ) const
{
    return FD_ISSET( fd, &_self->exceptFds );
}

int SelectExcept::getExceptFdsCount() const
{
    return _self->exceptFdsCount;
}

int SelectExcept::getExceptMaxFd() const
{
    return _self->maxFd;
}

int SelectExcept::wait( double sec )
{
    fd_set * pExceptFds = NULL;

    if ( _self->exceptFdsCount > 0 ) pExceptFds = &_self->exceptFds;

    if ( sec < 0 )
    {
        return ::select( _self->maxFd + 1, NULL, NULL, pExceptFds, NULL );
    }
    else
    {
        long intsec = (long)sec;
        long microsec = (long)( ( sec - (double)intsec ) * 1e6 );
        struct timeval tv = { intsec, microsec };
        return ::select( _self->maxFd + 1, NULL, NULL, pExceptFds, &tv );
    }
}

// class Select -------------------------------------------------------------------------------
bool Select::ValidFd( int fd )
{
#if defined(OS_WIN)
    return fd > -1;
#else
    return fd > -1 && fd < FD_SETSIZE;
#endif
}

int Select::wait( double sec )
{
    fd_set * pReadFds = NULL;
    fd_set * pWriteFds = NULL;
    fd_set * pExceptFds = NULL;

    if ( SelectRead::_self->readFdsCount > 0 ) pReadFds = &SelectRead::_self->readFds;
    if ( SelectWrite::_self->writeFdsCount > 0 ) pWriteFds = &SelectWrite::_self->writeFds;
    if ( SelectExcept::_self->exceptFdsCount > 0 ) pExceptFds = &SelectExcept::_self->exceptFds;

    int maxFd;
    maxFd = ( SelectRead::_self->maxFd > SelectWrite::_self->maxFd ? SelectRead::_self->maxFd : SelectWrite::_self->maxFd );
    maxFd = ( SelectExcept::_self->maxFd > maxFd ? SelectExcept::_self->maxFd : maxFd );

    if ( sec < 0 )
    {
        return ::select( maxFd + 1, pReadFds, pWriteFds, pExceptFds, NULL );
    }
    else
    {
        long intsec = (long)sec;
        long microsec = (long)( ( sec - (double)intsec ) * 1e6 );
        struct timeval tv = { intsec, microsec };
        return ::select( maxFd + 1, pReadFds, pWriteFds, pExceptFds, &tv );
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////
namespace select
{
// IoSocketCtx 超时处理
void _IoSocketCtxTimeoutCallback( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx, IoEventsData * ioEvents )
{
    auto assocCtx = timerCtx->assocCtx;
    if ( assocCtx )
    {
        assocCtx->timerCtx.reset(); // 取消关联的超时timer场景
        assocCtx->cancel(cancelTimeout); // 超时取消操作
        //winux::ColorOutputLine( winux::fgFuchsia, "timeout ", timerCtx->timeoutMs, " cancel io-type:", assocCtx->type );
        //ioEvents->wakeUpTrigger(IoEventsData::wutWantUpdate); // 唤醒以更新事件IO映射表
    }
}

// IoSocketCtx 取消超时事件关联，并取消超时定时器IO
void _IoSocketCtxResetTimerCtx( winux::SharedPointer<io::IoSocketCtx> ctx )
{
    if ( ctx->timerCtx ) // 有超时处理
    {
        auto timerCtx = ctx->timerCtx.lock();
        if ( timerCtx )
        {
            auto timer = timerCtx->timer; // 定时器对象
            if ( timerCtx->assocCtx )
            {
                timerCtx->assocCtx.reset();
            }
            timer->stop();
            ctx->timerCtx.reset();
        }
    }
}

// 工作线程函数
void _WorkerThreadFunc( IoService * serv, IoServiceThread * thread, IoEventsData * ioEvents, bool * stop )
{
    io::Select sel;
    *stop = false;
    while ( !*stop )
    {
        sel.clear();

        // 处理预投递的IoCtxs
        ioEvents->_handleIoEventsPost();

        // 事件加入select监听
        ioEvents->_handleIoEventsListen(sel);

        // 等待事件就绪
        int rc = sel.wait();

        if ( rc < 0 )
        {
            //size_t index = -1;
            //for ( size_t i = 0; i < serv->getGroupThreadCount(); i++ )
            //{
            //    if ( thread == serv->getGroupThread(i) )
            //    {
            //        index = i;
            //    }
            //}
            //winux::ColorOutputLine( winux::fgRed, "select err:", Socket::ErrNo(), ", thread:", index );
        }
        else // rc >= 0
        {
            // 处理IO事件
            ioEvents->_handleIoEventsCallback( sel, rc );
        }
        
        // 处理超时响应，并删除取消的IO
        ioEvents->_handleIoEventsTimeoutAndDeleteCancel();
    }
}

void IoEventsData::_handleIoEventsPost()
{
    winux::ScopeGuard guard(this->_mtxPreIoCtxs);
    for ( auto it = this->_preIoCtxs.begin(); it != this->_preIoCtxs.end(); it++ )
    {
        this->post( it->first, it->second );
    }
    this->_preIoCtxs.clear();
}

void IoEventsData::_handleIoEventsListen( io::Select & sel )
{
    winux::ScopeGuard guard(this->_mtxIoMaps);

    // 监听唤醒select.wait事件
    sel.setReadFd( this->_sockWakeUp.get() );

    // 统计
    size_t sockIoCount = 0, timerIoCount = 0;

    bool hasEraseInIoMaps = false;
    // 监听IO事件
    for ( auto itMaps = this->_ioMaps.begin(); itMaps != this->_ioMaps.end(); hasEraseInIoMaps = false )
    {
        auto obj = itMaps->first;
        auto & ioMap = itMaps->second;

        // 监听IO请求
        bool hasEraseInIoMap = false;
        for ( auto it = ioMap.begin(); it != ioMap.end(); hasEraseInIoMap = false )
        {
            auto ioType = it->first;
            auto ioCtx = it->second;

            if ( ioCtx->cancelType == cancelNone )
            {
                if ( obj.type == IoEventsData::aotTimer ) // aotTimer
                {
                    auto timerCtx = ioCtx.cast<IoTimerCtx>();
                    if ( timerCtx->assocCtx ) // 有关联IO，是超时定时器
                    {
                        if ( io::Select::ValidFd( timerCtx->assocCtx->sock->get() ) ) // 关联IO的sock是否有效
                        {
                            auto fd =
                            #if defined(OS_WIN)
                                timerCtx->_sockSignal.get();
                        #else
                                timerCtx->timer->get();
                        #endif
                            if ( sel.setReadFd(fd) )
                            {
                                timerIoCount++;
                            }
                            else
                            {
                                timerCtx->cancel(cancelProactive);
                                timerCtx->assocCtx->cancel(cancelProactive);
                            }
                        }
                        else // sockfd 无效
                        {
                            timerCtx->cancel(cancelProactive);
                            timerCtx->assocCtx->cancel(cancelProactive);
                        }
                    }
                    else // 普通定时器
                    {
                        auto fd =
                        #if defined(OS_WIN)
                            timerCtx->_sockSignal.get();
                    #else
                            timerCtx->timer->get();
                    #endif
                        if ( sel.setReadFd(fd) )
                        {
                            timerIoCount++;
                        }
                        else
                        {
                            timerCtx->cancel(cancelProactive);
                        }
                    }
                }
                else // aotSocket
                {
                    auto sock = reinterpret_cast<eiennet::async::Socket *>(obj.ptr)->sharedFromThis();
                    auto sockIoCtx = ioCtx.cast<IoSocketCtx>();
                    if ( io::Select::ValidFd( sock->get() ) )
                    {
                        // 监听错误
                        sel.setExceptFd( sock->get() );

                        // 监听socket IO
                        switch ( ioType )
                        {
                        case ioAccept:
                            sel.setReadFd( sock->get() );
                            break;
                        case ioConnect:
                            sel.setWriteFd( sock->get() );
                            break;
                        case ioRecv:
                            sel.setReadFd( sock->get() );
                            break;
                        case ioSend:
                            sel.setWriteFd( sock->get() );
                            break;
                        case ioRecvFrom:
                            sel.setReadFd( sock->get() );
                            break;
                        case ioSendTo:
                            sel.setWriteFd( sock->get() );
                            break;
                        }

                        sockIoCount++;
                    }
                    else // sockfd 无效
                    {
                        ioCtx->cancel(cancelProactive);
                        if ( sockIoCtx->timerCtx )
                        {
                            auto timerCtx = sockIoCtx->timerCtx.lock();
                            if ( timerCtx )
                            {
                                timerCtx->cancel(cancelProactive);
                            }
                        }
                    }
                }
            }

            // 如果已经是end则不能再++it
            if ( !hasEraseInIoMap && it != ioMap.end() ) ++it;
        } // for ( auto it = ioMap.begin(); it != ioMap.end(); hasEraseInIoMap = false )

          // 如果IO映射表已空，则删除该异步对象
        if ( ioMap.empty() )
        {
            itMaps = this->_ioMaps.erase(itMaps);
            hasEraseInIoMaps = true;
        }

        // 如果已经是end则不能再++it
        if ( !hasEraseInIoMaps && itMaps != this->_ioMaps.end() ) ++itMaps;
    }

    this->_sockIoCount = sockIoCount;
    this->_timerIoCount = timerIoCount;
}

void IoEventsData::_handleIoEventsCallback( io::Select & sel, int rc )
{
    winux::ScopeGuard guard(this->_mtxIoMaps);

    if ( rc > 0 )
    {
        // 处理唤醒select.wait事件
        if ( sel.hasReadFd( this->_sockWakeUp.get() ) )
        {
            eiennet::ip::EndPoint ep;
            auto data = this->_sockWakeUp.recvFrom( &ep, sizeof(winux::ushort) * 16 );
            //ColorOutputLine( winux::fgFuchsia, "wake up:", data.size(), ", ioMaps:", ioEvents->_ioMaps.size(), ", thread:", thread );
            rc--;
        }
    }

    // 处理IO事件
    bool hasEraseInIoMaps = false;
    for ( auto itMaps = this->_ioMaps.begin(); itMaps != this->_ioMaps.end(); hasEraseInIoMaps = false )
    {
        auto obj = itMaps->first; // 异步对象地址及类别
        auto & ioMap = itMaps->second;

        if ( obj.type == IoEventsData::aotSocket && ioMap.size() > 0 && rc > 0 )
        {
            // Socket出错处理
            auto sock = reinterpret_cast<eiennet::async::Socket *>(obj.ptr)->sharedFromThis();
            if ( sel.hasExceptFd( sock->get() ) )
            {
                // 调用sock错误处理
                {
                    winux::ScopeUnguard unguard(this->_mtxIoMaps);
                    sock->onError(sock);
                }

                // 删除该sock的所有IO事件
                itMaps = this->_ioMaps.erase(itMaps);
                hasEraseInIoMaps = true;
                // 就绪数-1
                rc--;
                continue;
            }
        }

        // 处理该异步对象的IO事件
        bool hasEraseInIoMap = false;
        for ( auto it = ioMap.begin(); it != ioMap.end(); hasEraseInIoMap = false )
        {
            auto ioType = it->first;
            auto ioCtx = it->second;
            auto timeDiff = winux::GetUtcTimeMs() - ioCtx->startTime;

            if ( rc > 0 )
            {
                if ( ioCtx->cancelType == cancelNone ) // 未被取消
                {
                    if ( obj.type == IoEventsData::aotTimer ) // aotTimer
                    {
                        auto timerCtx = ioCtx.cast<IoTimerCtx>();
                        auto timer = timerCtx->timer; // 定时器对象
                    #if defined(OS_WIN)
                        auto timerFd = timerCtx->_sockSignal.get();
                    #else
                        auto timerFd = timer->get();
                    #endif
                        if ( sel.hasReadFd(timerFd) )
                        {
                        #if defined(OS_WIN)
                            eiennet::ip::EndPoint ep;
                            timerCtx->_sockSignal.recvFrom( &ep, sizeof(winux::uint64) );
                        #else
                            uint64_t cnt = 0;
                            read( timerFd, &cnt, sizeof(uint64_t) );
                        #endif

                            if ( timerCtx->cbOk )
                            {
                                // 调用回调函数
                                winux::ScopeUnguard unguard(this->_mtxIoMaps);
                                timerCtx->cbOk( timer, timerCtx.get() );
                            }

                            {
                                winux::ScopeGuard guard( timer->getMutex() );
                                if ( timerCtx->periodic == false ) // 非周期
                                {
                                    {
                                        winux::ScopeUnguard unguard( timer->getMutex() );
                                        timer->unset();
                                    }
                                    timer->_timerCtx.reset();
                                    //timerCtx->decRef();

                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
                                    hasEraseInIoMap = true;
                                }
                                else
                                {
                                    timer->_posted = false;
                                }
                            }

                            // 就绪数-1
                            rc--;
                        }
                    }
                    else // aotSocket
                    {
                        auto sockIoCtx = ioCtx.cast<IoSocketCtx>();
                        auto sock = sockIoCtx->sock; // 套接字对象
                        switch ( ioType )
                        {
                        case ioAccept:
                            if ( sel.hasReadFd( sock->get() ) )
                            {
                                auto ctx = sockIoCtx.ensureCast<IoAcceptCtx>();

                                _IoSocketCtxResetTimerCtx(ctx);

                                // 已处理，删除这个请求
                                it = ioMap.erase(it);
                                hasEraseInIoMap = true;

                                // 接受客户连接
                                auto clientSock = sock->accept(&ctx->clientEp);
                                // 处理回调
                                if ( ctx->cbOk )
                                {
                                    winux::ScopeUnguard unguard(this->_mtxIoMaps);
                                    if ( ctx->cbOk( sock, clientSock, ctx->clientEp ) )
                                    {
                                        ctx->sock->acceptAsync( ctx->cbOk, ctx->timeoutMs, ctx->cbTimeout, ctx->sock->getThread() );
                                    }
                                }

                                // 就绪数-1
                                rc--;
                            }
                            break;
                        case ioConnect:
                            if ( sel.hasWriteFd( sock->get() ) )
                            {
                                auto ctx = sockIoCtx.ensureCast<IoConnectCtx>();

                                _IoSocketCtxResetTimerCtx(ctx);

                                // 已处理，删除这个请求
                                it = ioMap.erase(it);
                                hasEraseInIoMap = true;

                                ctx->costTimeMs = timeDiff;
                                // 处理回调
                                if ( ctx->cbOk )
                                {
                                    winux::ScopeUnguard unguard(this->_mtxIoMaps);
                                    ctx->cbOk( sock, ctx->costTimeMs );
                                }

                                // 就绪数-1
                                rc--;
                            }
                            break;
                        case ioRecv:
                            if ( sel.hasReadFd( sock->get() ) )
                            {
                                auto ctx = sockIoCtx.ensureCast<IoRecvCtx>();

                                _IoSocketCtxResetTimerCtx(ctx);

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
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
                                    hasEraseInIoMap = true;

                                    // 处理回调
                                    if ( ctx->cbOk )
                                    {
                                        winux::ScopeUnguard unguard(this->_mtxIoMaps);
                                        ctx->cbOk( sock, data, ctx->cnnAvail );
                                    }
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
                            if ( sel.hasWriteFd( sock->get() ) )
                            {
                                auto ctx = sockIoCtx.ensureCast<IoSendCtx>();

                                _IoSocketCtxResetTimerCtx(ctx);

                                ctx->cnnAvail = true;
                                ctx->costTimeMs += timeDiff;

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
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
                                    hasEraseInIoMap = true;

                                    // 处理回调
                                    if ( ctx->cbOk )
                                    {
                                        winux::ScopeUnguard unguard(this->_mtxIoMaps);
                                        ctx->cbOk( sock, ctx->hadBytes, ctx->costTimeMs, ctx->cnnAvail );
                                    }
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
                            if ( sel.hasReadFd( sock->get() ) )
                            {
                                auto ctx = sockIoCtx.ensureCast<IoRecvFromCtx>();

                                _IoSocketCtxResetTimerCtx(ctx);

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
                                if ( data ) ctx->data.append( data );
                                ctx->hadBytes += data.size();

                                if ( ctx->hadBytes >= ctx->targetBytes || data.size() == 0 )
                                {
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
                                    hasEraseInIoMap = true;

                                    // 处理回调
                                    if ( ctx->cbOk )
                                    {
                                        winux::ScopeUnguard unguard(this->_mtxIoMaps);
                                        ctx->cbOk( sock, ctx->data, ctx->epFrom );
                                    }
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
                            if ( sel.hasWriteFd( sock->get() ) )
                            {
                                auto ctx = sockIoCtx.ensureCast<IoSendToCtx>();

                                _IoSocketCtxResetTimerCtx(ctx);

                                bool fail = false;
                                ctx->costTimeMs += timeDiff;

                                if ( ctx->hadBytes < ctx->data.size() )
                                {
                                    size_t wantBytes = ctx->data.size() - ctx->hadBytes;
                                    int sendBytes = sock->sendTo( *ctx->epTo.get(), ctx->data.get<winux::byte>() + ctx->hadBytes, wantBytes );
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
                                    // 已处理，删除这个请求
                                    it = ioMap.erase(it);
                                    hasEraseInIoMap = true;

                                    // 处理回调
                                    if ( ctx->cbOk )
                                    {
                                        winux::ScopeUnguard unguard(this->_mtxIoMaps);
                                        ctx->cbOk( sock, ctx->hadBytes, ctx->costTimeMs );
                                    }
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
                } // if ( ioCtx->cancelType == cancelNone )
            }
            else // rc <= 0
            {
                break;
            }

            // 如果已经是`end`则不能再`++it`
            if ( !hasEraseInIoMap && it != ioMap.end() ) ++it;
        } // for ( auto it = ioMap.begin(); it != ioMap.end(); hasEraseInIoMap = false )

        // 如果IO映射表已空，则删除该异步对象
        if ( ioMap.empty() )
        {
            itMaps = this->_ioMaps.erase(itMaps);
            hasEraseInIoMaps = true;
        }

        // 如果已经是`end`则不能再`++it`
        if ( !hasEraseInIoMaps && itMaps != this->_ioMaps.end() ) ++itMaps;
    }
}

void  IoEventsData::_handleIoEventsTimeoutAndDeleteCancel()
{
    winux::ScopeGuard guard(this->_mtxIoMaps);

    bool hasEraseInIoMaps = false;
    // 枚举IO对象
    for ( auto itMaps = this->_ioMaps.begin(); itMaps != this->_ioMaps.end(); hasEraseInIoMaps = false )
    {
        auto obj = itMaps->first;
        auto & ioMap = itMaps->second;

        // 枚举IO请求
        bool hasEraseInIoMap = false;
        for ( auto it = ioMap.begin(); it != ioMap.end(); hasEraseInIoMap = false )
        {
            auto ioType = it->first;
            auto ioCtx = it->second;

            if ( ioCtx->cancelType != cancelNone ) // 已取消
            {
                it = ioMap.erase(it); // 删除已取消的IO事件
                hasEraseInIoMap = true;

                if ( obj.type == IoEventsData::aotSocket ) // aotSocket
                {
                    if ( ioCtx->cancelType == cancelTimeout ) // 超时取消，处理超时响应
                    {
                        auto sockIoCtx = ioCtx.cast<IoSocketCtx>();
                        switch ( ioType )
                        {
                        case ioAccept:
                            {
                                auto ctx = sockIoCtx.ensureCast<IoAcceptCtx>();
                                if ( ctx->cbTimeout )
                                {
                                    winux::ScopeUnguard unguard(this->_mtxIoMaps);
                                    if ( ctx->cbTimeout( ctx->sock, ctx.get() ) )
                                    {
                                        ctx->sock->acceptAsync( ctx->cbOk, ctx->timeoutMs, ctx->cbTimeout, ctx->sock->getThread() );
                                    }
                                }
                                else
                                {
                                    winux::ScopeUnguard unguard(this->_mtxIoMaps);
                                    ctx->sock->acceptAsync( ctx->cbOk, ctx->timeoutMs, ctx->cbTimeout, ctx->sock->getThread() );
                                }
                            }
                            break;
                        case ioConnect:
                            {
                                auto ctx = sockIoCtx.ensureCast<IoConnectCtx>();
                                if ( ctx->cbTimeout )
                                {
                                    winux::ScopeUnguard unguard(this->_mtxIoMaps);
                                    ctx->cbTimeout( ctx->sock, ctx.get() );
                                }
                            }
                            break;
                        case ioRecv:
                            {
                                auto ctx = sockIoCtx.ensureCast<IoRecvCtx>();
                                if ( ctx->cbTimeout )
                                {
                                    winux::ScopeUnguard unguard(this->_mtxIoMaps);
                                    ctx->cbTimeout( ctx->sock, ctx.get() );
                                }
                            }
                            break;
                        case ioSend:
                            {
                                auto ctx = sockIoCtx.ensureCast<IoSendCtx>();
                                if ( ctx->cbTimeout )
                                {
                                    winux::ScopeUnguard unguard(this->_mtxIoMaps);
                                    ctx->cbTimeout( ctx->sock, ctx.get() );
                                }
                            }
                            break;
                        case ioRecvFrom:
                            {
                                auto ctx = sockIoCtx.ensureCast<IoRecvFromCtx>();
                                if ( ctx->cbTimeout )
                                {
                                    winux::ScopeUnguard unguard(this->_mtxIoMaps);
                                    ctx->cbTimeout( ctx->sock, ctx.get() );
                                }
                            }
                            break;
                        case ioSendTo:
                            {
                                auto ctx = sockIoCtx.ensureCast<IoSendToCtx>();
                                if ( ctx->cbTimeout )
                                {
                                    winux::ScopeUnguard unguard(this->_mtxIoMaps);
                                    ctx->cbTimeout( ctx->sock, ctx.get() );
                                }
                            }
                            break;
                        }
                    }
                }
            }

            // 如果已经是end则不能再++it
            if ( !hasEraseInIoMap && it != ioMap.end() ) ++it;
        } // for ( auto it = ioMap.begin(); it != ioMap.end(); hasEraseInIoMap = false )

          // 如果IO映射表已空，则删除该异步对象
        if ( ioMap.empty() )
        {
            itMaps = this->_ioMaps.erase(itMaps);
            hasEraseInIoMaps = true;
        }

        // 如果已经是end则不能再++it
        if ( !hasEraseInIoMaps && itMaps != this->_ioMaps.end() ) ++itMaps;
    }
}

// struct IoAcceptCtx -------------------------------------------------------------------------
IoAcceptCtx::IoAcceptCtx()
{

}

IoAcceptCtx::~IoAcceptCtx()
{

}

// struct IoConnectCtx ------------------------------------------------------------------------
IoConnectCtx::IoConnectCtx()
{

}

IoConnectCtx::~IoConnectCtx()
{

}

// struct IoRecvCtx ---------------------------------------------------------------------------
IoRecvCtx::IoRecvCtx()
{

}

IoRecvCtx::~IoRecvCtx()
{

}

// struct IoSendCtx ---------------------------------------------------------------------------
IoSendCtx::IoSendCtx()
{

}

IoSendCtx::~IoSendCtx()
{

}

// struct IoRecvFromCtx -----------------------------------------------------------------------
IoRecvFromCtx::IoRecvFromCtx()
{

}

IoRecvFromCtx::~IoRecvFromCtx()
{

}

// struct IoSendToCtx -------------------------------------------------------------------------
IoSendToCtx::IoSendToCtx()
{

}

IoSendToCtx::~IoSendToCtx()
{

}

// struct IoTimerCtx --------------------------------------------------------------------------
IoTimerCtx::IoTimerCtx()
{
#if defined(OS_WIN)
    _portSockSignal = 0;
    if ( _sockSignal.bind( eiennet::ip::EndPoint( "", 0 ) ) )
    {
        eiennet::ip::EndPoint ep;
        _sockSignal.getBoundEp(&ep);
        _portSockSignal = ep.getPort();
    }
#else

#endif
}

IoTimerCtx::~IoTimerCtx()
{

}

bool IoTimerCtx::cancel( CancelType cancelType )
{
    this->cancelType = cancelType;

    if ( this->timer )
    {
        this->timer->unset();
        return true;
    }
    return false;
}

// class IoEventsData -------------------------------------------------------------------------
IoEventsData::IoEventsData() : _mtxPreIoCtxs(true), _mtxIoMaps(true), _portSockWakeUp(0), _sockIoCount(0), _timerIoCount(0)
{
    if ( _sockWakeUp.bind( eiennet::ip::EndPoint( "", 0 ) ) )
    {
        eiennet::ip::EndPoint ep;
        _sockWakeUp.getBoundEp(&ep);
        _portSockWakeUp = ep.getPort();
    }
}

void IoEventsData::wakeUpTrigger( WakeUpType type )
{
    winux::ushort t = (winux::ushort)type;
    _sockWakeUp.sendTo( eiennet::ip::EndPoint( "127.0.0.1", _portSockWakeUp ), &t, sizeof(t) );
}

void IoEventsData::prePost( IoType type, winux::SharedPointer<IoCtx> ctx )
{
    winux::ScopeGuard guard(_mtxPreIoCtxs);
    this->_preIoCtxs.push_back( std::make_pair( type, ctx ) );
}

void IoEventsData::post( IoType type, winux::SharedPointer<IoCtx> ctx )
{
    winux::ScopeGuard guard(_mtxIoMaps);
    switch ( type )
    {
    case ioNone:
        break;
    case ioAccept:
    case ioConnect:
    case ioRecv:
    case ioSend:
    case ioRecvFrom:
    case ioSendTo:
        {
            auto sockIoCtx = ctx.cast<IoSocketCtx>();

            IoKey key( sockIoCtx->sock.get(), aotSocket );

            auto itMaps = this->_ioMaps.find(key);
            if ( itMaps != this->_ioMaps.end() ) // 已存在此socket
            {
                auto & ioMap = itMaps->second;
                auto it = ioMap.find(type);
                if ( it != ioMap.end() ) // 已存在此IoCtx
                {
                    auto existingCtx = it->second.cast<IoSocketCtx>();
                    if ( existingCtx->timerCtx )
                    {
                        auto timerCtx = existingCtx->timerCtx.lock();
                        if ( timerCtx )
                        {
                            auto timer = timerCtx->timer;

                            // 删除关联的超时timer
                            //this->_ioMaps.erase( IoKey( timer.get(), aotTimer ) );

                            timer->stop();
                        }
                    }
                }

                ioMap[type] = ctx;
            }
            else
            {
                this->_ioMaps[key][type] = ctx;
            }
        }
        break;
    case ioTimer:
        {
            auto timerCtx = ctx.cast<IoTimerCtx>();

            IoKey key( timerCtx->timer.get(), aotTimer );

            auto itMaps = this->_ioMaps.find(key);
            if ( itMaps != this->_ioMaps.end() ) // 已存在此timer
            {
                auto timer = timerCtx->timer;
                timer->stop();
                // 删除已存在的IoTimerCtx
                //this->_ioMaps.erase(itMaps);
            }

            this->_ioMaps[key][type] = timerCtx;
        }
        break;
    }
}

// class IoServiceThread ----------------------------------------------------------------------
void IoServiceThread::run()
{
    _WorkerThreadFunc( this->_serv, this, &this->_ioEvents, &this->_stop );
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
IoService::IoService( size_t groupThread ) : _stop(false)
{
    // 创建工作线程组
    this->_group.create<IoServiceThread>( groupThread, this );
    //this->_pool.startup(poolThread);
}

void IoService::stop()
{
    this->_stop = true;
    this->_ioEvents.wakeUpTrigger(IoEventsData::wutWantStop);
    for ( size_t i = 0; i < _group.count(); i++ )
    {
        // 给每个线程投递退出信号
        auto * th = this->getGroupThread(i);
        th->_stop = true;
        th->_ioEvents.wakeUpTrigger(IoEventsData::wutWantStop);
    }
}

int IoService::run()
{
    this->_group.startup();
    _WorkerThreadFunc( this, nullptr, &this->_ioEvents, &this->_stop );
    this->_group.wait();
    //this->_pool.whenEmptyStopAndWait();
    return 0;
}

void IoService::postAccept( winux::SharedPointer<eiennet::async::Socket> sock, IoAcceptCtx::OkFn cbOk, winux::uint64 timeoutMs, IoAcceptCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    if ( !this->associate( sock, th ) ) return;

    auto ctx = winux::MakeShared( IoAcceptCtx::New(), [] ( IoAcceptCtx * ctx ) { ctx->decRef(); } );
    ctx->timeoutMs = timeoutMs;
    ctx->sock = sock;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;

    IoEventsData * ioEvents = sock->getThread() ? &((IoServiceThread *)sock->getThread())->_ioEvents : &this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        this->_postTimerEx( eiennet::async::Timer::New(*this), ctx->timeoutMs, false, [ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents->prePost( ioAccept, ctx );

    // 唤醒更新
    ioEvents->wakeUpTrigger(IoEventsData::wutWantUpdate);
}

void IoService::postConnect( winux::SharedPointer<eiennet::async::Socket> sock, eiennet::EndPoint const & ep, IoConnectCtx::OkFn cbOk, winux::uint64 timeoutMs, IoConnectCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    if ( !this->associate( sock, th ) ) return;

    sock->connect(ep);

    auto ctx = winux::MakeShared( IoConnectCtx::New(), [] ( IoConnectCtx * ctx ) { ctx->decRef(); } );
    ctx->timeoutMs = timeoutMs;
    ctx->sock = sock;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;

    IoEventsData * ioEvents = sock->getThread() ? &((IoServiceThread *)sock->getThread())->_ioEvents : &this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        this->_postTimerEx( eiennet::async::Timer::New(*this), ctx->timeoutMs, false, [ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents->prePost( ioConnect, ctx );

    // 唤醒更新
    ioEvents->wakeUpTrigger(IoEventsData::wutWantUpdate);
}

void IoService::postRecv( winux::SharedPointer<eiennet::async::Socket> sock, size_t targetSize, IoRecvCtx::OkFn cbOk, winux::uint64 timeoutMs, IoRecvCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    if ( !this->associate( sock, th ) ) return;

    auto ctx = winux::MakeShared( IoRecvCtx::New(), [] ( IoRecvCtx * ctx ) { ctx->decRef(); } );
    ctx->timeoutMs = timeoutMs;
    ctx->sock = sock;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    ctx->targetBytes = targetSize;

    IoEventsData * ioEvents = sock->getThread() ? &((IoServiceThread *)sock->getThread())->_ioEvents : &this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        this->_postTimerEx( eiennet::async::Timer::New(*this), ctx->timeoutMs, false, [ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents->prePost( ioRecv, ctx );

    // 唤醒更新
    ioEvents->wakeUpTrigger(IoEventsData::wutWantUpdate);
}

void IoService::postSend( winux::SharedPointer<eiennet::async::Socket> sock, void const * data, size_t size, IoSendCtx::OkFn cbOk, winux::uint64 timeoutMs, IoSendCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    if ( !this->associate( sock, th ) ) return;

    auto ctx = winux::MakeShared( IoSendCtx::New(), [] ( IoSendCtx * ctx ) { ctx->decRef(); } );
    ctx->timeoutMs = timeoutMs;
    ctx->sock = sock;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    ctx->data.setBuf( data, size, false );

    IoEventsData * ioEvents = sock->getThread() ? &((IoServiceThread *)sock->getThread())->_ioEvents : &this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        this->_postTimerEx( eiennet::async::Timer::New(*this), ctx->timeoutMs, false, [ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents->prePost( ioSend, ctx );

    // 唤醒更新
    ioEvents->wakeUpTrigger(IoEventsData::wutWantUpdate);
}

void IoService::postRecvFrom( winux::SharedPointer<eiennet::async::Socket> sock, size_t targetSize, IoRecvFromCtx::OkFn cbOk, winux::uint64 timeoutMs, IoRecvFromCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    if ( !this->associate( sock, th ) ) return;

    auto ctx = winux::MakeShared( IoRecvFromCtx::New(), [] ( IoRecvFromCtx * ctx ) { ctx->decRef(); } );
    ctx->timeoutMs = timeoutMs;
    ctx->sock = sock;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    ctx->targetBytes = targetSize;

    IoEventsData * ioEvents = sock->getThread() ? &((IoServiceThread *)sock->getThread())->_ioEvents : &this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        this->_postTimerEx( eiennet::async::Timer::New(*this), ctx->timeoutMs, false, [ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents->prePost( ioRecvFrom, ctx );

    // 唤醒更新
    ioEvents->wakeUpTrigger(IoEventsData::wutWantUpdate);
}

void IoService::postSendTo( winux::SharedPointer<eiennet::async::Socket> sock, eiennet::EndPoint const & ep, void const * data, size_t size, IoSendToCtx::OkFn cbOk, winux::uint64 timeoutMs, IoSendToCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{
    if ( !this->associate( sock, th ) ) return;

    auto ctx = winux::MakeShared( IoSendToCtx::New(), [] ( IoSendToCtx * ctx ) { ctx->decRef(); } );
    ctx->timeoutMs = timeoutMs;
    ctx->sock = sock;
    ctx->cbOk = cbOk;
    ctx->cbTimeout = cbTimeout;
    ctx->data.setBuf( data, size, false );
    ctx->epTo.attachNew( ep.clone() );

    IoEventsData * ioEvents = sock->getThread() ? &((IoServiceThread *)sock->getThread())->_ioEvents : &this->_ioEvents;

    // 超时处理
    if ( ctx->timeoutMs != -1 )
    {
        this->_postTimerEx( eiennet::async::Timer::New(*this), ctx->timeoutMs, false, [ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
        }, ctx, sock->getThread() );
    }

    // 投递 IO
    ioEvents->prePost( ioSendTo, ctx );

    // 唤醒更新
    ioEvents->wakeUpTrigger(IoEventsData::wutWantUpdate);
}

void IoService::postTimer( winux::SharedPointer<eiennet::async::Timer> timer, winux::uint64 timeoutMs, bool periodic, IoTimerCtx::OkFn cbOk, io::IoServiceThread * th )
{
    this->_postTimerEx( timer, timeoutMs, periodic, cbOk, winux::SharedPointer<IoSocketCtx>(), th );
}

void IoService::_postTimerEx( winux::SharedPointer<eiennet::async::Timer> timer, winux::uint64 timeoutMs, bool periodic, IoTimerCtx::OkFn cbOk, winux::SharedPointer<IoSocketCtx> assocCtx, io::IoServiceThread * th )
{
    winux::SharedPointer<IoTimerCtx> timerCtx;
    {
        winux::ScopeGuard guard( timer->getMutex() );
        timer->_posted = false;
        if ( timer->_timerCtx )
        {
            timerCtx = timer->_timerCtx.lock().ensureCast<IoTimerCtx>();
        }
        else
        {
            timerCtx.attachNew( IoTimerCtx::New(), [] ( IoTimerCtx * ctx ) { ctx->decRef(); } );
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

    IoEventsData * ioEvents = timer->getThread() ? &((IoServiceThread *)timer->getThread())->_ioEvents : &this->_ioEvents;
    // 投递 IO
    ioEvents->prePost( ioTimer, timerCtx );

    timer->set( timeoutMs, periodic );

    // 如果有关联的IoSocketCtx就不必唤醒更新，因为IoSocketCtx投递时会唤醒
    if ( !assocCtx )
    {
        ioEvents->wakeUpTrigger(IoEventsData::wutWantUpdate);
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
    IoEventsData * ioEvents = sock->getThread() ? &static_cast<IoServiceThread *>( sock->getThread() )->_ioEvents : &this->_ioEvents;

    winux::ScopeGuard guard(ioEvents->_mtxIoMaps);

    auto & ioMaps = ioEvents->_ioMaps;
    auto itMaps = ioMaps.find( IoEventsData::IoKey( sock.get(), IoEventsData::aotSocket ) );
    if ( itMaps != ioMaps.end() )
    {
        for ( auto & pr : itMaps->second )
        {
            auto ctx = pr.second.cast<IoSocketCtx>();
            ctx->cancelType = cancelProactive;

            if ( ctx->timerCtx )
            {
                auto timerCtx = ctx->timerCtx.lock();
                if ( timerCtx ) // 如果有超时定时器，停止它
                {
                    auto timer = timerCtx->timer;
                    timer->stop();
                }
            }
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


} // namespace select


} // namespace io
