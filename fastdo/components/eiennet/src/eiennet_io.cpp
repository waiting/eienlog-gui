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
#include "eiennet_io.hpp"

namespace io
{
// class IoServiceThread ----------------------------------------------------------------------
IoServiceThread::IoServiceThread() : _weight(0)
{

}

size_t IoServiceThread::getWeight() const
{
    return _weight.load(std::memory_order_acquire);
}

void IoServiceThread::incWeight()
{
    _weight.fetch_add( 1, std::memory_order_acq_rel );
}

void IoServiceThread::decWeight()
{
    _weight.fetch_sub( 1, std::memory_order_acq_rel );
}

void IoServiceThread::timerTrigger( IoTimerCtx * timerCtx )
{

}

// class IoService ----------------------------------------------------------------------------
void IoService::timerTrigger( IoTimerCtx * timerCtx )
{

}


} // namespace io
