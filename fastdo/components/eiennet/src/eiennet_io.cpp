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
#include "eiennet_async.hpp"
#if defined(OS_WIN)
#include "eiennet_io_iocp.hpp"
#else
#include "eiennet_io_epoll.hpp"
#endif

namespace io
{
EIENNET_FUNC_IMPL(winux::SharedPointer<IoService>) IoService::New( size_t groupThread )
{
#if defined(OS_WIN)
    return winux::MakeSharedNew<io::iocp::IoService>(groupThread);
#else
    return winux::MakeSharedNew<io::epoll::IoService>(groupThread);
#endif
}


} // namespace io
