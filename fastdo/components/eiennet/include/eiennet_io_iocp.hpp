#ifndef __EIENNET_IO_IOCP_HPP__
#define __EIENNET_IO_IOCP_HPP__

/** \brief IO模型 */
namespace io
{
namespace iocp
{
/** \brief IO场景 */
struct IoCtx : virtual io::IoCtx
{
    OVERLAPPED ol;

protected:
    IoCtx()
    {
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
        case stateFinish:
            if ( this->sock && this->sock->operator bool() )
            {
                if ( CancelIoEx( (HANDLE)(INT_PTR)this->sock->get(), &this->ol ) )
                {
                    return true;
                }
            }
            break;
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
        case stateFinish:
            if ( this->sock && this->sock->operator bool() )
            {
                if ( CancelIoEx( (HANDLE)(INT_PTR)this->sock->get(), &this->ol ) )
                {
                    return true;
                }
            }
            break;
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
        case stateFinish:
            if ( this->sock && this->sock->operator bool() )
            {
                if ( CancelIoEx( (HANDLE)(INT_PTR)this->sock->get(), &this->ol ) )
                {
                    return true;
                }
            }
            break;
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
        case stateFinish:
            if ( this->sock && this->sock->operator bool() )
            {
                if ( CancelIoEx( (HANDLE)(INT_PTR)this->sock->get(), &this->ol ) )
                {
                    return true;
                }
            }
            break;
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
        case stateFinish:
            if ( this->sock && this->sock->operator bool() )
            {
                if ( CancelIoEx( (HANDLE)(INT_PTR)this->sock->get(), &this->ol ) )
                {
                    return true;
                }
            }
            break;
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
        case stateFinish:
            if ( this->sock && this->sock->operator bool() )
            {
                if ( CancelIoEx( (HANDLE)(INT_PTR)this->sock->get(), &this->ol ) )
                {
                    return true;
                }
            }
            break;
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
        case stateFinish:
            if ( this->timer )
            {
                this->timer->unset();
                return true;
            }
            break;
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

/** \brief IOCP封装 */
class Iocp
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

    winux::PlainMembers< struct Iocp_Data, sizeof(HANDLE) * 5 > _self;
};


/** \brief Io服务线程 */
class EIENNET_DLL IoServiceThread : public io::IoServiceThread
{
public:
    IoServiceThread( IoService * serv ) : _serv(serv)
    {
        _iocp.initFuncs();
    }

    virtual void run() override;

    virtual void timerTrigger( io::IoTimerCtx * timerCtx ) override;

private:
    Iocp _iocp;
    IoService * _serv;
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
        io::IoServiceThread * th = AutoDispatch
    ) override;
    virtual void postRecv(
        winux::SharedPointer<eiennet::async::Socket> sock,
        size_t targetSize,
        IoRecvCtx::OkFn cbOk,
        winux::uint64 timeoutMs = -1,
        IoRecvCtx::TimeoutFn cbTimeout = nullptr,
        io::IoServiceThread * th = AutoDispatch
    ) override;
    virtual void postSend(
        winux::SharedPointer<eiennet::async::Socket> sock,
        void const * data,
        size_t size,
        IoSendCtx::OkFn cbOk,
        winux::uint64 timeoutMs = -1,
        IoSendCtx::TimeoutFn cbTimeout = nullptr,
        io::IoServiceThread * th = AutoDispatch
    ) override;
    virtual void postRecvFrom(
        winux::SharedPointer<eiennet::async::Socket> sock,
        size_t targetSize,
        IoRecvFromCtx::OkFn cbOk,
        winux::uint64 timeoutMs = -1,
        IoRecvFromCtx::TimeoutFn cbTimeout = nullptr,
        io::IoServiceThread * th = AutoDispatch
    ) override;
    virtual void postSendTo(
        winux::SharedPointer<eiennet::async::Socket> sock,
        eiennet::EndPoint const & ep,
        void const * data,
        size_t size,
        IoSendToCtx::OkFn cbOk,
        winux::uint64 timeoutMs = -1,
        IoSendToCtx::TimeoutFn cbTimeout = nullptr,
        io::IoServiceThread * th = AutoDispatch
    ) override;
    virtual void postTimer(
        winux::SharedPointer<eiennet::async::Timer> timer,
        winux::uint64 timeoutMs,
        bool periodic,
        IoTimerCtx::OkFn cbOk,
        IoSocketCtx * assocCtx = nullptr,
        io::IoServiceThread * th = AutoDispatch
    ) override;

    virtual void timerTrigger( io::IoTimerCtx * timerCtx ) override;

    /** \brief 标记删除指定sock所有IO监听 */
    void removeSock( winux::SharedPointer<eiennet::async::Socket> sock );

    /** \brief 关联线程
     *
     *  \param sock 异步套接字
     *  \param th 为空表示主线程，为-1表示自动分配，其他则为指定线程 */
    bool associate( winux::SharedPointer<eiennet::async::Socket> sock, io::IoServiceThread * th = AutoDispatch );

    /** \brief 获取最小负载线程 */
    virtual IoServiceThread * getMinWeightThread() const override;

    /** \brief 获取指定索引的组线程 */
    IoServiceThread * getGroupThread( size_t i ) const;

    /** \brief 获取组线程数 */
    size_t getGroupThreadCount() const { return _group.count(); }

private:
    Iocp _iocp;
    winux::ThreadGroup _group;

    DISABLE_OBJECT_COPY(IoService)
};


} // namespace iocp


} // namespace io

#endif // __EIENNET_IO_IOCP_HPP__

