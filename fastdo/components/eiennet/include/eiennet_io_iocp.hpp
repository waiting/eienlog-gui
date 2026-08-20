#ifndef __EIENNET_IO_IOCP_HPP__
#define __EIENNET_IO_IOCP_HPP__

/** \brief IO模型 */
namespace io
{
namespace iocp
{
    class IoService;
    struct IoAcceptCtx;
    struct IoConnectCtx;
    bool _PostAccept( IoService * serv, IoAcceptCtx * ctx );
    bool _PostConnect( IoService * serv, IoConnectCtx * ctx, eiennet::EndPoint const & ep );
}
/** \brief IOCP封装 */
class EIENNET_DLL Iocp
{
public:
    Iocp();
    ~Iocp();

    // 初始化一些函数，因为这是属于WinSock2规范之外的微软另外提供的扩展函数，所以需要额外获取一下函数的指针
    bool initFuncs();

    // 关联句柄到IOCP
    bool associate( HANDLE h, ULONG_PTR key );

    // 投递自定义IOCP完成消息
    void postCustom( DWORD bytesTransferred, ULONG_PTR key, LPOVERLAPPED ol );

    HANDLE get() const;
    operator bool() const;

private:
    winux::PlainMembers< struct Iocp_Data, sizeof(HANDLE) * 5 > _self;
    friend bool iocp::_PostAccept( iocp::IoService * serv, iocp::IoAcceptCtx * ctx );
    friend bool iocp::_PostConnect( iocp::IoService * serv, iocp::IoConnectCtx * ctx, eiennet::EndPoint const & ep );
    friend class iocp::IoService;

    DISABLE_OBJECT_COPY(Iocp)
};

/** \brief IOCP 模型 */
namespace iocp
{
/** \brief IO场景 */
struct IoCtx : virtual io::IoCtx
{
    OVERLAPPED ol;

protected:
    IoCtx()
    {
        this->state = stateNormal;
        ZeroMemory( &this->ol, sizeof(OVERLAPPED) );
    }
    virtual ~IoCtx() { }
};

/** \brief 接受场景接口 */
struct IoAcceptCtx : IoCtx, io::IoAcceptCtx, winux::EnableStaticNew<IoAcceptCtx>
{
public:
    winux::Buffer outputBuf; // for AcceptEx() lpOutputBuffer
    winux::uint32 localAddrLen;
    winux::uint32 remoteAddrLen;
    winux::SharedPointer<eiennet::async::Socket> clientSock;

    virtual bool changeState( IoState state ) override
    {
        io::IoAcceptCtx::changeState(state);
        switch ( this->state )
        {
        case stateProactiveCancel:
        case stateTimeoutCancel:
            if ( this->sock && this->sock->operator bool() )
            {
                if ( CancelIoEx( (HANDLE)(INT_PTR)this->sock->get(), &this->ol ) )
                {
                    return true;
                }
            }
            break;
        case stateFinish:
            ;
        }
        return false;
    }

protected:
    IoAcceptCtx() : localAddrLen(0), remoteAddrLen(0) { }
    virtual ~IoAcceptCtx() { }

    FRIEND_ENABLE_STATIC_NEW;
};

/** \brief 连接场景接口 */
struct IoConnectCtx : IoCtx, io::IoConnectCtx, winux::EnableStaticNew<IoConnectCtx>
{
public:
    virtual bool changeState( IoState state ) override
    {
        io::IoConnectCtx::changeState(state);
        switch ( this->state )
        {
        case stateProactiveCancel:
        case stateTimeoutCancel:
            if ( this->sock && this->sock->operator bool() )
            {
                if ( CancelIoEx( (HANDLE)(INT_PTR)this->sock->get(), &this->ol ) )
                {
                    return true;
                }
            }
            break;
        case stateFinish:
            ;
        }
        return false;
    }

protected:
    IoConnectCtx() { }
    virtual ~IoConnectCtx() { }

    FRIEND_ENABLE_STATIC_NEW;
};

/** \brief 数据接收场景接口 */
struct IoRecvCtx : IoCtx, io::IoRecvCtx, winux::EnableStaticNew<IoRecvCtx>
{
    WSABUF wsabuf;

public:
    virtual bool changeState( IoState state ) override
    {
        io::IoRecvCtx::changeState(state);
        switch ( this->state )
        {
        case stateProactiveCancel:
        case stateTimeoutCancel:
            if ( this->sock && this->sock->operator bool() )
            {
                if ( CancelIoEx( (HANDLE)(INT_PTR)this->sock->get(), &this->ol ) )
                {
                    return true;
                }
            }
            break;
        case stateFinish:
            ;
        }
        return false;
    }

protected:
    IoRecvCtx()
    {
        ZeroMemory( &this->wsabuf, sizeof(WSABUF) );
    }
    virtual ~IoRecvCtx() { }

    FRIEND_ENABLE_STATIC_NEW;
};

/** \brief 数据发送场景接口 */
struct IoSendCtx : IoCtx, io::IoSendCtx, winux::EnableStaticNew<IoSendCtx>
{
    WSABUF wsabuf;

public:
    virtual bool changeState( IoState state ) override
    {
        io::IoSendCtx::changeState(state);
        switch ( this->state )
        {
        case stateProactiveCancel:
        case stateTimeoutCancel:
            if ( this->sock && this->sock->operator bool() )
            {
                if ( CancelIoEx( (HANDLE)(INT_PTR)this->sock->get(), &this->ol ) )
                {
                    return true;
                }
            }
            break;
        case stateFinish:
            ;
        }
        return false;
    }

protected:
    IoSendCtx()
    {
        ZeroMemory( &this->wsabuf, sizeof(WSABUF) );
    }
    virtual ~IoSendCtx() { }

    FRIEND_ENABLE_STATIC_NEW;
};

/** \brief 无连接，数据接收场景接口 */
struct IoRecvFromCtx : IoCtx, io::IoRecvFromCtx, winux::EnableStaticNew<IoRecvFromCtx>
{
    WSABUF wsabuf;

public:
    virtual bool changeState( IoState state ) override
    {
        io::IoRecvFromCtx::changeState(state);
        switch ( this->state )
        {
        case stateProactiveCancel:
        case stateTimeoutCancel:
            if ( this->sock && this->sock->operator bool() )
            {
                if ( CancelIoEx( (HANDLE)(INT_PTR)this->sock->get(), &this->ol ) )
                {
                    return true;
                }
            }
            break;
        case stateFinish:
            ;
        }
        return false;
    }

protected:
    IoRecvFromCtx()
    {
        ZeroMemory( &this->wsabuf, sizeof(WSABUF) );
    }
    virtual ~IoRecvFromCtx() { }

    FRIEND_ENABLE_STATIC_NEW;
};

/** \brief 无连接，数据发送场景接口 */
struct IoSendToCtx : IoCtx, io::IoSendToCtx, winux::EnableStaticNew<IoSendToCtx>
{
    WSABUF wsabuf;

public:
    virtual bool changeState( IoState state ) override
    {
        io::IoSendToCtx::changeState(state);
        switch ( this->state )
        {
        case stateProactiveCancel:
        case stateTimeoutCancel:
            if ( this->sock && this->sock->operator bool() )
            {
                if ( CancelIoEx( (HANDLE)(INT_PTR)this->sock->get(), &this->ol ) )
                {
                    return true;
                }
            }
            break;
        case stateFinish:
            ;
        }
        return false;
    }

protected:
    IoSendToCtx()
    {
        ZeroMemory( &this->wsabuf, sizeof(WSABUF) );
    }
    virtual ~IoSendToCtx() { }

    FRIEND_ENABLE_STATIC_NEW;
};

/** \brief 定时器IO场景 */
struct IoTimerCtx : IoCtx, io::IoTimerCtx, winux::EnableStaticNew<IoTimerCtx>
{
public:
    virtual bool changeState( IoState state ) override
    {
        io::IoTimerCtx::changeState(state);
        switch ( this->state )
        {
        case stateProactiveCancel:
        case stateTimeoutCancel:
            if ( this->timer )
            {
                this->timer->unset();
                return true;
            }
            break;
        case stateFinish:
            ;
        }
        return false;
    }

protected:
    IoTimerCtx() { }
    virtual ~IoTimerCtx() { }

    FRIEND_ENABLE_STATIC_NEW;
};


class IoService;
class IoServiceThread;

/** \brief IO事件数据 */
class EIENNET_DLL IoEventsData
{
public:
    enum WakeUpType
    {
        wutWantNone, //!< 无目的单纯唤醒
        wutWantStop, //!< 欲要停止，唤醒
        wutWantUpdate, //!< 欲要更新IO事件，唤醒以更新事件监听
    };
    struct IoVecStruct
    {
        std::vector<IoCtx *> ctxs; //!< IoCtx vector

        IoVecStruct() { }
    };

    using IoVecMap = std::map< int, IoVecStruct >;

    /** \brief 构造函数 */
    IoEventsData();

    /** \brief 唤醒沉默的iocp等待
    *
    *  \param type 唤醒类型 */
    void wakeUpTrigger( WakeUpType type );

    /** \brief 预投递
    *
    *  \param ioCtx IoCtx实例 */
    void prePost( IoCtx * ioCtx );

    /** \brief 投递IO事件
    *
    *  \param ioCtx IoCtx实例 */
    void post( IoCtx * ioCtx );

private:
    // 处理IoCtxs投递
    void _handleIoCtxsPost();
    // 处理IoCtxs监听
    void _handleIoCtxsListen();
    // 处理IoCtxs事件回调
    void _handleIoCtxsCallback( int rc );
    // 处理IoCtxs超时响应以及删除取消的IO
    void _handleIoCtxsTimeoutAndDelete();

    std::vector<IoCtx *> _preIoCtxs; //!< 预投递的IoCtx
    winux::Mutex _mtxPreIoCtxs; //!< 互斥量，保护PreIoCtxs数据

    IoVecMap _ioVecMap; //!< 监听IO事件数据结构

    Iocp _iocp; //!< iocp实例

    size_t _sockIoCount; // 套接字IO数
    size_t _timerIoCount; // 定时器IO数

    friend void _IocpWorkerFunc( IoService * serv, IoServiceThread * thread, IoEventsData & ioEvents, bool * stop );
    friend bool _PostAccept( IoService * serv, IoAcceptCtx * ctx );
    friend bool _PostConnect( IoService * serv, IoConnectCtx * ctx, eiennet::EndPoint const & ep );
    friend class IoService;
    friend class IoServiceThread;
};

/** \brief Io服务线程 */
class EIENNET_DLL IoServiceThread : public io::IoServiceThread
{
public:
    IoServiceThread( IoService * serv ) : _serv(serv), _stop(false)
    {
    }

    virtual void run() override;

    virtual void timerTrigger( io::IoTimerCtx * timerCtx ) override;

    /** \brief 获取套接字IO数 */
    virtual size_t getSockIoCount() const override { return _ioEvents._sockIoCount; }
    /** \brief 获取定时器IO数 */
    virtual size_t getTimerIoCount() const override { return _ioEvents._timerIoCount; }

private:
    IoEventsData _ioEvents;
    IoService * _serv;
    bool _stop;

    friend bool _PostAccept( IoService * serv, IoAcceptCtx * ctx );
    friend bool _PostConnect( IoService * serv, IoConnectCtx * ctx, eiennet::EndPoint const & ep );
    friend bool _PostRecv( IoService * serv, IoRecvCtx * ctx );
    friend bool _PostSend( IoService * serv, IoSendCtx * ctx );
    friend bool _PostRecvFrom( IoService * serv, IoRecvFromCtx * ctx );
    friend bool _PostSendTo( IoService * serv, IoSendToCtx * ctx );
    friend class IoService;

    DISABLE_OBJECT_COPY(IoServiceThread)
};

/** \brief Io服务类 */
class EIENNET_DLL IoService : public io::IoService
{
public:
    IoService( size_t groupThread = 4 );

    virtual void stop() override;
    virtual int run() override;

    virtual void postAccept(
        winux::SharedPointer<eiennet::async::Socket> sock,
        IoAcceptCtx::OkFn cbOk,
        winux::uint64 timeoutMs = -1,
        IoAcceptCtx::TimeoutFn cbTimeout = nullptr,
        io::IoServiceThread * th = nullptr
    ) override;
    virtual void postConnect(
        winux::SharedPointer<eiennet::async::Socket> sock,
        eiennet::EndPoint const & ep,
        IoConnectCtx::OkFn cbOk,
        winux::uint64 timeoutMs = -1,
        IoConnectCtx::TimeoutFn cbTimeout = nullptr,
        io::IoServiceThread * th = (io::IoServiceThread *)-1
    ) override;
    virtual void postRecv(
        winux::SharedPointer<eiennet::async::Socket> sock,
        size_t targetSize,
        IoRecvCtx::OkFn cbOk,
        winux::uint64 timeoutMs = -1,
        IoRecvCtx::TimeoutFn cbTimeout = nullptr,
        io::IoServiceThread * th = (io::IoServiceThread *)-1
    ) override;
    virtual void postSend(
        winux::SharedPointer<eiennet::async::Socket> sock,
        void const * data,
        size_t size,
        IoSendCtx::OkFn cbOk,
        winux::uint64 timeoutMs = -1,
        IoSendCtx::TimeoutFn cbTimeout = nullptr,
        io::IoServiceThread * th = (io::IoServiceThread *)-1
    ) override;
    virtual void postRecvFrom(
        winux::SharedPointer<eiennet::async::Socket> sock,
        size_t targetSize,
        IoRecvFromCtx::OkFn cbOk,
        winux::uint64 timeoutMs = -1,
        IoRecvFromCtx::TimeoutFn cbTimeout = nullptr,
        io::IoServiceThread * th = (io::IoServiceThread *)-1
    ) override;
    virtual void postSendTo(
        winux::SharedPointer<eiennet::async::Socket> sock,
        eiennet::EndPoint const & ep,
        void const * data,
        size_t size,
        IoSendToCtx::OkFn cbOk,
        winux::uint64 timeoutMs = -1,
        IoSendToCtx::TimeoutFn cbTimeout = nullptr,
        io::IoServiceThread * th = (io::IoServiceThread *)-1
    ) override;
    virtual void postTimer(
        winux::SharedPointer<eiennet::async::Timer> timer,
        winux::uint64 timeoutMs,
        bool periodic,
        IoTimerCtx::OkFn cbOk,
        IoSocketCtx * assocCtx = nullptr,
        io::IoServiceThread * th = (io::IoServiceThread *)-1
    ) override;

    virtual void timerTrigger( io::IoTimerCtx * timerCtx ) override;

    /** \brief 标记删除指定sock所有IO监听 */
    virtual void removeSock( winux::SharedPointer<eiennet::async::Socket> sock ) override;

    /** \brief 关联线程
     *
     *  \param sock 异步套接字
     *  \param th 为空表示主线程，为-1表示自动分配，其他则为指定线程 */
    bool associate( winux::SharedPointer<eiennet::async::Socket> sock, io::IoServiceThread * th = (io::IoServiceThread *)-1 );

    /** \brief 获取套接字IO数 */
    virtual size_t getSockIoCount() const override { return _ioEvents._sockIoCount; }
    /** \brief 获取定时器IO数 */
    virtual size_t getTimerIoCount() const override { return _ioEvents._timerIoCount; }

private:
    IoEventsData _ioEvents;
    bool _stop;

    friend bool _PostAccept( IoService * serv, IoAcceptCtx * ctx );
    friend bool _PostConnect( IoService * serv, IoConnectCtx * ctx, eiennet::EndPoint const & ep );
    friend bool _PostRecv( IoService * serv, IoRecvCtx * ctx );
    friend bool _PostSend( IoService * serv, IoSendCtx * ctx );
    friend bool _PostRecvFrom( IoService * serv, IoRecvFromCtx * ctx );
    friend bool _PostSendTo( IoService * serv, IoSendToCtx * ctx );

    DISABLE_OBJECT_COPY(IoService)
};


} // namespace iocp


} // namespace io


#endif // __EIENNET_IO_IOCP_HPP__
