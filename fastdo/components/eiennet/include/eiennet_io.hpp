#ifndef __EIENNET_IO_HPP__
#define __EIENNET_IO_HPP__

namespace eiennet
{
class AsyncSocket;
class Timer;
}

/** \brief IO模型 */
namespace io
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

/** \brief 取消类型 */
enum CancelType
{
    cancelNone, //!< 不取消
    cancelProactive, //!< 主动取消
    cancelTimeout //!< 超时自动取消
};

/** \brief IO场景基类（提供引用计数功能） */
struct IoCtx
{
    IoType type; //!< IO类型
    CancelType cancelType; //!< 取消类型
    winux::uint64 startTime; //!< 请求开启的时间
    winux::uint64 timeoutMs; //!< 超时时间

protected:
    IoCtx() : type(ioNone), cancelType(cancelNone), startTime( winux::GetUtcTimeMs() ), timeoutMs(-1), _uses(1) { }
    virtual ~IoCtx() { }

public:
    /** \brief 增加引用计数 */
    void incRef()
    {
        _uses.fetch_add( 1, std::memory_order_acq_rel );
    }

    /** \brief 减少引用计数。当引用计数为0时销毁资源 */
    void decRef()
    {
        if ( _uses.fetch_sub( 1, std::memory_order_acq_rel ) == 1 )
        {
            std::atomic_thread_fence(std::memory_order_acquire);
            delete this;
        }
    }

    /** \brief 取消IO操作 */
    virtual bool cancel( CancelType cancelType ) = 0
    {
        return false;
    }

private:
    std::atomic<long> _uses; // 引用计数
};

struct IoTimerCtx;
/** \brief 套接字IO场景 */
struct IoSocketCtx : virtual IoCtx
{
    winux::SharedPointer<eiennet::AsyncSocket> sock; //!< 异步套接字
    IoTimerCtx * timerCtx;  //!< 超时场景

protected:
    IoSocketCtx() : timerCtx(nullptr) { }
    virtual ~IoSocketCtx() { }
};

/** \brief 接受场景接口 */
struct IoAcceptCtx : IoSocketCtx
{
    using OkFn = std::function< bool ( winux::SharedPointer<eiennet::AsyncSocket> servSock, winux::SharedPointer<eiennet::AsyncSocket> clientSock, eiennet::ip::EndPoint const & ep ) >;
    using TimeoutFn = std::function< bool ( winux::SharedPointer<eiennet::AsyncSocket> servSock, IoAcceptCtx * ctx ) >;

    OkFn cbOk; //!< 成功回调函数
    TimeoutFn cbTimeout; //!< 超时回调函数

    eiennet::ip::EndPoint clientEp; //!< 客户连接IP地址

protected:
    IoAcceptCtx()
    {
        this->type = ioAccept;
    }
    virtual ~IoAcceptCtx() { }
};

/** \brief 连接场景接口 */
struct IoConnectCtx : IoSocketCtx
{
    using OkFn = std::function< void ( winux::SharedPointer<eiennet::AsyncSocket> sock, winux::uint64 costTimeMs ) >;
    using TimeoutFn = std::function< void ( winux::SharedPointer<eiennet::AsyncSocket> sock, IoConnectCtx * ctx ) >;

    OkFn cbOk; //!< 成功回调函数
    TimeoutFn cbTimeout; //!< 超时回调函数

    winux::uint64 costTimeMs; //!< 总花费时间

protected:
    IoConnectCtx() : costTimeMs(0)
    {
        this->type = ioConnect;
    }
    virtual ~IoConnectCtx() { }
};

/** \brief 数据接收场景接口 */
struct IoRecvCtx : IoSocketCtx
{
    using OkFn = std::function< void ( winux::SharedPointer<eiennet::AsyncSocket> sock, winux::Buffer & data, bool cnnAvail ) >;
    using TimeoutFn = std::function< void ( winux::SharedPointer<eiennet::AsyncSocket> sock, IoRecvCtx * ctx ) >;

    OkFn cbOk; //!< 成功回调函数
    TimeoutFn cbTimeout; //!< 超时回调函数

    size_t hadBytes;        //!< 已接收数据量
    size_t targetBytes;     //!< 目标数据量
    winux::GrowBuffer data; //!< 已接收的数据
    bool cnnAvail; //!< 连接是否有效

protected:
    IoRecvCtx() : hadBytes(0), targetBytes(0), cnnAvail(false)
    {
        this->type = ioRecv;
    }
    virtual ~IoRecvCtx() { }
};

/** \brief 数据发送场景接口 */
struct IoSendCtx : IoSocketCtx
{
    using OkFn = std::function< void ( winux::SharedPointer<eiennet::AsyncSocket> sock, size_t hadBytes, winux::uint64 costTimeMs, bool cnnAvail ) >;
    using TimeoutFn = std::function< void ( winux::SharedPointer<eiennet::AsyncSocket> sock, IoSendCtx * ctx ) >;

    OkFn cbOk; //!< 成功回调函数
    TimeoutFn cbTimeout; //!< 超时回调函数

    size_t hadBytes;    //!< 已发送数据量
    winux::uint64 costTimeMs; //!< 总花费时间
    winux::Buffer data; //!< 待发送的数据
    bool cnnAvail; //!< 连接是否有效

protected:
    IoSendCtx() : hadBytes(0), costTimeMs(0), cnnAvail(false)
    {
        this->type = ioSend;
    }
    virtual ~IoSendCtx() { }
};

/** \brief 无连接，数据接收场景接口 */
struct IoRecvFromCtx : IoSocketCtx
{
    using OkFn = std::function< void ( winux::SharedPointer<eiennet::AsyncSocket> sock, winux::Buffer & data, eiennet::EndPoint const & ep ) >;
    using TimeoutFn = std::function< void ( winux::SharedPointer<eiennet::AsyncSocket> sock, IoRecvFromCtx * ctx ) >;

    OkFn cbOk; //!< 成功回调函数
    TimeoutFn cbTimeout; //!< 超时回调函数

    size_t hadBytes;        //!< 已接收数据量
    size_t targetBytes;     //!< 目标数据量
    winux::GrowBuffer data; //!< 已接收的数据
    eiennet::ip::EndPoint epFrom;    //!< 数据来自的EP

protected:
    IoRecvFromCtx() : hadBytes(0), targetBytes(0)
    {
        this->type = ioRecvFrom;
    }
    virtual ~IoRecvFromCtx() { }
};

/** \brief 无连接，数据发送场景接口 */
struct IoSendToCtx : IoSocketCtx
{
    using OkFn = std::function< void ( winux::SharedPointer<eiennet::AsyncSocket> sock, size_t hadBytes, winux::uint64 costTimeMs ) >;
    using TimeoutFn = std::function< void ( winux::SharedPointer<eiennet::AsyncSocket> sock, IoSendToCtx * ctx ) >;

    OkFn cbOk; //!< 成功回调函数
    TimeoutFn cbTimeout; //!< 超时回调函数

    size_t hadBytes;    //!< 已发送数据量
    winux::uint64 costTimeMs; //!< 总花费时间
    winux::Buffer data; //!< 待发送的数据
    winux::SimplePointer<eiennet::EndPoint> epTo; //!< 数据发送到的EP

protected:
    IoSendToCtx() : hadBytes(0), costTimeMs(0)
    {
        this->type = ioSendTo;
    }
    virtual ~IoSendToCtx() { }
};

/** \brief 定时器场景 */
struct IoTimerCtx : virtual IoCtx
{
    using OkFn = std::function< bool ( winux::SharedPointer<eiennet::Timer> timer, IoTimerCtx * ctx ) >;

    OkFn cbOk; //!< 回调函数

    winux::SharedPointer<eiennet::Timer> timer; //!< 定时器
    IoSocketCtx * assocCtx; //!< 关联的IO场景
    bool periodic; //!< 是否为周期的

protected:
    IoTimerCtx() : assocCtx(nullptr), periodic(false)
    {
        this->type = ioTimer;
    }

    virtual ~IoTimerCtx() { }
};

class IoServiceThread;
/** \brief IoService基类 */
class IoService
{
public:
    virtual ~IoService() { }

    virtual void stop() = 0;
    virtual int run() = 0;
 
    virtual void postAccept( winux::SharedPointer<eiennet::AsyncSocket> sock, IoAcceptCtx::OkFn cbOk, winux::uint64 timeoutMs = -1, IoAcceptCtx::TimeoutFn cbTimeout = nullptr ) = 0;
    virtual void postConnect( winux::SharedPointer<eiennet::AsyncSocket> sock, eiennet::EndPoint const & ep, IoConnectCtx::OkFn cbOk, winux::uint64 timeoutMs = -1, IoConnectCtx::TimeoutFn cbTimeout = nullptr ) = 0;
    virtual void postRecv( winux::SharedPointer<eiennet::AsyncSocket> sock, size_t targetSize, IoRecvCtx::OkFn cbOk, winux::uint64 timeoutMs = -1, IoRecvCtx::TimeoutFn cbTimeout = nullptr ) = 0;
    virtual void postSend( winux::SharedPointer<eiennet::AsyncSocket> sock, void const * data, size_t size, IoSendCtx::OkFn cbOk, winux::uint64 timeoutMs = -1, IoSendCtx::TimeoutFn cbTimeout = nullptr ) = 0;
    virtual void postRecvFrom( winux::SharedPointer<eiennet::AsyncSocket> sock, size_t targetSize, IoRecvFromCtx::OkFn cbOk, winux::uint64 timeoutMs = -1, IoRecvFromCtx::TimeoutFn cbTimeout = nullptr ) = 0;
    virtual void postSendTo( winux::SharedPointer<eiennet::AsyncSocket> sock, eiennet::EndPoint const & ep, void const * data, size_t size, IoSendToCtx::OkFn cbOk, winux::uint64 timeoutMs = -1, IoSendToCtx::TimeoutFn cbTimeout = nullptr ) = 0;
    virtual void postTimer( winux::SharedPointer<eiennet::Timer> timer, winux::uint64 timeoutMs, bool periodic, IoTimerCtx::OkFn cbOk, IoSocketCtx * assocCtx = nullptr, IoServiceThread * th = (IoServiceThread *)-1 ) = 0;
};


} // namespace io

#endif // __EIENNET_IO_HPP__
