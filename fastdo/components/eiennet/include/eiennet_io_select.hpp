#ifndef __EIENNET_IO_SELECT_HPP__
#define __EIENNET_IO_SELECT_HPP__

#include "eiennet_io.hpp"

/** \brief IO模型 */
namespace io
{
using eiennet::Socket;

/** \brief SelectRead Io模型 */
class EIENNET_DLL SelectRead
{
public:
    SelectRead();
    SelectRead( Socket const & sock );
    SelectRead( int fd );
    SelectRead( winux::Mixed const & fds );
    ~SelectRead();

    SelectRead & setReadSock( Socket const & sock ) { return setReadFd( sock.get() ); }
    SelectRead & setReadFd( int fd );
    SelectRead & delReadFd( int fd );
    SelectRead & setReadFds( winux::Mixed const & fds );
    SelectRead & clear();
    int hasReadSock( Socket const & sock ) const { return hasReadFd( sock.get() ); }
    int hasReadFd( int fd ) const;

    /** \brief 等待相应的fd就绪。sec<1表示小于1秒的时间，sec<0表示无限等待。eg: sec=1.5表示等待1500ms
     *
     *  若有fd就绪则返回就绪的fd的总数；若超时则返回0；若有错误发生则返回SOCKET_ERROR(-1)。\n
     *  可用`Socket::ErrNo()`查看`select()`调用的错误，可用`Socket::getError()`查看`select()`无错时socket发生的错误。*/
    int wait( double sec = -1 );

protected:
    winux::Members<struct SelectRead_Data> _self;
    DISABLE_OBJECT_COPY(SelectRead)
};

/** \brief SelectWrite Io模型 */
class EIENNET_DLL SelectWrite
{
public:
    SelectWrite();
    SelectWrite( Socket const & sock );
    SelectWrite( int fd );
    SelectWrite( winux::Mixed const & fds );
    ~SelectWrite();

    SelectWrite & setWriteSock( Socket const & sock ) { return setWriteFd( sock.get() ); }
    SelectWrite & setWriteFd( int fd );
    SelectWrite & delWriteFd( int fd );
    SelectWrite & setWriteFds( winux::Mixed const & fds );
    SelectWrite & clear();
    int hasWriteSock( Socket const & sock ) const { return hasWriteFd( sock.get() ); }
    int hasWriteFd( int fd ) const;

    /** \brief 等待相应的fd就绪。sec<1表示小于1秒的时间，sec<0表示无限等待。eg: sec=1.5表示等待1500ms
     *
     *  若有fd就绪则返回就绪的fd的总数；若超时则返回0；若有错误发生则返回SOCKET_ERROR(-1)。\n
     *  可用`Socket::ErrNo()`查看`select()`调用的错误，可用`Socket::getError()`查看`select()`无错时socket发生的错误。*/
    int wait( double sec = -1 );

protected:
    winux::Members<struct SelectWrite_Data> _self;
    DISABLE_OBJECT_COPY(SelectWrite)
};

/** \brief SelectExcept Io模型 */
class EIENNET_DLL SelectExcept
{
public:
    SelectExcept();
    SelectExcept( Socket const & sock );
    SelectExcept( int fd );
    SelectExcept( winux::Mixed const & fds );
    ~SelectExcept();

    SelectExcept & setExceptSock( Socket const & sock ) { return setExceptFd( sock.get() ); }
    SelectExcept & setExceptFd( int fd );
    SelectExcept & delExceptFd( int fd );
    SelectExcept & setExceptFds( winux::Mixed const & fds );
    SelectExcept & clear();
    int hasExceptSock( Socket const & sock ) const { return hasExceptFd( sock.get() ); }
    int hasExceptFd( int fd ) const;

    /** \brief 等待相应的fd就绪。sec<1表示小于1秒的时间，sec<0表示无限等待。eg: sec=1.5表示等待1500ms
     *
     *  若有fd就绪则返回就绪的fd的总数；若超时则返回0；若有错误发生则返回SOCKET_ERROR(-1)。\n
     *  可用`Socket::ErrNo()`查看`select()`调用的错误，可用`Socket::getError()`查看`select()`无错时socket发生的错误。*/
    int wait( double sec = -1 );

protected:
    winux::Members<struct SelectExcept_Data> _self;
    DISABLE_OBJECT_COPY(SelectExcept)
};

/** \brief Select Io模型 */
class EIENNET_DLL Select : public SelectRead, public SelectWrite, public SelectExcept
{
public:
    /** \brief Select模型构造函数 */
    Select() { }

    Select & setReadSock( Socket const & sock ) { SelectRead::setReadSock(sock); return *this; }
    Select & setReadFd( int fd ) { SelectRead::setReadFd(fd); return *this; }
    Select & delReadFd( int fd ) { SelectRead::delReadFd(fd); return *this; }
    Select & setReadFds( winux::Mixed const & fds ) { SelectRead::setReadFds(fds); return *this; }
    Select & clearReadFds() { SelectRead::clear(); return *this; }

    Select & setWriteSock( Socket const & sock ) { SelectWrite::setWriteSock(sock); return *this; }
    Select & setWriteFd( int fd ) { SelectWrite::setWriteFd(fd); return *this; }
    Select & delWriteFd( int fd ) { SelectWrite::delWriteFd(fd); return *this; }
    Select & setWriteFds( winux::Mixed const & fds ) { SelectWrite::setWriteFds(fds); return *this; }
    Select & clearWriteFds() { SelectWrite::clear(); return *this; }

    Select & setExceptSock( Socket const & sock ) { SelectExcept::setExceptSock(sock); return *this; }
    Select & setExceptFd( int fd ) { SelectExcept::setExceptFd(fd); return *this; }
    Select & delExceptFd( int fd ) { SelectExcept::delExceptFd(fd); return *this; }
    Select & setExceptFds( winux::Mixed const & fds ) { SelectExcept::setExceptFds(fds); return *this; }
    Select & clearExceptFds() { SelectExcept::clear(); return *this; }

    /** \brief 清空所有fds */
    Select & clear() { SelectRead::clear(); SelectWrite::clear(); SelectExcept::clear(); return *this; }

    /** \brief 等待相应的fd就绪。sec<1表示小于1秒的时间，sec<0表示无限等待。eg: sec=1.5表示等待1500ms
     *
     *  若有fd就绪则返回就绪的fd的总数；若超时则返回0；若有错误发生则返回SOCKET_ERROR(-1)。\n
     *  可用`Socket::ErrNo()`查看`select()`调用的错误，可用`Socket::getError()`查看`select()`无错时socket发生的错误。*/
    int wait( double sec = -1 );
};


namespace select
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

/** \brief IO事件数据 */
class EIENNET_DLL IoEventsData
{
public:
    enum AsyncObjectType
    {
        aotSocket, //!< 套接字
        aotTimer //!< 定时器
    };
    enum WakeUpType
    {
        wutWantNone, //!< 无目的单纯唤醒
        wutWantStop, //!< 欲要停止，唤醒
        wutWantUpdate, //!< 欲要更新IO事件，唤醒以更新事件监听
    };
    struct IoKey
    {
        IoKey( void * ptr, AsyncObjectType type ) : ptr(ptr), type(type)
        {
        }

        bool operator < ( IoKey const & other ) const
        {
            return this->ptr < other.ptr;
        }

        void * ptr;
        AsyncObjectType type;
    };

    using IoMapMap = std::map< IoKey, std::map< IoType, winux::SharedPointer<IoCtx> > >;
    using IoMap = IoMapMap::mapped_type;

    IoEventsData();

    // 唤醒沉默的select()等待
    void wakeUpTrigger( WakeUpType type );

    // 预投递
    void prePost( IoType type, winux::SharedPointer<IoCtx> ctx );

    // 投递IO事件
    void post( IoType type, winux::SharedPointer<IoCtx> ctx );

    std::vector<IoMap::value_type> _preIoCtxs; //!< 预投递的IoCtx
    winux::RecursiveMutex _mtxPreIoCtxs; //!< 互斥量，保护PreIoCtxs数据

    IoMapMap _ioMaps; //!< 监听IO事件数据结构
    winux::RecursiveMutex _mtxIoMaps; //!< 互斥量，保护IoMaps数据

    eiennet::ip::udp::Socket _sockWakeUp; //!< UDP套接字，用于发送唤醒select()信号
    winux::ushort _portSockWakeUp; //!< 唤醒信号套接字端口

    size_t _sockIoCount; // 套接字IO数
    size_t _timerIoCount; // 定时器IO数
};

class IoService;
/** \brief Io服务线程 */
class EIENNET_DLL IoServiceThread : public io::IoServiceThread
{
public:
    IoServiceThread( IoService * serv ) : _serv(serv), _stop(false)
    {
    }

    virtual void run() override;

    virtual void timerTrigger( io::IoTimerCtx * timerCtx ) override;

    IoEventsData _ioEvents;

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

    virtual void postAccept( winux::SharedPointer<eiennet::async::Socket> sock, IoAcceptCtx::OkFn cbOk, winux::uint64 timeoutMs = -1, IoAcceptCtx::TimeoutFn cbTimeout = nullptr, io::IoServiceThread * th = nullptr ) override;
    virtual void postConnect( winux::SharedPointer<eiennet::async::Socket> sock, eiennet::EndPoint const & ep, IoConnectCtx::OkFn cbOk, winux::uint64 timeoutMs = -1, IoConnectCtx::TimeoutFn cbTimeout = nullptr, io::IoServiceThread * th = (io::IoServiceThread *)-1 ) override;
    virtual void postRecv( winux::SharedPointer<eiennet::async::Socket> sock, size_t targetSize, IoRecvCtx::OkFn cbOk, winux::uint64 timeoutMs = -1, IoRecvCtx::TimeoutFn cbTimeout = nullptr, io::IoServiceThread * th = (io::IoServiceThread *)-1 ) override;
    virtual void postSend( winux::SharedPointer<eiennet::async::Socket> sock, void const * data, size_t size, IoSendCtx::OkFn cbOk, winux::uint64 timeoutMs = -1, IoSendCtx::TimeoutFn cbTimeout = nullptr, io::IoServiceThread * th = (io::IoServiceThread *)-1 ) override;
    virtual void postRecvFrom( winux::SharedPointer<eiennet::async::Socket> sock, size_t targetSize, IoRecvFromCtx::OkFn cbOk, winux::uint64 timeoutMs = -1, IoRecvFromCtx::TimeoutFn cbTimeout = nullptr, io::IoServiceThread * th = (io::IoServiceThread *)-1 ) override;
    virtual void postSendTo( winux::SharedPointer<eiennet::async::Socket> sock, eiennet::EndPoint const & ep, void const * data, size_t size, IoSendToCtx::OkFn cbOk, winux::uint64 timeoutMs = -1, IoSendToCtx::TimeoutFn cbTimeout = nullptr, io::IoServiceThread * th = (io::IoServiceThread *)-1 ) override;
    virtual void postTimer( winux::SharedPointer<eiennet::async::Timer> timer, winux::uint64 timeoutMs, bool periodic, IoTimerCtx::OkFn cbOk, winux::SharedPointer<IoSocketCtx> assocCtx = winux::SharedPointer<IoSocketCtx>(), io::IoServiceThread * th = (io::IoServiceThread *)-1 ) override;

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

    IoEventsData _ioEvents;
    //winux::ThreadPool _pool;

private:
    winux::ThreadGroup _group;
    bool _stop;

    DISABLE_OBJECT_COPY(IoService)
};

} // namespace select


} // namespace io

#endif // __EIENNET_IO_SELECT_HPP__

