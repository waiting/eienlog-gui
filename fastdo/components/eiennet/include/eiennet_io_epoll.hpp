#ifndef __EIENNET_IO_EPOLL_HPP__
#define __EIENNET_IO_EPOLL_HPP__

#include "eiennet_io.hpp"

/** \brief IO模型 */
namespace io
{
namespace epoll
{
/** \brief 接受场景接口 */
struct EIENNET_DLL IoAcceptCtx : io::IoAcceptCtx, winux::EnableStaticNew<IoAcceptCtx>
{
protected:
    IoAcceptCtx();
    virtual ~IoAcceptCtx();

    template < typename _Ty0 >
    friend class winux::EnableStaticNew;
};

/** \brief 连接场景接口 */
struct EIENNET_DLL IoConnectCtx : io::IoConnectCtx, winux::EnableStaticNew<IoConnectCtx>
{
protected:
    IoConnectCtx();
    virtual ~IoConnectCtx();

    template < typename _Ty0 >
    friend class winux::EnableStaticNew;
};

/** \brief 数据接收场景接口 */
struct EIENNET_DLL IoRecvCtx : io::IoRecvCtx, winux::EnableStaticNew<IoRecvCtx>
{
protected:
    IoRecvCtx();
    virtual ~IoRecvCtx();

    template < typename _Ty0 >
    friend class winux::EnableStaticNew;
};

/** \brief 数据发送场景接口 */
struct EIENNET_DLL IoSendCtx : io::IoSendCtx, winux::EnableStaticNew<IoSendCtx>
{
protected:
    IoSendCtx();
    virtual ~IoSendCtx();

    template < typename _Ty0 >
    friend class winux::EnableStaticNew;
};

/** \brief 无连接，数据接收场景接口 */
struct EIENNET_DLL IoRecvFromCtx : io::IoRecvFromCtx, winux::EnableStaticNew<IoRecvFromCtx>
{
protected:
    IoRecvFromCtx();
    virtual ~IoRecvFromCtx();

    template < typename _Ty0 >
    friend class winux::EnableStaticNew;
};

/** \brief 无连接，数据发送场景接口 */
struct EIENNET_DLL IoSendToCtx : io::IoSendToCtx, winux::EnableStaticNew<IoSendToCtx>
{
protected:
    IoSendToCtx();
    virtual ~IoSendToCtx();

    template < typename _Ty0 >
    friend class winux::EnableStaticNew;
};

/** \brief 定时器IO场景 */
struct EIENNET_DLL IoTimerCtx : io::IoTimerCtx, winux::EnableStaticNew<IoTimerCtx>
{
#if defined(OS_WIN)
    eiennet::ip::udp::Socket _sockSignal; //!< UDP套接字，用于发送定时信号的管道
    winux::ushort _portSockSignal; //!< 信号套接字端口
#else
#endif

    virtual bool cancel( CancelType cancelType ) override;

protected:
    IoTimerCtx();
    virtual ~IoTimerCtx();

    template < typename _Ty0 >
    friend class winux::EnableStaticNew;
};

class IoService;
class IoServiceThread;

/** \brief Io服务线程 */
class EIENNET_DLL IoServiceThread : public io::IoServiceThread
{
public:
    IoServiceThread( IoService * serv ) : _serv(serv), _stop(false)
    {
    }

    virtual void run() override;

    virtual void timerTrigger( io::IoTimerCtx * timerCtx ) override;

private:
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
        //winux::SharedPointer<IoSocketCtx> assocCtx = winux::SharedPointer<IoSocketCtx>(),
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
    //winux::ThreadPool _pool;
    winux::ThreadGroup _group;
    bool _stop;

    DISABLE_OBJECT_COPY(IoService)
};

} // namespace epoll


} // namespace io

#endif // __EIENNET_IO_EPOLL_HPP__

