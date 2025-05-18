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
#include "eiennet_io_select.hpp"

namespace io
{
// struct SelectRead_Data ---------------------------------------------------------------------
struct SelectRead_Data
{
    fd_set readFds;
    int readFdsCount;

    int maxFd;

    SelectRead_Data() { this->zeroInit(); }

    void zeroInit()
    {
        FD_ZERO(&this->readFds);
        this->readFdsCount = 0;

        this->maxFd = -2;
    }
};

// class SelectRead ---------------------------------------------------------------------------
SelectRead::SelectRead()
{
}

SelectRead::SelectRead( Socket const & sock )
{
    this->setReadSock(sock);
}

SelectRead::SelectRead( int fd )
{
    this->setReadFd(fd);
}

SelectRead::SelectRead( winux::Mixed const & fds )
{
    this->setReadFds(fds);
}

SelectRead::~SelectRead()
{
}

SelectRead & SelectRead::setReadFd( int fd )
{
    FD_SET( fd, &_self->readFds );
    if ( fd > _self->maxFd ) _self->maxFd = fd;
    _self->readFdsCount++;

    return *this;
}

SelectRead & SelectRead::delReadFd( int fd )
{
    FD_CLR( fd, &_self->readFds );
    _self->readFdsCount--;

    return *this;
}

SelectRead & SelectRead::setReadFds( winux::Mixed const & fds )
{
    if ( fds.isArray() )
    {
        size_t n = fds.getCount();
        for ( size_t i = 0; i < n; i++ )
        {
            this->setReadFd( fds[i].toInt() );
        }
    }
    else
    {
        this->setReadFd( fds.toInt() );
    }

    return *this;
}

SelectRead & SelectRead::clear()
{
    FD_ZERO(&_self->readFds);
    _self->maxFd = -2;
    _self->readFdsCount = 0;

    return *this;
}

int SelectRead::hasReadFd( int fd ) const
{
    return FD_ISSET( fd, &_self->readFds );
}

int SelectRead::wait( double sec )
{
    fd_set * pReadFds = NULL;

    if ( _self->readFdsCount > 0 ) pReadFds = &_self->readFds;

    if ( sec < 0 )
    {
        return ::select( _self->maxFd + 1, pReadFds, NULL, NULL, NULL );
    }
    else
    {
        long intsec = (long)sec;
        long microsec = (long)( ( sec - (double)intsec ) * 1e6 );
        struct timeval tv = { intsec, microsec };
        return ::select( _self->maxFd + 1, pReadFds, NULL, NULL, &tv );
    }
}

// struct SelectWrite_Data --------------------------------------------------------------------
struct SelectWrite_Data
{
    fd_set writeFds;
    int writeFdsCount;

    int maxFd;

    SelectWrite_Data() { this->zeroInit(); }

    void zeroInit()
    {
        FD_ZERO(&this->writeFds);
        this->writeFdsCount = 0;

        this->maxFd = -2;
    }
};

// class SelectWrite --------------------------------------------------------------------------
SelectWrite::SelectWrite()
{
}

SelectWrite::SelectWrite( Socket const & sock )
{
    this->setWriteSock(sock);
}

SelectWrite::SelectWrite( int fd )
{
    this->setWriteFd(fd);
}

SelectWrite::SelectWrite( winux::Mixed const & fds )
{
    this->setWriteFds(fds);
}

SelectWrite::~SelectWrite()
{
}

SelectWrite & SelectWrite::setWriteFd( int fd )
{
    FD_SET( fd, &_self->writeFds );
    if ( fd > _self->maxFd ) _self->maxFd = fd;
    _self->writeFdsCount++;

    return *this;
}

SelectWrite & SelectWrite::delWriteFd( int fd )
{
    FD_CLR( fd, &_self->writeFds );
    _self->writeFdsCount--;

    return *this;
}

SelectWrite & SelectWrite::setWriteFds( winux::Mixed const & fds )
{
    if ( fds.isArray() )
    {
        size_t n = fds.getCount();
        for ( size_t i = 0; i < n; i++ )
        {
            this->setWriteFd( fds[i].toInt() );
        }
    }
    else
    {
        this->setWriteFd( fds.toInt() );
    }

    return *this;
}

SelectWrite & SelectWrite::clear()
{
    FD_ZERO(&_self->writeFds);
    _self->maxFd = -2;
    _self->writeFdsCount = 0;

    return *this;
}

int SelectWrite::hasWriteFd( int fd ) const
{
    return FD_ISSET( fd, &_self->writeFds );
}

int SelectWrite::wait( double sec )
{
    fd_set * pWriteFds = NULL;

    if ( _self->writeFdsCount > 0 ) pWriteFds = &_self->writeFds;

    if ( sec < 0 )
    {
        return ::select( _self->maxFd + 1, NULL, pWriteFds, NULL, NULL );
    }
    else
    {
        long intsec = (long)sec;
        long microsec = (long)( ( sec - (double)intsec ) * 1e6 );
        struct timeval tv = { intsec, microsec };
        return ::select( _self->maxFd + 1, NULL, pWriteFds, NULL, &tv );
    }
}

// struct SelectExcept_Data -------------------------------------------------------------------
struct SelectExcept_Data
{
    fd_set exceptFds;
    int exceptFdsCount;

    int maxFd;

    SelectExcept_Data() { this->zeroInit(); }

    void zeroInit()
    {
        FD_ZERO(&this->exceptFds);
        this->exceptFdsCount = 0;

        this->maxFd = -2;
    }
};

// class SelectExcept -------------------------------------------------------------------------
SelectExcept::SelectExcept()
{
}

SelectExcept::SelectExcept( Socket const & sock )
{
    this->setExceptSock(sock);
}

SelectExcept::SelectExcept( int fd )
{
    this->setExceptFd(fd);
}

SelectExcept::SelectExcept( winux::Mixed const & fds )
{
    this->setExceptFds(fds);
}

SelectExcept::~SelectExcept()
{
}

SelectExcept & SelectExcept::setExceptFd( int fd )
{
    FD_SET( fd, &_self->exceptFds );
    if ( fd > _self->maxFd ) _self->maxFd = fd;
    _self->exceptFdsCount++;

    return *this;
}

SelectExcept & SelectExcept::delExceptFd( int fd )
{
    FD_CLR( fd, &_self->exceptFds );
    _self->exceptFdsCount--;

    return *this;
}

SelectExcept & SelectExcept::setExceptFds( winux::Mixed const & fds )
{
    if ( fds.isArray() )
    {
        size_t n = fds.getCount();
        for ( size_t i = 0; i < n; i++ )
        {
            this->setExceptFd( fds[i].toInt() );
        }
    }
    else
    {
        this->setExceptFd( fds.toInt() );
    }

    return *this;
}

SelectExcept & SelectExcept::clear()
{
    FD_ZERO(&_self->exceptFds);
    _self->maxFd = -2;
    _self->exceptFdsCount = 0;

    return *this;
}

int SelectExcept::hasExceptFd( int fd ) const
{
    return FD_ISSET( fd, &_self->exceptFds );
}

int SelectExcept::wait( double sec )
{
    fd_set * pExceptFds = NULL;

    if ( _self->exceptFdsCount > 0 ) pExceptFds = &_self->exceptFds;

    if ( sec < 0 )
    {
        return ::select( _self->maxFd + 1, NULL, NULL, pExceptFds, NULL );
    }
    else
    {
        long intsec = (long)sec;
        long microsec = (long)( ( sec - (double)intsec ) * 1e6 );
        struct timeval tv = { intsec, microsec };
        return ::select( _self->maxFd + 1, NULL, NULL, pExceptFds, &tv );
    }
}

// class Select -------------------------------------------------------------------------------
int Select::wait( double sec )
{
    fd_set * pReadFds = NULL;
    fd_set * pWriteFds = NULL;
    fd_set * pExceptFds = NULL;

    if ( SelectRead::_self->readFdsCount > 0 ) pReadFds = &SelectRead::_self->readFds;
    if ( SelectWrite::_self->writeFdsCount > 0 ) pWriteFds = &SelectWrite::_self->writeFds;
    if ( SelectExcept::_self->exceptFdsCount > 0 ) pExceptFds = &SelectExcept::_self->exceptFds;

    int maxFd;
    maxFd = ( SelectRead::_self->maxFd > SelectWrite::_self->maxFd ? SelectRead::_self->maxFd : SelectWrite::_self->maxFd );
    maxFd = ( SelectExcept::_self->maxFd > maxFd ? SelectExcept::_self->maxFd : maxFd );

    if ( sec < 0 )
    {
        return ::select( maxFd + 1, pReadFds, pWriteFds, pExceptFds, NULL );
    }
    else
    {
        long intsec = (long)sec;
        long microsec = (long)( ( sec - (double)intsec ) * 1e6 );
        struct timeval tv = { intsec, microsec };
        return ::select( maxFd + 1, pReadFds, pWriteFds, pExceptFds, &tv );
    }
}


} // namespace io
