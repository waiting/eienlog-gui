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
    #include <sys/select.h>
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
#include "eiennet_io_select.hpp"

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
            if ( !this->setReadFd(fds[i]) )
                b = false;
        }
    }
    else
    {
        b = this->setReadFd(fds);
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
            if ( !this->setWriteFd(fds[i]) )
                b = false;
        }
    }
    else
    {
        b = this->setWriteFd(fds);
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
            if ( !this->setExceptFd(fds[i]) )
                b = false;
        }
    }
    else
    {
        b = this->setExceptFd(fds);
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
static void _IoSocketCtxTimeoutCallback( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx, IoEventsData & ioEvents )
{
    auto * assocCtx = timerCtx->assocCtx;
    if ( assocCtx )
    {
        assocCtx->timerCtx = nullptr; // 取消关联的超时timer场景
        assocCtx->changeState(stateTimeoutCancel); // 超时取消操作
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
                sockCtx->changeState(stateProactiveCancel);
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
        fd = dynamic_cast<io::select::IoTimerCtx *>(ioCtx)->_sockSignal.get();
    #else
        fd = dynamic_cast<io::select::IoTimerCtx *>(ioCtx)->timer->get();
    #endif
    }
    else
    {
        fd = dynamic_cast<io::IoSocketCtx *>(ioCtx)->sock->get();
    }
    return fd;
}

// Select工作函数
void _SelectWorkerFunc( IoService * serv, IoServiceThread * thread, IoEventsData & ioEvents, bool * stop )
{
    while ( !*stop )
    {
        ioEvents._sel.clear();

        // 处理预投递的IoCtxs
        ioEvents._handleIoCtxsPost();

        // 事件加入select监听
        ioEvents._handleIoCtxsListen();

        // 等待事件就绪
        int rc = ioEvents._sel.wait();

        if ( rc < 0 )
        {
            if ( errno == EINTR ) continue;

        }
        else // rc >= 0
        {
            // 处理IO事件
            ioEvents._handleIoCtxsCallback(rc);
        }

        // 处理超时响应，并删除不是普通状态的IO
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

void IoEventsData::_handleIoCtxsListen()
{
    // 监听wake up事件
    this->_sel.setReadFd( this->_sockWakeUp.get() );

    // 统计
    size_t sockIoCount = 0, timerIoCount = 0;

    bool hasEraseInIoVecMap = false;
    // 监听IO事件
    for ( auto itVecStruct = this->_ioVecMap.begin(); itVecStruct != this->_ioVecMap.end(); hasEraseInIoVecMap = false )
    {
        auto & ioVecStruct = itVecStruct->second;
        auto & ioVec = ioVecStruct.ctxs;

        if ( ioVec.size() > 0 && ioVec[0]->type != ioTimer )
        {
            auto * sockCtx = dynamic_cast<IoSocketCtx *>(ioVec[0]);
            auto sock = sockCtx->sock;
            if ( io::Select::ValidFd( sock->get() ) )
            {
                // 监听错误
                this->_sel.setExceptFd( sock->get() );
            }
        }

        // 监听IO请求
        bool hasEraseInIoVec = false;
        for ( auto it = ioVec.begin(); it != ioVec.end(); hasEraseInIoVec = false )
        {
            auto * ioCtx = *it;

            if ( ioCtx->state == stateNormal )
            {
                if ( ioCtx->type == ioTimer ) // ioTimer
                {
                    auto * timerCtx = dynamic_cast<IoTimerCtx *>(ioCtx);
                    if ( timerCtx->assocCtx ) // 有关联IO，是超时定时器
                    {
                        if ( io::Select::ValidFd( timerCtx->assocCtx->sock->get() ) ) // 关联IO的sock是否有效
                        {
                            auto timerFd = _GetFdByIoCtx(timerCtx);
                            if ( this->_sel.setReadFd(timerFd) )
                            {
                                timerIoCount++;
                            }
                            else
                            {
                                timerCtx->changeState(stateProactiveCancel);
                                timerCtx->assocCtx->changeState(stateProactiveCancel);
                            }
                        }
                        else // sockfd 无效
                        {
                            timerCtx->changeState(stateProactiveCancel);
                            timerCtx->assocCtx->changeState(stateProactiveCancel);
                        }
                    }
                    else // 普通定时器
                    {
                        auto timerFd = _GetFdByIoCtx(timerCtx);
                        if ( this->_sel.setReadFd(timerFd) )
                        {
                            timerIoCount++;
                        }
                        else
                        {
                            timerCtx->changeState(stateProactiveCancel);
                        }
                    }
                }
                else // IoSocketCtx series
                {
                    auto * sockIoCtx = dynamic_cast<IoSocketCtx *>(ioCtx);
                    auto sock = sockIoCtx->sock;
                    if ( io::Select::ValidFd( sock->get() ) )
                    {
                        // 监听socket IO
                        switch ( sockIoCtx->type )
                        {
                        case ioAccept:
                            this->_sel.setReadFd( sock->get() );
                            break;
                        case ioConnect:
                            this->_sel.setWriteFd( sock->get() );
                            break;
                        case ioRecv:
                            this->_sel.setReadFd( sock->get() );
                            break;
                        case ioSend:
                            this->_sel.setWriteFd( sock->get() );
                            break;
                        case ioRecvFrom:
                            this->_sel.setReadFd( sock->get() );
                            break;
                        case ioSendTo:
                            this->_sel.setWriteFd( sock->get() );
                            break;
                        }

                        sockIoCount++;
                    }
                    else // sockfd 无效
                    {
                        ioCtx->changeState(stateProactiveCancel);
                        if ( sockIoCtx->timerCtx )
                        {
                            sockIoCtx->timerCtx->changeState(stateProactiveCancel);
                        }
                    }
                }
            }

            // 如果已经是end则不能再++it
            if ( !hasEraseInIoVec && it != ioVec.end() ) ++it;
        } // for ( auto it = ioVec.begin(); it != ioVec.end(); hasEraseInIoVec = false )

        // 如果IO映射表已空，则删除该异步对象
        if ( ioVec.empty() )
        {
            itVecStruct = this->_ioVecMap.erase(itVecStruct);
            hasEraseInIoVecMap = true;
        }

        // 如果已经是end则不能再++it
        if ( !hasEraseInIoVecMap && itVecStruct != this->_ioVecMap.end() ) ++itVecStruct;
    }

    this->_sockIoCount = sockIoCount;
    this->_timerIoCount = timerIoCount;
}

void IoEventsData::_handleIoCtxsCallback( int rc )
{
    if ( rc > 0 )
    {
        // 处理唤醒select.wait事件
        if ( this->_sel.hasReadFd( this->_sockWakeUp.get() ) )
        {
            eiennet::ip::EndPoint ep;
            auto data = this->_sockWakeUp.recvFrom( &ep, sizeof(winux::ushort) * 16 );
            //ColorOutputLine( winux::fgFuchsia, "wake up:", data.size(), ", ioMaps:", this->_ioVecMap.size(), ", thread:", thread );
            rc--;
        }
    }

    // 处理IO事件
    bool hasEraseInIoVecMap = false;
    for ( auto itVecStruct = this->_ioVecMap.begin(); itVecStruct != this->_ioVecMap.end(); hasEraseInIoVecMap = false )
    {
        auto & ioVecStruct = itVecStruct->second;
        auto & ioVec = ioVecStruct.ctxs;

        if ( ioVec.size() > 0 && ioVec[0]->type != ioTimer && rc > 0 )
        {
            auto * sockCtx = dynamic_cast<IoSocketCtx *>(ioVec[0]);
            // Socket出错处理
            auto sock = sockCtx->sock;
            if ( this->_sel.hasExceptFd( sock->get() ) )
            {
                // 调用sock错误处理
                sock->onError(sock);

                // 取消该sock的所有IO事件
                _CancelIoCtxs(&ioVecStruct);

                // 就绪数-1
                rc--;
                continue;
            }
        }

        // 处理该异步对象的IO事件
        bool hasEraseInIoVec = false;
        for ( auto it = ioVec.begin(); it != ioVec.end(); hasEraseInIoVec = false )
        {
            auto * ioCtx = *it;

            if ( rc > 0 )
            {
                if ( ioCtx->state == stateNormal ) // 普通状态
                {
                    if ( ioCtx->type == ioTimer ) // Timer的事件处理
                    {
                        auto * timerCtx = dynamic_cast<IoTimerCtx *>(ioCtx);
                        auto timer = timerCtx->timer; // 定时器对象
                        auto timerFd = _GetFdByIoCtx(timerCtx);
                        if ( this->_sel.hasReadFd(timerFd) )
                        {
                            // 读取timer
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

                            // 就绪数-1
                            rc--;
                        }
                    }
                    else // Socket的事件处理
                    {
                        auto * sockIoCtx = dynamic_cast<IoSocketCtx *>(ioCtx);
                        auto sock = sockIoCtx->sock; // 套接字对象
                        switch ( sockIoCtx->type )
                        {
                        case ioAccept:
                            if ( this->_sel.hasReadFd( sock->get() ) )
                            {
                                auto * ctx = static_cast<IoAcceptCtx *>(sockIoCtx);
                                _IoSocketCtxClearTimerCtx(ctx);

                                // 接受客户连接
                                auto clientSock = sock->accept(&ctx->clientEp);
                                // 处理回调
                                if ( ctx->cbOk )
                                {
                                    if ( ctx->cbOk( sock, clientSock, ctx->clientEp ) )
                                    {
                                        ctx->sock->acceptAsync( ctx->cbOk, ctx->timeoutMs, ctx->cbTimeout, ctx->sock->getThread() );
                                    }
                                }
                                else
                                {
                                    ctx->sock->acceptAsync( ctx->cbOk, ctx->timeoutMs, ctx->cbTimeout, ctx->sock->getThread() );
                                }

                                // 已处理，完成这个请求
                                ctx->changeState(stateFinish);

                                // 就绪数-1
                                rc--;
                            }
                            break;
                        case ioConnect:
                            if ( this->_sel.hasWriteFd( sock->get() ) )
                            {
                                auto * ctx = static_cast<IoConnectCtx *>(sockIoCtx);
                                _IoSocketCtxClearTimerCtx(ctx);

                                ctx->costTimeMs = winux::GetUtcTimeMs() - ctx->startTime;
                                // 处理回调
                                if ( ctx->cbOk )
                                {
                                    ctx->cbOk( sock, ctx->costTimeMs );
                                }

                                // 已处理，完成这个请求
                                ctx->changeState(stateFinish);

                                // 就绪数-1
                                rc--;
                            }
                            break;
                        case ioRecv:
                            if ( this->_sel.hasReadFd( sock->get() ) )
                            {
                                auto * ctx = static_cast<IoRecvCtx *>(sockIoCtx);
                                _IoSocketCtxClearTimerCtx(ctx);

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
                                ctx->cnnAvail = data && data.size();
                                if ( ctx->cnnAvail )
                                {
                                    ctx->data.append(data);
                                    ctx->hadBytes += data.size();
                                }

                                if ( ctx->hadBytes >= ctx->targetBytes || data.size() == 0 )
                                {
                                    // 处理回调
                                    if ( ctx->cbOk )
                                    {
                                        ctx->cbOk( sock, ctx->data, ctx->cnnAvail );
                                    }

                                    // 已处理，完成这个请求
                                    ctx->changeState(stateFinish);
                                }
                                else
                                {
                                    IoEventsData & ioEvents = *this;
                                    // 重投这个IO请求和Timer
                                    ctx->startTime = winux::GetUtcTimeMs();
                                    if ( ctx->timeoutMs != -1 )
                                    {
                                        eiennet::async::Timer::New( *sock->getService() )->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
                                        }, ctx, sock->getThread() );
                                    }
                                }

                                // 就绪数-1
                                rc--;
                            }
                            break;
                        case ioSend:
                            if ( this->_sel.hasWriteFd( sock->get() ) )
                            {
                                auto * ctx = static_cast<IoSendCtx *>(sockIoCtx);
                                _IoSocketCtxClearTimerCtx(ctx);

                                ctx->cnnAvail = true;
                                ctx->costTimeMs += winux::GetUtcTimeMs() - ctx->startTime;

                                if ( ctx->hadBytes < ctx->data.size() )
                                {
                                    size_t wantBytes = ctx->data.size() - ctx->hadBytes;
                                    int sendBytes = sock->send( ctx->data.getAt<winux::byte>(ctx->hadBytes), wantBytes );
                                    if ( sendBytes > 0 )
                                    {
                                        ctx->hadBytes += sendBytes;
                                    }
                                    else // sendBytes <= 0
                                    {
                                        ctx->cnnAvail = false;
                                    }
                                }

                                if ( ctx->hadBytes >= ctx->data.size() || !ctx->cnnAvail )
                                {
                                    // 处理回调
                                    if ( ctx->cbOk )
                                    {
                                        ctx->cbOk( sock, ctx->hadBytes, ctx->costTimeMs, ctx->cnnAvail );
                                    }

                                    // 已处理，完成这个请求
                                    ctx->changeState(stateFinish);
                                }
                                else
                                {
                                    IoEventsData & ioEvents = *this;
                                    // 重投这个IO请求和Timer
                                    ctx->startTime = winux::GetUtcTimeMs();
                                    if ( ctx->timeoutMs != -1 )
                                    {
                                        eiennet::async::Timer::New( *sock->getService() )->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
                                        }, ctx, sock->getThread() );
                                    }
                                }

                                // 就绪数-1
                                rc--;
                            }
                            break;
                        case ioRecvFrom:
                            if ( this->_sel.hasReadFd( sock->get() ) )
                            {
                                auto * ctx = static_cast<IoRecvFromCtx *>(sockIoCtx);
                                _IoSocketCtxClearTimerCtx(ctx);

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
                                    // 处理回调
                                    if ( ctx->cbOk )
                                    {
                                        ctx->cbOk( sock, ctx->data, ctx->epFrom );
                                    }

                                    // 已处理，完成这个请求
                                    ctx->changeState(stateFinish);
                                }
                                else
                                {
                                    IoEventsData & ioEvents = *this;
                                    // 重投这个IO请求和Timer
                                    ctx->startTime = winux::GetUtcTimeMs();
                                    if ( ctx->timeoutMs != -1 )
                                    {
                                        eiennet::async::Timer::New( *sock->getService() )->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
                                        }, ctx, sock->getThread() );
                                    }
                                }

                                // 就绪数-1
                                rc--;
                            }
                            break;
                        case ioSendTo:
                            if ( this->_sel.hasWriteFd( sock->get() ) )
                            {
                                auto * ctx = static_cast<IoSendToCtx *>(sockIoCtx);
                                _IoSocketCtxClearTimerCtx(ctx);

                                bool fail = false;
                                ctx->costTimeMs += winux::GetUtcTimeMs() - ctx->startTime;

                                if ( ctx->hadBytes < ctx->data.size() )
                                {
                                    size_t wantBytes = ctx->data.size() - ctx->hadBytes;
                                    int sendBytes = sock->sendTo( *ctx->epTo.get(), ctx->data.getAt<winux::byte>(ctx->hadBytes), wantBytes );
                                    if ( sendBytes > 0 )
                                    {
                                        ctx->hadBytes += sendBytes;
                                    }
                                    else // sendBytes <= 0
                                    {
                                        fail = true;
                                    }
                                }

                                if ( ctx->hadBytes >= ctx->data.size() || fail )
                                {
                                    // 处理回调
                                    if ( ctx->cbOk )
                                    {
                                        ctx->cbOk( sock, ctx->hadBytes, ctx->costTimeMs );
                                    }

                                    // 已处理，完成这个请求
                                    ctx->changeState(stateFinish);
                                }
                                else
                                {
                                    IoEventsData & ioEvents = *this;
                                    // 重投这个IO请求和Timer
                                    ctx->startTime = winux::GetUtcTimeMs();
                                    if ( ctx->timeoutMs != -1 )
                                    {
                                        eiennet::async::Timer::New( *sock->getService() )->waitAsyncEx( ctx->timeoutMs, false, [&ioEvents] ( winux::SharedPointer<eiennet::async::Timer> timer, io::IoTimerCtx * timerCtx ) {
                                            _IoSocketCtxTimeoutCallback( timer, timerCtx, ioEvents );
                                        }, ctx, sock->getThread() );
                                    }
                                }

                                // 就绪数-1
                                rc--;
                            }
                            break;
                        }
                    }
                } // if ( ioCtx->state == stateNormal )
            }
            else // rc <= 0
            {
                break;
            }

            // 如果已经是`end`则不能再`++it`
            if ( !hasEraseInIoVec && it != ioVec.end() ) ++it;
        } // for ( auto it = ioVec.begin(); it != ioVec.end(); hasEraseInIoVec = false )

        // 如果IO映射表已空，则删除该异步对象
        if ( ioVec.empty() )
        {
            itVecStruct = this->_ioVecMap.erase(itVecStruct);
            hasEraseInIoVecMap = true;
        }

        // 如果已经是`end`则不能再`++it`
        if ( !hasEraseInIoVecMap && itVecStruct != this->_ioVecMap.end() ) ++itVecStruct;
    }
}

void  IoEventsData::_handleIoCtxsTimeoutAndDelete()
{
    bool hasEraseInIoVecMap = false;
    // 枚举IO对象
    for ( auto itVecStruct = this->_ioVecMap.begin(); itVecStruct != this->_ioVecMap.end(); hasEraseInIoVecMap = false )
    {
        auto & ioVecStruct = itVecStruct->second;
        auto & ioVec = itVecStruct->second.ctxs;

        // 枚举IO请求
        bool hasEraseInIoVec = false;
        for ( auto it = ioVec.begin(); it != ioVec.end(); hasEraseInIoVec = false )
        {
            auto * ioCtx = *it;

            if ( ioCtx->state != stateNormal ) // 不是普通状态
            {
                it = ioVec.erase(it); // 删除已取消的IO事件
                hasEraseInIoVec = true;

                if ( ioCtx->type != ioTimer ) // Socket的事件处理
                {
                    auto * sockCtx = dynamic_cast<IoSocketCtx *>(ioCtx);
                    if ( ioCtx->state == stateTimeoutCancel ) // 超时取消，处理超时响应
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
                                auto * ctx = static_cast<IoConnectCtx *>(sockCtx);
                                if ( ctx->cbTimeout )
                                {
                                    ctx->cbTimeout( ctx->sock, ctx );
                                }
                            }
                            break;
                        case ioRecv:
                            {
                                auto * ctx = static_cast<IoRecvCtx *>(sockCtx);
                                if ( ctx->cbTimeout )
                                {
                                    ctx->cbTimeout( ctx->sock, ctx );
                                }
                            }
                            break;
                        case ioSend:
                            {
                                auto * ctx = static_cast<IoSendCtx *>(sockCtx);
                                if ( ctx->cbTimeout )
                                {
                                    ctx->cbTimeout( ctx->sock, ctx );
                                }
                            }
                            break;
                        case ioRecvFrom:
                            {
                                auto * ctx = static_cast<IoRecvFromCtx *>(sockCtx);
                                if ( ctx->cbTimeout )
                                {
                                    ctx->cbTimeout( ctx->sock, ctx );
                                }
                            }
                            break;
                        case ioSendTo:
                            {
                                auto * ctx = static_cast<IoSendToCtx *>(sockCtx);
                                if ( ctx->cbTimeout )
                                {
                                    ctx->cbTimeout( ctx->sock, ctx );
                                }
                            }
                            break;
                        }
                    }
                }

                // 删除`IoCtx`
                ioCtx->decRef();
            } // ioCtx->state != stateNormal

            // 如果已经是end则不能再++it
            if ( !hasEraseInIoVec && it != ioVec.end() ) ++it;
        } // for ( auto it = ioVec.begin(); it != ioVec.end(); hasEraseInIoVec = false )

        // 如果IO映射表已空，则删除该异步对象
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
                }
            }
            else // 未存在此timer
            {
                auto & ioVecStruct = this->_ioVecMap[timerFd];
                auto & ioVec = ioVecStruct.ctxs;
                ioVec.push_back(timerCtx);
            }
        }
        break;
    default:
        {
            auto * sockCtx = dynamic_cast<IoSocketCtx *>(ioCtx);
            auto sockFd = _GetFdByIoCtx(sockCtx);
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
                            existingCtx->timerCtx->decRef();
                            // 删除关联的超时timer
                            this->_ioVecMap.erase(timerFd);
                        }
                        existingCtx->timerCtx = nullptr; // 解除关联
                    }
                    existingCtx->decRef(); // 释放已存在的IoCtx
                    ioVec.erase(it);
                }

                ioVec.push_back(sockCtx);
            }
            else // 未存在此socket
            {
                auto & ioVecStruct = this->_ioVecMap[sockFd];
                auto & ioVec = ioVecStruct.ctxs;
                ioVec.push_back(sockCtx);
            }
        }
        break;
    }
}

// class IoServiceThread ----------------------------------------------------------------------
void IoServiceThread::run()
{
    _SelectWorkerFunc( this->_serv, this, this->_ioEvents, &this->_stop );
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

}

void IoService::stop()
{
    this->_stop = true;
    this->_ioEvents.wakeUpTrigger(IoEventsData::wutWantStop);
    for ( size_t i = 0; i < _group.count(); i++ )
    {
        // 给每个线程投递退出信号
        auto * th = this->getGroupThread<IoServiceThread>(i);
        th->_stop = true;
        th->_ioEvents.wakeUpTrigger(IoEventsData::wutWantStop);
    }
}

int IoService::run()
{
    this->_group.startup();
    _SelectWorkerFunc( this, nullptr, this->_ioEvents, &this->_stop );
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
            auto * sockIoCtx = dynamic_cast<IoSocketCtx *>(ioCtx);
            if ( sockIoCtx->timerCtx ) // 如果有超时定时器，停止它
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


} // namespace select


} // namespace io
