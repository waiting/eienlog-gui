#ifndef __EIENNET_IO_HPP__
#define __EIENNET_IO_HPP__

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


} // namespace io

#endif // __EIENNET_IO_HPP__
