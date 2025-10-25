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
#include "eiennet_io_epoll.hpp"
#include "eiennet_async.hpp"

namespace io
{
namespace epoll
{
// struct IoAcceptCtx -------------------------------------------------------------------------
IoAcceptCtx::IoAcceptCtx()
{
    //ColorOutputLine( winux::fgGreen, "IoAcceptCtx()" );
}

IoAcceptCtx::~IoAcceptCtx()
{
    //ColorOutputLine( winux::fgAqua, "~IoAcceptCtx()" );
}

// struct IoConnectCtx ------------------------------------------------------------------------
IoConnectCtx::IoConnectCtx()
{
    //ColorOutputLine( winux::fgGreen, "IoConnectCtx()" );
}

IoConnectCtx::~IoConnectCtx()
{
    //ColorOutputLine( winux::fgAqua, "~IoConnectCtx()" );
}

// struct IoRecvCtx ---------------------------------------------------------------------------
IoRecvCtx::IoRecvCtx()
{
    //ColorOutputLine( winux::fgGreen, "IoRecvCtx(", this, ")"  );
}

IoRecvCtx::~IoRecvCtx()
{
    //ColorOutputLine( winux::fgAqua, "~IoRecvCtx(", this, "), sock:", this->sock->get() );
}

// struct IoSendCtx ---------------------------------------------------------------------------
IoSendCtx::IoSendCtx()
{
    //ColorOutputLine( winux::fgGreen, "IoSendCtx(", this, ")" );
}

IoSendCtx::~IoSendCtx()
{
    //ColorOutputLine( winux::fgAqua, "~IoSendCtx(", this, "), sock:", this->sock->get() );
}

// struct IoRecvFromCtx -----------------------------------------------------------------------
IoRecvFromCtx::IoRecvFromCtx()
{
    //ColorOutputLine( winux::fgGreen, "IoRecvFromCtx()" );
}

IoRecvFromCtx::~IoRecvFromCtx()
{
    //ColorOutputLine( winux::fgAqua, "~IoRecvFromCtx()" );
}

// struct IoSendToCtx -------------------------------------------------------------------------
IoSendToCtx::IoSendToCtx()
{
    //ColorOutputLine( winux::fgGreen, "IoSendToCtx()" );
}

IoSendToCtx::~IoSendToCtx()
{
    //ColorOutputLine( winux::fgAqua, "~IoSendToCtx()" );
}

// struct IoTimerCtx --------------------------------------------------------------------------
IoTimerCtx::IoTimerCtx()
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
    //ColorOutputLine( winux::fgGreen, "IoTimerCtx()" );
}

IoTimerCtx::~IoTimerCtx()
{
    //ColorOutputLine( winux::fgAqua, "~IoTimerCtx()" );
}

bool IoTimerCtx::cancel( CancelType cancelType )
{
    this->cancelType = cancelType;

    if ( this->timer )
    {
        this->timer->unset();
        return true;
    }
    return false;
}


// class IoServiceThread ----------------------------------------------------------------------
void IoServiceThread::run()
{

}

void IoServiceThread::timerTrigger( io::IoTimerCtx * timerCtx )
{
}


// class IoService ----------------------------------------------------------------------------
IoService::IoService( size_t groupThread ) : _stop(false)
{
    // 创建工作线程组
    this->_group.create<IoServiceThread>( groupThread, this );
    //this->_pool.startup(poolThread);
}

void IoService::stop()
{
    this->_stop = true;
    for ( size_t i = 0; i < _group.count(); i++ )
    {
        // 给每个线程投递退出信号
        auto * th = this->getGroupThread(i);
        th->_stop = true;
    }
}

int IoService::run()
{
    this->_group.startup();

    this->_group.wait();
    return 0;
}

void IoService::postAccept( winux::SharedPointer<eiennet::async::Socket> sock, IoAcceptCtx::OkFn cbOk, winux::uint64 timeoutMs, IoAcceptCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{

}

void IoService::postConnect( winux::SharedPointer<eiennet::async::Socket> sock, eiennet::EndPoint const & ep, IoConnectCtx::OkFn cbOk, winux::uint64 timeoutMs, IoConnectCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{

}

void IoService::postRecv( winux::SharedPointer<eiennet::async::Socket> sock, size_t targetSize, IoRecvCtx::OkFn cbOk, winux::uint64 timeoutMs, IoRecvCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{

}

void IoService::postSend( winux::SharedPointer<eiennet::async::Socket> sock, void const * data, size_t size, IoSendCtx::OkFn cbOk, winux::uint64 timeoutMs, IoSendCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{

}

void IoService::postRecvFrom( winux::SharedPointer<eiennet::async::Socket> sock, size_t targetSize, IoRecvFromCtx::OkFn cbOk, winux::uint64 timeoutMs, IoRecvFromCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{

}

void IoService::postSendTo( winux::SharedPointer<eiennet::async::Socket> sock, eiennet::EndPoint const & ep, void const * data, size_t size, IoSendToCtx::OkFn cbOk, winux::uint64 timeoutMs, IoSendToCtx::TimeoutFn cbTimeout, io::IoServiceThread * th )
{

}

void IoService::postTimer( winux::SharedPointer<eiennet::async::Timer> timer, winux::uint64 timeoutMs, bool periodic, IoTimerCtx::OkFn cbOk, io::IoServiceThread * th )
{

}

void IoService::timerTrigger( io::IoTimerCtx * timerCtx )
{
}

IoServiceThread * IoService::getMinWeightThread() const
{
    if ( _group.count() > 0 )
    {
        auto th0 = this->getGroupThread(0);
        for ( size_t i = 1; i < _group.count(); i++ )
        {
            auto th = this->getGroupThread(i);
            if ( th->getWeight() < th0->getWeight() )
            {
                th0 = th;
            }
        }
        return th0;
    }
    return nullptr;
}

IoServiceThread * IoService::getGroupThread( size_t i ) const
{
    return static_cast<IoServiceThread *>( _group.threadAt(i).get() );
}


} // namespace epoll


} // namespace io
