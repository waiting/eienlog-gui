#ifndef __EIENNET_IO_POLL_HPP__
#define __EIENNET_IO_POLL_HPP__

/** \brief IO模型 */
namespace io
{
struct Poll_Data;

/** \brief Poll IO模型 */
class EIENNET_DLL Poll
{
public:
    static short const PollIn;
    static short const PollPri;
    static short const PollOut;
    static short const PollErr;
    static short const PollHup;
    static short const PollNVal;

    /** \brief 枚举事件函数 */
    using EvtFn = std::function< void ( int fd, short events ) >;

    /** \brief 构造函数 */
    Poll();

    /** \brief 析构函数 */
    ~Poll();

    /** \brief 查找fd在poll集合中的位置
     *
     *  \param fd 文件描述符
     *  \return 位置 */
    size_t find( int fd ) const;

    /** \brief 添加事件 */
    bool add( int fd, short events );

    /** \brief 设置事件 */
    bool set( size_t i, int fd, short events );

    /** \brief 修改事件 */
    bool mod( int fd, short events )
    {
        return this->set( this->find(fd), fd, events );
    }

    /** \brief 删除事件 */
    bool del( int fd );

    /** \brief 清除全部事件 */
    void clear();

    /** \brief 等待 */
    int wait( double sec = -1 );

    /** \brief 枚举事件 */
    size_t enumEvents( EvtFn cbEvt );

private:
    winux::PlainMembers<struct Poll_Data, 32> _self;
    DISABLE_OBJECT_COPY(Poll);
};

/** \brief poll 模型 */
namespace poll
{
/** \brief 接受场景接口 */
struct IoAcceptCtx : io::IoAcceptCtx, winux::EnableStaticNew<IoAcceptCtx>
{
protected:
    IoAcceptCtx() { }
    virtual ~IoAcceptCtx() { }

    FRIEND_ENABLE_STATIC_NEW;
};

/** \brief 连接场景接口 */
struct IoConnectCtx : io::IoConnectCtx, winux::EnableStaticNew<IoConnectCtx>
{
protected:
    IoConnectCtx() { }
    virtual ~IoConnectCtx() { }

    FRIEND_ENABLE_STATIC_NEW;
};

/** \brief 数据接收场景接口 */
struct IoRecvCtx : io::IoRecvCtx, winux::EnableStaticNew<IoRecvCtx>
{
protected:
    IoRecvCtx() { }
    virtual ~IoRecvCtx() { }

    FRIEND_ENABLE_STATIC_NEW;
};

/** \brief 数据发送场景接口 */
struct IoSendCtx : io::IoSendCtx, winux::EnableStaticNew<IoSendCtx>
{
protected:
    IoSendCtx() { }
    virtual ~IoSendCtx() { }

    FRIEND_ENABLE_STATIC_NEW;
};

/** \brief 无连接，数据接收场景接口 */
struct IoRecvFromCtx : io::IoRecvFromCtx, winux::EnableStaticNew<IoRecvFromCtx>
{
protected:
    IoRecvFromCtx() { }
    virtual ~IoRecvFromCtx() { }

    FRIEND_ENABLE_STATIC_NEW;
};

/** \brief 无连接，数据发送场景接口 */
struct IoSendToCtx : io::IoSendToCtx, winux::EnableStaticNew<IoSendToCtx>
{
protected:
    IoSendToCtx() { }
    virtual ~IoSendToCtx() { }

    FRIEND_ENABLE_STATIC_NEW;
};

/** \brief 定时器IO场景 */
struct IoTimerCtx : io::IoTimerCtx, winux::EnableStaticNew<IoTimerCtx>
{
#if defined(OS_WIN)
    eiennet::ip::udp::Socket _sockSignal; //!< UDP套接字，用于发送定时信号的管道
    winux::ushort _portSockSignal; //!< 信号套接字端口
#else

#endif

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
    IoTimerCtx()
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
        short events; //!< 事件掩码

        IoVecStruct() : events(0) { }
    };

    using IoVecMap = std::map< int, IoVecStruct >;

    /** \brief 构造函数 */
    IoEventsData();

    /** \brief 唤醒沉默的poll()等待
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
    // 处理IoCtxs事件回调
    void _handleIoCtxsCallback( int rc );
    // 处理IoCtxs超时响应以及删除取消的IO
    void _handleIoCtxsTimeoutAndDelete();

    std::vector<IoCtx *> _preIoCtxs; //!< 预投递的IoCtx
    winux::Mutex _mtxPreIoCtxs; //!< 互斥量，保护PreIoCtxs数据

    IoVecMap _ioVecMap; //!< 监听IO事件数据结构
    winux::Mutex _mtxIoVecMap; //!< 互斥量，保护IoVecMap数据

    io::Poll _poll; //!< poll实例

    eiennet::ip::udp::Socket _sockWakeUp; //!< UDP套接字，用于发送唤醒poll()信号
    winux::ushort _portSockWakeUp; //!< 唤醒信号套接字端口

    size_t _sockIoCount; // 套接字IO数
    size_t _timerIoCount; // 定时器IO数

    friend void _PollWorkerFunc( IoService * serv, IoServiceThread * thread, IoEventsData & ioEvents, bool * stop );
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

    /** \brief 获取套接字IO数 */
    virtual size_t getSockIoCount() const override { return _ioEvents._sockIoCount; }
    /** \brief 获取定时器IO数 */
    virtual size_t getTimerIoCount() const override { return _ioEvents._timerIoCount; }

    /** \brief 关联线程
     *
     *  \param sock 异步套接字
     *  \param th 为空表示主线程，为-1表示自动分配，其他则为指定线程 */
    bool associate( winux::SharedPointer<eiennet::async::Socket> sock, io::IoServiceThread * th = (io::IoServiceThread *)-1 );

private:
    IoEventsData _ioEvents;
    bool _stop;

    DISABLE_OBJECT_COPY(IoService)
};


} // namespace poll


} // namespace io


#endif // __EIENNET_IO_POLL_HPP__
