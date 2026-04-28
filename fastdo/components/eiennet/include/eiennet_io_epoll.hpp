#ifndef __EIENNET_IO_EPOLL_HPP__
#define __EIENNET_IO_EPOLL_HPP__

/** \brief IO模型 */
namespace io
{
namespace epoll
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
class Epoll;

/** \brief IO事件数据：fd到IoCtxs的映射 */
class EIENNET_DLL IoEventsData
{
public:
    using IoMapMap = std::map< int, std::map< IoType, IoCtx * > >;
    using IoMap = IoMapMap::mapped_type;

    /** \brief 构造函数
     *
     *  \param epoll epoll实例 */
    IoEventsData( Epoll * epoll );

    /** \brief 设置与`sock/timer fd`相关的`IoCtx`
     *
     *  \param ioCtx IoCtx实例 */
    void setIoCtx( IoCtx * ioCtx );

    /** \brief 获取与`sock/timer fd`相关的指定类型的`IoCtx`
     *
     *  \param fd sock/timer fd
     *  \param type IO类型（ioAccept, ioConnect, ioRecv, ioSend, ioRecvFrom, ioSendTo, ioTimer）
     *  \param events 事件掩码指针
     *  \return IoCtx实例 */
    IoCtx * getIoCtx( int fd, IoType type, uint32_t * events = nullptr );

    /** \brief 删除与`sock/timer fd`相关的指定`IoCtx`
     *
     *  \param ioCtx IoCtx实例 */
    void delIoCtx( IoCtx * ioCtx );

    /** \brief 是否存在与`sock/timer fd`相关的`IoCtxs`
     *
     *  \param fd sock/timer fd
     *  \return 是否存在 */
    bool hasIoCtxs( int fd ) const;

    /** \brief 获取与`sock/timer fd`相关的所有`IoCtxs`
     *
     *  \param fd sock/timer fd
     *  \return IoCtx映射 */
    IoMap & getIoCtxs( int fd );

    /** \brief 删除与`sock/timer fd`相关的所有`IoCtxs`
     *
     *  \param fd sock/timer fd */
    void delIoCtxs( int fd );

    /** \brief 获取epoll实例 */
    Epoll & getEpoll() const { return *_epoll; }

private:
    IoMapMap _fdToIoCtxs; //!< fd到IoCtxs的映射
    std::map< int, uint32_t > _fdToEvents; //!< fd到epoll事件掩码的映射
    winux::Mutex _mtx; //!< 互斥锁
    Epoll * _epoll; //!< epoll实例

    friend void _EpollWorkerFunc( IoService * serv, IoServiceThread * thread, Epoll * epoll, bool * stop );
    friend class IoService;
};

/** \brief epoll封装 */
class EIENNET_DLL Epoll
{
public:
    /** \brief Epoll 构造函数
     * 
     *  \param maxEvents 单次wait()最大处理事件
     *  \param mts 是否多线程安全 */
    Epoll( size_t maxEvents = 64, bool mts = false );

    ~Epoll();

    /** \brief 添加fd到epoll
     *
     *  \param fd sock/timer fd
     *  \param events 事件类型（EPOLLET | EPOLLIN | EPOLLOUT | EPOLLERR）
     *  \param data 关联数据
     *  \return 成功返回0，失败返回-1 */
    int add( int fd, uint32_t events, void * data = nullptr );

    /** \brief 修改fd的epoll事件
     *
     *  \param fd sock/timer fd
     *  \param events 事件类型（EPOLLET | EPOLLIN | EPOLLOUT | EPOLLERR）
     *  \param data 关联数据
     *  \return 成功返回0，失败返回-1 */
    int mod( int fd, uint32_t events, void * data = nullptr );

    /** \brief 从epoll删除fd
     *
     *  \param fd sock/timer fd
     *  \return 成功返回0，失败返回-1 */
    int del( int fd );

    /** \brief 等待事件发生
     *
     *  \param timeout 超时时间（毫秒），-1表示无限等待
     *  \return 成功返回事件数量，失败返回-1 */
    int wait( int timeout = -1 );

    int get() const { return _epollFd; }

    size_t evtsCount() const;

    struct epoll_event * evts();

    struct epoll_event & evt( int i );

    winux::Mutex & getMutex() { return _mtx; }

private:
    int _epollFd;
    size_t _maxEvents;
    winux::Mutex _mtx;
    bool _mts;  // 是否多线程安全（加锁）
    std::vector<struct epoll_event> _evts; // 最大事件返回
};


/** \brief Io服务线程 */
class EIENNET_DLL IoServiceThread : public io::IoServiceThread
{
public:
    IoServiceThread( IoService * serv );

    virtual void run() override;

    virtual void timerTrigger( io::IoTimerCtx * timerCtx ) override;

private:
    Epoll _epoll;
    IoEventsData _ioEvents;
    winux::SimpleHandle<int> _stopEventFd; // 停止eventfd
    IoService * _serv;
    bool _stop;

    friend class IoService;
    friend void _EpollWorkerFunc( IoService * serv, IoServiceThread * thread, Epoll * epoll, bool * stop );

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
    void removeSock( winux::SharedPointer<eiennet::async::Socket> sock );

    /** \brief 关联线程
     *
     *  \param sock 异步套接字
     *  \param th 为空表示主线程，为-1表示自动分配，其他则为指定线程 */
    bool associate( winux::SharedPointer<eiennet::async::Socket> sock, io::IoServiceThread * th = (io::IoServiceThread *)-1 );

    /** \brief 获取最小负载线程 */
    virtual IoServiceThread * getMinWeightThread() const override;

    /** \brief 获取指定索引的组线程 */
    IoServiceThread * getGroupThread( size_t i ) const;

    /** \brief 获取组线程数 */
    size_t getGroupThreadCount() const { return _group.count(); }

private:
    Epoll _epoll;
    IoEventsData _ioEvents;
    winux::SimpleHandle<int> _stopEventFd; // 停止eventfd
    winux::ThreadGroup _group;
    bool _stop;

    friend void _EpollWorkerFunc( IoService * serv, IoServiceThread * thread, Epoll * epoll, bool * stop );

    DISABLE_OBJECT_COPY(IoService)
};

} // namespace epoll


} // namespace io


#endif // __EIENNET_IO_EPOLL_HPP__

