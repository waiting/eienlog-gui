#pragma once

namespace eiennet
{
/** \brief 异步套接字相关 */
namespace async
{
class Socket;
class Timer;
typedef winux::SharedPointer<Socket> SocketSharedPtr;
typedef winux::SharedPointer<Timer> TimerSharedPtr;

/** \brief 异步套接字 */
class EIENNET_DLL Socket : public eiennet::Socket, public winux::EnableSharedFromThis<Socket>
{
protected:
    /** \brief 构造函数1 */
    explicit Socket( io::IoService & serv, int sock = -1, bool isNewSock = false );

    /** \brief 构造函数2 */
    Socket( io::IoService & serv, AddrFamily af, SockType sockType, Protocol proto );

public:
    static SocketSharedPtr New( io::IoService & serv, int sock = -1, bool isNewSock = false )
    {
        return SocketSharedPtr( new Socket( serv, sock, isNewSock ) );
    }

    static SocketSharedPtr New( io::IoService & serv, AddrFamily af, SockType sockType, Protocol proto )
    {
        return SocketSharedPtr( new Socket( serv, af, sockType, proto ) );
    }

    static SocketSharedPtr New( io::IoServiceSharedPtr serv, int sock = -1, bool isNewSock = false )
    {
        return SocketSharedPtr( new Socket( *serv.get(), sock, isNewSock ) );
    }

    static SocketSharedPtr New( io::IoServiceSharedPtr serv, AddrFamily af, SockType sockType, Protocol proto )
    {
        return SocketSharedPtr( new Socket( *serv.get(), af, sockType, proto ) );
    }

    virtual ~Socket();

    SocketSharedPtr accept( EndPoint * ep = nullptr )
    {
        int sock;
        return this->eiennet::Socket::accept( &sock, ep ) ? this->onCreateClient( *_serv, sock, true ) : SocketSharedPtr();
    }

    /** \brief 设置套接字关联数据 */
    void setDataPtr( void * data ) { _data = data; }
    /** \brief 获取套接字关联数据 */
    void * getDataPtr() const { return _data; }
    /** \brief 获取套接字关联数据 */
    template < typename _Ty >
    _Ty * getDataPtr() const { return reinterpret_cast<_Ty*>(_data); }

    /** \brief 设置关联线程 */
    void setThread( io::IoServiceThread * th ) { _thread = th; }
    /** \brief 获取关联线程 */
    template < typename _ThreadCls = io::IoServiceThread >
    _ThreadCls * getThread() const { return static_cast<_ThreadCls *>(_thread); }

    /** \brief 获取IO服务对象 */
    template < typename _ServiceCls = io::IoService >
    _ServiceCls * getService() const { return static_cast<_ServiceCls *>(_serv); }

    /** \brief 接受客户连接（异步） */
    template < typename _Fx1, typename _Fx2 = std::nullptr_t >
    void acceptAsync( _Fx1 && cbOk, winux::uint64 timeoutMs = -1, _Fx2 && cbTimeout = nullptr, io::IoServiceThread * th = nullptr )
    {
        this->_serv->postAccept(
            this->sharedFromThis(),
            winux::FuncWrap<io::IoAcceptCtx::OkFn>( std::forward<_Fx1>(cbOk) ),
            timeoutMs,
            winux::FuncWrap<io::IoAcceptCtx::TimeoutFn>( std::forward<_Fx2>(cbTimeout) ),
            th
        );
    }
    /** \brief 连接服务器（异步） */
    template < typename _Fx1, typename _Fx2 = std::nullptr_t >
    void connectAsync( EndPoint const & ep, _Fx1 && cbOk, winux::uint64 timeoutMs = -1, _Fx2 && cbTimeout = nullptr, io::IoServiceThread * th = (io::IoServiceThread *)-1 )
    {
        this->_serv->postConnect(
            this->sharedFromThis(),
            ep,
            winux::FuncWrap<io::IoConnectCtx::OkFn>( std::forward<_Fx1>(cbOk) ),
            timeoutMs,
            winux::FuncWrap<io::IoConnectCtx::TimeoutFn>( std::forward<_Fx2>(cbTimeout) ),
            th
        );
    }
    /** \brief 接收直到指定大小的数据（异步） */
    template < typename _Fx1, typename _Fx2 = std::nullptr_t >
    void recvUntilSizeAsync( size_t targetSize, _Fx1 && cbOk, winux::uint64 timeoutMs = -1, _Fx2 && cbTimeout = nullptr, io::IoServiceThread * th = (io::IoServiceThread *)-1 )
    {
        this->_serv->postRecv(
            this->sharedFromThis(),
            targetSize,
            winux::FuncWrap<io::IoRecvCtx::OkFn>( std::forward<_Fx1>(cbOk) ),
            timeoutMs,
            winux::FuncWrap<io::IoRecvCtx::TimeoutFn>( std::forward<_Fx2>(cbTimeout) ),
            th
        );
    }
    /** \brief 接收数据（异步） */
    template < typename _Fx1, typename _Fx2 = std::nullptr_t >
    void recvAsync( _Fx1 && cbOk, winux::uint64 timeoutMs = -1, _Fx2 && cbTimeout = nullptr, io::IoServiceThread * th = (io::IoServiceThread *)-1 )
    {
        this->recvUntilSizeAsync( 0, std::forward<_Fx1>(cbOk), timeoutMs, std::forward<_Fx2>(cbTimeout), th );
    }
    /** \brief 发送数据（异步） */
    template < typename _Fx1, typename _Fx2 = std::nullptr_t >
    void sendAsync( void const * data, size_t size, _Fx1 && cbOk, winux::uint64 timeoutMs = -1, _Fx2 && cbTimeout = nullptr, io::IoServiceThread * th = (io::IoServiceThread *)-1 )
    {
        this->_serv->postSend(
            this->sharedFromThis(),
            data,
            size,
            winux::FuncWrap<io::IoSendCtx::OkFn>( std::forward<_Fx1>(cbOk) ),
            timeoutMs,
            winux::FuncWrap<io::IoSendCtx::TimeoutFn>( std::forward<_Fx2>(cbTimeout) ),
            th
        );
    }
    /** \brief 发送数据（异步） */
    template < typename _Fx1, typename _Fx2 = std::nullptr_t >
    void sendAsync( winux::Buffer const & data, _Fx1 && cbOk, winux::uint64 timeoutMs = -1, _Fx2 && cbTimeout = nullptr, io::IoServiceThread * th = (io::IoServiceThread *)-1 )
    {
        this->sendAsync( data.get(), data.size(), std::forward<_Fx1>(cbOk), timeoutMs, std::forward<_Fx2>(cbTimeout), th );
    }
    /** \brief 无连接，接收直到指定大小的数据（异步） */
    template < typename _Fx1, typename _Fx2 = std::nullptr_t >
    void recvFromUntilSizeAsync( size_t targetSize, _Fx1 && cbOk, winux::uint64 timeoutMs = -1, _Fx2 && cbTimeout = nullptr, io::IoServiceThread * th = (io::IoServiceThread *)-1 )
    {
        this->_serv->postRecvFrom(
            this->sharedFromThis(),
            targetSize,
            winux::FuncWrap<io::IoRecvFromCtx::OkFn>( std::forward<_Fx1>(cbOk) ),
            timeoutMs,
            winux::FuncWrap<io::IoRecvFromCtx::TimeoutFn>( std::forward<_Fx2>(cbTimeout) ),
            th
        );
    }
    /** \brief 无连接，接收数据（异步） */
    template < typename _Fx1, typename _Fx2 = std::nullptr_t >
    void recvFromAsync( _Fx1 && cbOk, winux::uint64 timeoutMs = -1, _Fx2 && cbTimeout = nullptr, io::IoServiceThread * th = (io::IoServiceThread *)-1 )
    {
        this->recvFromUntilSizeAsync( 0, std::forward<_Fx1>(cbOk), timeoutMs, std::forward<_Fx2>(cbTimeout), th );
    }
    /** \brief 无连接，发送数据（异步） */
    template < typename _Fx1, typename _Fx2 = std::nullptr_t >
    void sendToAsync( EndPoint const & ep, void const * data, size_t size, io::IoSendToCtx::OkFn cbOk, winux::uint64 timeoutMs = -1, io::IoSendToCtx::TimeoutFn cbTimeout = nullptr, io::IoServiceThread * th = (io::IoServiceThread *)-1 )
    {
        this->_serv->postSendTo(
            this->sharedFromThis(),
            ep,
            data,
            size,
            winux::FuncWrap<io::IoSendToCtx::OkFn>( std::forward<_Fx1>(cbOk) ),
            timeoutMs,
            winux::FuncWrap<io::IoSendToCtx::TimeoutFn>( std::forward<_Fx2>(cbTimeout) ),
            th
        );
    }
    /** \brief 无连接，发送数据（异步） */
    template < typename _Fx1, typename _Fx2 = std::nullptr_t >
    void sendToAsync( EndPoint const & ep, winux::Buffer const & data, _Fx1 && cbOk, winux::uint64 timeoutMs = -1, _Fx2 && cbTimeout = nullptr, io::IoServiceThread * th = (io::IoServiceThread *)-1 )
    {
        this->sendToAsync( ep, data.get(), data.size(), std::forward<_Fx1>(cbOk), timeoutMs, std::forward<_Fx2>(cbTimeout), th );
    }

    /** \brief 错误处理
     *
     *  \param sock 出错的Socket */
    DEFINE_CUSTOM_EVENT( Error, ( SocketSharedPtr sock ), (sock) )

    /** \brief 创建客户连接 */
    DEFINE_CUSTOM_EVENT_RETURN_EX( SocketSharedPtr, CreateClient, ( io::IoService & serv, int sock, bool isNewSock ) );

protected:
    io::IoService * _serv; // IO服务对象
    void * _data; // 套接字关联数据
    io::IoServiceThread * _thread; // 线程
};


/** \brief 定时器 */
class EIENNET_DLL Timer : public winux::EnableSharedFromThis<Timer>
{
protected:
    Timer( io::IoService & serv );

public:
    static TimerSharedPtr New( io::IoService & serv )
    {
        return TimerSharedPtr( new Timer(serv) );
    }

    static TimerSharedPtr New( io::IoServiceSharedPtr serv )
    {
        return TimerSharedPtr( new Timer( *serv.get() ) );
    }

    virtual ~Timer();

    void create();

    void destroy();

    void set( winux::uint64 timeoutMs, bool periodic );

    void unset();

    /** \brief 停止定时器
     *
     *  标记定时器关联的IoTimerCtx为主动取消，标记为非周期。
     *  如果定时器未触发信号，返回`IoTimerCtx`并设置`Timer::_timerCtx`为`nullptr`，否则返回`nullptr`。 */
    io::IoTimerCtx * stop();

    template < typename _Fx1 >
    void waitAsync( winux::uint64 timeoutMs, bool periodic, _Fx1 && cbOk )
    {
        this->waitAsyncEx( timeoutMs, periodic, std::forward<_Fx1>(cbOk) );
    }

    template < typename _Fx1 >
    void waitAsyncEx(
        winux::uint64 timeoutMs,
        bool periodic,
        _Fx1 && cbOk,
        io::IoSocketCtx * assocCtx = nullptr,
        io::IoServiceThread * th = (io::IoServiceThread *)-1
    )
    {
        this->_serv->postTimer(
            this->sharedFromThis(),
            timeoutMs,
            periodic,
            winux::FuncWrap<io::IoTimerCtx::OkFn>( std::forward<_Fx1>(cbOk) ),
            assocCtx,
            th
        );
    }

    /** \brief 设置关联线程 */
    void setThread( io::IoServiceThread * th ) { _thread = th; }
    /** \brief 获取关联线程 */
    template < typename _ThreadCls = io::IoServiceThread >
    _ThreadCls * getThread() const { return static_cast<_ThreadCls *>(_thread); }

    /** \brief 获取IO服务对象 */
    template < typename _ServiceCls = io::IoService >
    _ServiceCls * getService() const { return static_cast<_ServiceCls *>(_serv); }

    winux::MutexNative & getMutex() const { return const_cast<winux::MutexNative &>(_mtx); }

    /** \brief 获取底层定时器的句柄
     * 
     *  Windows平台是PTP_TIMER，Linux平台是timerfd文件描述符 */
    intptr_t get() const;

    // 一个Timer只能投递一个IoTimerCtx，这个成员标记关联的IoTimerCtx，同时为Windows平台下回调函数传递IoTimerCtx
    io::IoTimerCtx * _timerCtx;
    // 是否已经发出定时器信号，如果是周期性的，则应恢复成false
    bool _posted;

private:
    winux::PlainMembers<struct Timer_Data, 8> _self;

    io::IoService * _serv;
    io::IoServiceThread * _thread;
    winux::MutexNative _mtx;

    friend struct Timer_Data;
    DISABLE_OBJECT_COPY(Timer)
};


} // namespace async


namespace ip
{
namespace tcp
{
namespace async
{
class Socket;
typedef winux::SharedPointer<Socket> SocketSharedPtr;

/** \brief TCP/IP异步套接字 */
class EIENNET_DLL Socket : public eiennet::async::Socket
{
public:
    typedef eiennet::async::Socket BaseClass;

protected:
    Socket( io::IoService & serv, int sock, bool isNewSock = false ) : BaseClass( serv, sock, isNewSock ) { }

    explicit Socket( io::IoService & serv ) : BaseClass( serv, BaseClass::afInet, BaseClass::sockStream, BaseClass::protoUnspec ) { }

public:
    static SocketSharedPtr New( io::IoService & serv, int sock, bool isNewSock = false )
    {
        return SocketSharedPtr( new Socket( serv, sock, isNewSock ) );
    }

    static SocketSharedPtr New( io::IoService & serv )
    {
        return SocketSharedPtr( new Socket(serv) );
    }

    static SocketSharedPtr New( io::IoServiceSharedPtr serv, int sock, bool isNewSock = false )
    {
        return SocketSharedPtr( new Socket( *serv.get(), sock, isNewSock ) );
    }

    static SocketSharedPtr New( io::IoServiceSharedPtr serv )
    {
        return SocketSharedPtr( new Socket( *serv.get() ) );
    }
};


} // namespace async


} // namespace tcp


namespace udp
{
namespace async
{
class Socket;
typedef winux::SharedPointer<Socket> SocketSharedPtr;

/** \brief UDP/IP异步套接字 */
class EIENNET_DLL Socket : public eiennet::async::Socket
{
public:
    typedef eiennet::async::Socket BaseClass;

protected:
    Socket( io::IoService & serv, int sock, bool isNewSock = false ) : BaseClass( serv, sock, isNewSock ) { }

    explicit Socket( io::IoService & serv ) : BaseClass( serv, BaseClass::afInet, BaseClass::sockDatagram, BaseClass::protoUnspec ) { }

public:
    static SocketSharedPtr New( io::IoService & serv, int sock, bool isNewSock = false )
    {
        return SocketSharedPtr( new Socket( serv, sock, isNewSock ) );
    }

    static SocketSharedPtr New( io::IoService & serv )
    {
        return SocketSharedPtr( new Socket(serv) );
    }

    static SocketSharedPtr New( io::IoServiceSharedPtr serv, int sock, bool isNewSock = false )
    {
        return SocketSharedPtr( new Socket( *serv.get(), sock, isNewSock ) );
    }

    static SocketSharedPtr New( io::IoServiceSharedPtr serv )
    {
        return SocketSharedPtr( new Socket( *serv.get() ) );
    }
};


} // namespace async


} // namespace udp


} // namespace ip


} // namespace eiennet
