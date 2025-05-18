#pragma once

namespace eiennet
{
/** \brief IO类型 */
enum IoType
{
    ioNone,
    ioAccept,
    ioConnect,
    ioRecv,
    ioSend,
    ioRecvFrom,
    ioSendTo,
    ioTimer
};

/** \brief IO场景基类 */
struct IoCtx
{
    winux::uint64 startTime; //!< 请求开启的时间
    winux::uint64 timeoutMs; //!< 超时时间

    IoCtx() : startTime(winux::GetUtcTimeMs()), timeoutMs(-1)
    {
    }

    virtual ~IoCtx()
    {
    }
};

class AsyncSocket;

/** \brief 接受场景 */
struct IoAcceptCtx : IoCtx
{
    using OkFunction = std::function< bool ( winux::SharedPointer<AsyncSocket> servSock, winux::SharedPointer<AsyncSocket> clientSock, ip::EndPoint const & ep ) >;
    using TimeoutFunction = std::function< bool ( winux::SharedPointer<AsyncSocket> servSock, IoAcceptCtx * ctx ) >;

    OkFunction cbOk; //!< 成功回调函数
    TimeoutFunction cbTimeout; //!< 超时回调函数

    ip::EndPoint ep; //!< 客户连接IP地址

    IoAcceptCtx() { }
};

/** \brief 连接场景 */
struct IoConnectCtx : IoCtx
{
    using OkFunction = std::function< void ( winux::SharedPointer<AsyncSocket> sock, winux::uint64 costTimeMs ) >;
    using TimeoutFunction = std::function< void ( winux::SharedPointer<AsyncSocket> sock, IoConnectCtx * ctx ) >;

    OkFunction cbOk; //!< 成功回调函数
    TimeoutFunction cbTimeout; //!< 超时回调函数

    winux::uint64 costTimeMs; //!< 总花费时间

    IoConnectCtx() : costTimeMs(0) { }
};

/** \brief 数据接收场景 */
struct IoRecvCtx : IoCtx
{
    using OkFunction = std::function< void ( winux::SharedPointer<AsyncSocket> sock, winux::Buffer & data, bool cnnAvail ) >;
    using TimeoutFunction = std::function< void ( winux::SharedPointer<AsyncSocket> sock, IoRecvCtx * ctx ) >;

    OkFunction cbOk; //!< 成功回调函数
    TimeoutFunction cbTimeout; //!< 超时回调函数

    size_t hadBytes;        //!< 已接收数据量
    size_t targetBytes;     //!< 目标数据量
    winux::GrowBuffer data; //!< 已接收的数据
    bool cnnAvail; //!< 连接是否有效

    IoRecvCtx() : hadBytes(0), targetBytes(0), cnnAvail(false) { }
};

/** \brief 数据发送场景 */
struct IoSendCtx : IoCtx
{
    using OkFunction = std::function< void ( winux::SharedPointer<AsyncSocket> sock, size_t hadBytes, winux::uint64 costTimeMs, bool cnnAvail ) >;
    using TimeoutFunction = std::function< void ( winux::SharedPointer<AsyncSocket> sock, IoSendCtx * ctx ) >;

    OkFunction cbOk; //!< 成功回调函数
    TimeoutFunction cbTimeout; //!< 超时回调函数

    size_t hadBytes;    //!< 已发送数据量
    winux::uint64 costTimeMs; //!< 总花费时间
    winux::Buffer data; //!< 待发送的数据
    bool cnnAvail; //!< 连接是否有效

    IoSendCtx() : hadBytes(0), costTimeMs(0), cnnAvail(false) { }
};

/** \brief 无连接，数据接收场景 */
struct IoRecvFromCtx : IoCtx
{
    using OkFunction = std::function< void ( winux::SharedPointer<AsyncSocket> sock, winux::Buffer & data, EndPoint const & ep ) >;
    using TimeoutFunction = std::function< void ( winux::SharedPointer<AsyncSocket> sock, IoRecvFromCtx * ctx ) >;

    OkFunction cbOk; //!< 成功回调函数
    TimeoutFunction cbTimeout; //!< 超时回调函数

    size_t hadBytes;        //!< 已接收数据量
    size_t targetBytes;     //!< 目标数据量
    winux::GrowBuffer data; //!< 已接收的数据
    ip::EndPoint epFrom;    //!< 来自此EP的数据

    IoRecvFromCtx() : hadBytes(0), targetBytes(0) { }
};

/** \brief 无连接，数据发送场景 */
struct IoSendToCtx : IoCtx
{
    using OkFunction = std::function< void ( winux::SharedPointer<AsyncSocket> sock, size_t hadBytes, winux::uint64 costTimeMs ) >;
    using TimeoutFunction = std::function< void ( winux::SharedPointer<AsyncSocket> sock, IoSendToCtx * ctx ) >;

    OkFunction cbOk; //!< 成功回调函数
    TimeoutFunction cbTimeout; //!< 超时回调函数

    size_t hadBytes;    //!< 已发送数据量
    winux::uint64 costTimeMs; //!< 总花费时间
    winux::Buffer data; //!< 待发送的数据
    winux::SimplePointer<EndPoint> ep; // 目的地址

    IoSendToCtx() : hadBytes(0), costTimeMs(0) { }
};

/** \brief IO服务 */
class EIENNET_DLL IoService
{
public:
    using IoMapMap = std::map< winux::SharedPointer<AsyncSocket>, std::map< IoType, winux::SharedPointer<IoCtx> > >;
    using IoMap = IoMapMap::mapped_type;

    /** \brief 构造函数
     *
     *  \param threadCount int 线程数
     *  \param serverWait double 服务器等待时间(ms) */
    IoService( int threadCount = 4, double serverWait = 0.002 );

    /** \brief 析构函数 */
    virtual ~IoService();

    /** \brief 初始化 */
    bool init( int threadCount = 4, double serverWait = 0.002 );

    /** \brief 停止监听服务 */
    void stop();

    /** \brief 运行 */
    int run();

    /** \brief 获取监听的socks数，IO数 */
    void getSockIoCount( size_t * sockCount, size_t * ioCount ) const;

    /** \brief 移除指定sock的所有IO监听 */
    void removeSock( winux::SharedPointer<AsyncSocket> sock );

    /** \brief 暴露线程池 */
    winux::ThreadPool & getPool() { return _pool; }

    /** \brief 暴露客户映射表 { client_sock: { io_type: io_ctx, ... }, ... } */
    IoMapMap & getIoMaps() { return _ioMaps; }

    /** \brief 暴露互斥量 */
    winux::Mutex & getMutex() { return _mtx; }


protected:
    /** \brief 投递IO请求 */
    void _post( winux::SharedPointer<AsyncSocket> sock, IoType type, winux::SharedPointer<IoCtx> ctx );

    /** \brief IO请求加入Select轮询数组前 */
    DEFINE_CUSTOM_EVENT( RunBeforeJoin, (), () )
    /** \brief 完成Select.wait()后 */
    DEFINE_CUSTOM_EVENT( RunAfterWait, ( int rc ), (rc) )

private:
    winux::ThreadPool _pool;
    IoMapMap _ioMaps;
    winux::RecursiveMutex _mtx; //!< 互斥量，保护竞态数据
    winux::Condition _cdt;
    double _serverWait;         //!< 服务器IO等待时间间隔（秒）
    int _threadCount;
    bool _stop;

    friend class AsyncSocket;
    DISABLE_OBJECT_COPY(IoService)
};

/** \brief 异步套接字 */
class EIENNET_DLL AsyncSocket : public Socket, public winux::EnableSharedFromThis<AsyncSocket>
{
protected:
    /** \brief 构造函数1 */
    explicit AsyncSocket( IoService & ioServ, int sock = -1, bool isNewSock = false ) : Socket( sock, isNewSock ), _ioServ(&ioServ), _data(nullptr)
    {
        this->setBlocking(false);
    }

    /** \brief 构造函数2 */
    AsyncSocket( IoService & ioServ, AddrFamily af, SockType sockType, Protocol proto ) : Socket( af, sockType, proto ), _ioServ(&ioServ), _data(nullptr)
    {
        this->setBlocking(false);
    }

public:
    static winux::SharedPointer<AsyncSocket> New( IoService & ioServ, int sock = -1, bool isNewSock = false )
    {
        return winux::SharedPointer<AsyncSocket>( new AsyncSocket( ioServ, sock, isNewSock ) );
    }

    static winux::SharedPointer<AsyncSocket> New( IoService & ioServ, AddrFamily af, SockType sockType, Protocol proto )
    {
        return winux::SharedPointer<AsyncSocket>( new AsyncSocket( ioServ, af, sockType, proto ) );
    }

    winux::SharedPointer<AsyncSocket> accept( EndPoint * ep = nullptr )
    {
        int sock;
        return this->Socket::accept( &sock, ep ) ? winux::SharedPointer<AsyncSocket>( this->onCreateClient( *_ioServ, sock, true ) ) : winux::SharedPointer<AsyncSocket>();
    }

    /** \brief 设置套接字关联数据 */
    void setDataPtr( void * data ) { _data = data; }
    /** \brief 获取套接字关联数据 */
    void * getDataPtr() const { return _data; }
    /** \brief 获取套接字关联数据 */
    template < typename _Ty >
    _Ty * getDataPtr() const { return reinterpret_cast<_Ty*>(_data); }

    /** \brief 获取IO服务对象 */
    IoService & getService() const { return *_ioServ; }
    /** \brief 接受客户连接（异步） */
    void acceptAsync( IoAcceptCtx::OkFunction cbOk, winux::uint64 timeoutMs = -1, IoAcceptCtx::TimeoutFunction cbTimeout = nullptr );
    /** \brief 连接服务器（异步） */
    void connectAsync( EndPoint const & ep, IoConnectCtx::OkFunction cbOk, winux::uint64 timeoutMs = -1, IoConnectCtx::TimeoutFunction cbTimeout = nullptr );
    /** \brief 接收直到指定大小的数据（异步） */
    void recvUntilSizeAsync( size_t targetSize, IoRecvCtx::OkFunction cbOk, winux::uint64 timeoutMs = -1, IoRecvCtx::TimeoutFunction cbTimeout = nullptr );
    /** \brief 接收数据（异步） */
    void recvAsync( IoRecvCtx::OkFunction cbOk, winux::uint64 timeoutMs = -1, IoRecvCtx::TimeoutFunction cbTimeout = nullptr )
    {
        this->recvUntilSizeAsync( 0, cbOk, timeoutMs, cbTimeout );
    }
    /** \brief 发送数据（异步） */
    void sendAsync( void const * data, size_t size, IoSendCtx::OkFunction cbOk, winux::uint64 timeoutMs = -1, IoSendCtx::TimeoutFunction cbTimeout = nullptr );
    /** \brief 发送数据（异步） */
    void sendAsync( winux::Buffer const & data, IoSendCtx::OkFunction cbOk, winux::uint64 timeoutMs = -1, IoSendCtx::TimeoutFunction cbTimeout = nullptr )
    {
        this->sendAsync( data.getBuf(), data.getSize(), cbOk, timeoutMs, cbTimeout );
    }
    /** \brief 无连接，接收直到指定大小的数据（异步） */
    void recvFromUntilSizeAsync( size_t targetSize, IoRecvFromCtx::OkFunction cbOk, winux::uint64 timeoutMs = -1, IoRecvFromCtx::TimeoutFunction cbTimeout = nullptr );
    /** \brief 无连接，接收数据（异步） */
    void recvFromAsync( IoRecvFromCtx::OkFunction cbOk, winux::uint64 timeoutMs = -1, IoRecvFromCtx::TimeoutFunction cbTimeout = nullptr )
    {
        this->recvFromUntilSizeAsync( 0, cbOk, timeoutMs, cbTimeout );
    }
    /** \brief 无连接，发送数据（异步） */
    void sendToAsync( EndPoint const & ep, void const * data, size_t size, IoSendToCtx::OkFunction cbOk, winux::uint64 timeoutMs = -1, IoSendToCtx::TimeoutFunction cbTimeout = nullptr );
    /** \brief 无连接，发送数据（异步） */
    void sendToAsync( EndPoint const & ep, winux::Buffer const & data, IoSendToCtx::OkFunction cbOk, winux::uint64 timeoutMs = -1, IoSendToCtx::TimeoutFunction cbTimeout = nullptr )
    {
        this->sendToAsync( ep, data.getBuf(), data.getSize(), cbOk, timeoutMs, cbTimeout );
    }

    /** \brief 错误处理
     *
     *  \param sock 出错的Socket */
    DEFINE_CUSTOM_EVENT( Error, ( winux::SharedPointer<AsyncSocket> sock ), (sock) )

    /** \brief 创建客户连接 */
    DEFINE_CUSTOM_EVENT_RETURN_EX( AsyncSocket *, CreateClient, ( IoService & ioServ, int sock, bool isNewSock ) );

protected:
    IoService * _ioServ;
    void * _data;

    friend class IoService;
};

namespace ip
{
namespace tcp
{
/** \brief TCP/IP异步套接字 */
class EIENNET_DLL AsyncSocket : public eiennet::AsyncSocket
{
public:
    typedef eiennet::AsyncSocket BaseClass;

protected:
    AsyncSocket( IoService & ioServ, int sock, bool isNewSock = false ) : BaseClass( ioServ, sock, isNewSock ) { }

    explicit AsyncSocket( IoService & ioServ ) : BaseClass( ioServ, BaseClass::afInet, BaseClass::sockStream, BaseClass::protoUnspec ) { }

public:
    static winux::SharedPointer<AsyncSocket> New( IoService & ioServ, int sock, bool isNewSock = false )
    {
        return winux::SharedPointer<AsyncSocket>( new AsyncSocket( ioServ, sock, isNewSock ) );
    }

    static winux::SharedPointer<AsyncSocket> New( IoService & ioServ )
    {
        return winux::SharedPointer<AsyncSocket>( new AsyncSocket(ioServ) );
    }
};

} // namespace tcp

namespace udp
{
/** \brief UDP/IP异步套接字 */
class EIENNET_DLL AsyncSocket : public eiennet::AsyncSocket
{
public:
    typedef eiennet::AsyncSocket BaseClass;

protected:
    AsyncSocket( IoService & ioServ, int sock, bool isNewSock = false ) : BaseClass( ioServ, sock, isNewSock ) { }

    explicit AsyncSocket( IoService & ioServ ) : BaseClass( ioServ, BaseClass::afInet, BaseClass::sockDatagram, BaseClass::protoUnspec ) { }

public:
    static winux::SharedPointer<AsyncSocket> New( IoService & ioServ, int sock, bool isNewSock = false )
    {
        return winux::SharedPointer<AsyncSocket>( new AsyncSocket( ioServ, sock, isNewSock ) );
    }

    static winux::SharedPointer<AsyncSocket> New( IoService & ioServ )
    {
        return winux::SharedPointer<AsyncSocket>( new AsyncSocket(ioServ) );
    }
};

} // namespace udp

} // namespace ip

} // namespace eiennet

