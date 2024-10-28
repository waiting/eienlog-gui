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

#include <algorithm>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include "eiennet_base.hpp"
#include "eiennet_socket.hpp"

namespace eiennet
{

#if defined(OS_WIN)

static constexpr int __addrFamilies[] = {
    AF_UNSPEC,
    AF_UNIX,            /* local to host (pipes, portals). */
    AF_INET,            /* internetwork: UDP, TCP, etc. */
    0,
    AF_IPX,             /* IPX protocols: IPX, SPX, etc. */
    AF_APPLETALK,       /* AppleTalk */
    0,
    0,
    0,
    0,                  /* Reserved for X.25 project.  */
    AF_INET6,           /* IP version 6.  */
    0,
    AF_DECnet,          /* Reserved for DECnet project.  */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    AF_ATM,             /* Native ATM Services. */
    0,
    AF_SNA,             /* Linux SNA Project */
    AF_IRDA,            /* IRDA sockets.  */
    0,
    0,
    0,
    0,
    AF_NETDES,          /* Network Designers OSI & gateway. */
    0,
    0,
    AF_BTH,             /* Bluetooth sockets.  */
    0,
    0,                  /* RxRPC sockets.  */
    0,                  /* mISDN sockets.  */
    0,                  /* Phonet sockets.  */
    0,                  /* IEEE 802.15.4 sockets.  */
    AF_MAX              /* For now..  */
};

static constexpr int __sockTypes[] = {
    0,
    SOCK_STREAM,    /* Sequenced, reliable, connection-based byte streams. */
    SOCK_DGRAM,     /* Connectionless, unreliable datagrams of fixed maximum length.  */
    SOCK_RAW,       /* Raw protocol interface.  */
    SOCK_RDM,       /* Reliably-delivered messages.  */
    SOCK_SEQPACKET, /* Sequenced, reliable, connection-based, datagrams of fixed maximum length.  */

    6,              /* Datagram Congestion Control Protocol.  */
    10,             /* Linux specific way of getting packets at the dev level.  For writing rarp and other similar things on the user level. */

    /* Flags to be ORed into the type parameter of socket and socketpair and used for the flags parameter of paccept.  */
    02000000,       /* Atomically set close-on-exec flag for the new descriptor(s). */
    04000           /* Atomically mark descriptor(s) as non-blocking.  */
};

static constexpr int __protocols[] = {
    IPPROTO_IP,     /* Dummy protocol for TCP.  */
    IPPROTO_ICMP,   /* Internet Control Message Protocol.  */
    IPPROTO_IGMP,   /* Internet Group Management Protocol. */
    IPPROTO_IPV4,   /* IPIP tunnels (older KA9Q tunnels use 94).  */
    IPPROTO_TCP,    /* Transmission Control Protocol.  */
    IPPROTO_EGP,    /* Exterior Gateway Protocol.  */
    IPPROTO_PUP,    /* PUP protocol.  */
    IPPROTO_UDP,    /* User Datagram Protocol.  */
    IPPROTO_IDP,    /* XNS IDP protocol.  */
    29,             /* SO Transport Protocol Class 4.  */
    33,             /* Datagram Congestion Control Protocol.  */
    IPPROTO_IPV6,   /* IPv6 header.  */
    46,             /* Reservation Protocol.  */
    47,             /* General Routing Encapsulation.  */
    IPPROTO_ESP,    /* encapsulating security payload.  */
    IPPROTO_AH,     /* authentication header.  */
    92,             /* Multicast Transport Protocol.  */
    94,             /* IP option pseudo header for BEET.  */
    98,             /* Encapsulation Header.  */
    IPPROTO_PIM,    /* Protocol Independent Multicast.  */
    108,            /* Compression Header Protocol.  */
    IPPROTO_SCTP,   /* Stream Control Transmission Protocol.  */
    136,            /* UDP-Lite protocol.  */
    IPPROTO_RAW,    /* Raw IP packets.  */
    IPPROTO_MAX
};

#else

static constexpr int __addrFamilies[] = {
    AF_UNSPEC,
    AF_LOCAL&AF_UNIX&AF_FILE,   /* Local to host (pipes and file-domain). , POSIX name for PF_LOCAL. , Another non-standard name for PF_LOCAL. */
    AF_INET,                    /* IP protocol family.  */
    AF_AX25,                    /* Amateur Radio AX.25.  */
    AF_IPX,                     /* Novell Internet Protocol.  */
    AF_APPLETALK,               /* Appletalk DDP.  */
    AF_NETROM,                  /* Amateur radio NetROM.  */
    AF_BRIDGE,                  /* Multiprotocol bridge.  */
    AF_ATMPVC,                  /* ATM PVCs.  */
    AF_X25,                     /* Reserved for X.25 project.  */
    AF_INET6,                   /* IP version 6.  */
    AF_ROSE,                    /* Amateur Radio X.25 PLP.  */
    AF_DECnet,                  /* Reserved for DECnet project.  */
    AF_NETBEUI,                 /* Reserved for 802.2LLC project.  */
    AF_SECURITY,                /* Security callback pseudo AF.  */
    AF_KEY,                     /* PF_KEY key management API.  */
    AF_NETLINK&AF_ROUTE,        /* Alias to emulate 4.4BSD.  */
    AF_PACKET,                  /* Packet family.  */
    AF_ASH,                     /* Ash.  */
    AF_ECONET,                  /* Acorn Econet.  */
    AF_ATMSVC,                  /* ATM SVCs.  */
    AF_RDS,                     /* RDS sockets.  */
    AF_SNA,                     /* Linux SNA Project */
    AF_IRDA,                    /* IRDA sockets.  */
    AF_PPPOX,                   /* PPPoX sockets.  */
    AF_WANPIPE,                 /* Wanpipe API sockets.  */
    AF_LLC,                     /* Linux LLC.  */
    27,
    28,
    AF_CAN,                     /* Controller Area Network.  */
    AF_TIPC,                    /* TIPC sockets.  */
    AF_BLUETOOTH,               /* Bluetooth sockets.  */
    AF_IUCV,                    /* IUCV sockets.  */
    AF_RXRPC,                   /* RxRPC sockets.  */
    AF_ISDN,                    /* mISDN sockets.  */
    AF_PHONET,                  /* Phonet sockets.  */
    AF_IEEE802154,              /* IEEE 802.15.4 sockets.  */
    AF_MAX                      /* For now..  */
};

static constexpr int __sockTypes[] = {
    0,
    SOCK_STREAM,    /* Sequenced, reliable, connection-based byte streams. */
    SOCK_DGRAM,     /* Connectionless, unreliable datagrams of fixed maximum length.  */
    SOCK_RAW,       /* Raw protocol interface.  */
    SOCK_RDM,       /* Reliably-delivered messages.  */
    SOCK_SEQPACKET, /* Sequenced, reliable, connection-based, datagrams of fixed maximum length.  */

    SOCK_DCCP,      /* Datagram Congestion Control Protocol.  */
    SOCK_PACKET,    /* Linux specific way of getting packets at the dev level.  For writing rarp and
                        other similar things on the user level. */

    /* Flags to be ORed into the type parameter of socket and socketpair and used for the flags parameter of paccept.  */
    SOCK_CLOEXEC,   /* Atomically set close-on-exec flag for the new descriptor(s). */
    SOCK_NONBLOCK   /* Atomically mark descriptor(s) as non-blocking.  */
};

static constexpr int __protocols[] = {
    IPPROTO_IP,     /* Dummy protocol for TCP.  */
    IPPROTO_ICMP,   /* Internet Control Message Protocol.  */
    IPPROTO_IGMP,   /* Internet Group Management Protocol. */
    IPPROTO_IPIP,   /* IPIP tunnels (older KA9Q tunnels use 94).  */
    IPPROTO_TCP,    /* Transmission Control Protocol.  */
    IPPROTO_EGP,    /* Exterior Gateway Protocol.  */
    IPPROTO_PUP,    /* PUP protocol.  */
    IPPROTO_UDP,    /* User Datagram Protocol.  */
    IPPROTO_IDP,    /* XNS IDP protocol.  */
    IPPROTO_TP,     /* SO Transport Protocol Class 4.  */
    IPPROTO_DCCP,   /* Datagram Congestion Control Protocol.  */
    IPPROTO_IPV6,   /* IPv6 header.  */
    IPPROTO_RSVP,   /* Reservation Protocol.  */
    IPPROTO_GRE,    /* General Routing Encapsulation.  */
    IPPROTO_ESP,    /* encapsulating security payload.  */
    IPPROTO_AH,     /* authentication header.  */
    IPPROTO_MTP,    /* Multicast Transport Protocol.  */
    #ifdef IPPROTO_BEETPH
    IPPROTO_BEETPH, /* IP option pseudo header for BEET.  */
    #else
    94,
    #endif
    IPPROTO_ENCAP,  /* Encapsulation Header.  */
    IPPROTO_PIM,    /* Protocol Independent Multicast.  */
    IPPROTO_COMP,   /* Compression Header Protocol.  */
    IPPROTO_SCTP,   /* Stream Control Transmission Protocol.  */
    IPPROTO_UDPLITE,/* UDP-Lite protocol.  */
    IPPROTO_RAW,    /* Raw IP packets.  */
    IPPROTO_MAX
};

#endif

// union UnionAddr ------------------------------------------------------------------------
union UnionAddr
{
    sockaddr_in6 addrInet6;    // IPv6的socket地址结构.
    sockaddr_in addrInet;      // IPv4的socket地址结构.
    sockaddr addr;             // socket地址结构.
};

// struct SocketLib_Data ------------------------------------------------------------------
struct SocketLib_Data
{
#if defined(OS_WIN)
    WSADATA wsa;
#else

#endif
};

// class SocketLib ------------------------------------------------------------------------
SocketLib::SocketLib()
{
    _self.create(); //

#if defined(OS_WIN)
    WSAStartup( MAKEWORD( 2, 0 ), &_self->wsa );
#else
    // 在Linux下写socket的程序的时候，如果尝试send到一个已断开的socket上，就会让底层抛出一个SIGPIPE信号。
    // 这个信号的缺省处理方法是退出进程，大多数时候这都不是我们期望的。因此我们需要重载这个信号的处理方法。调用以下代码，即可安全的屏蔽SIGPIPE。
    signal( SIGPIPE, SIG_IGN );
#endif
}

SocketLib::~SocketLib()
{
#if defined(OS_WIN)
    WSACleanup();
#else
    signal( SIGPIPE, SIG_DFL );
#endif

    _self.destroy(); //
}

// struct Socket_Data ---------------------------------------------------------------------
struct Socket_Data
{
    // 延迟创建socket使用参数
    Socket::AddrFamily addrFamily;  // 地址族
    Socket::SockType sockType;      // 套接字类型
    Socket::Protocol protocol;      // 协议

    // 延迟设置socket属性
    bool _attrBlocking;     // 是否阻塞
    bool _attrBroadcast;    // 是否启用广播
    bool _attrReUseAddr;    // 是否开启了地址重用
    winux::uint32 _attrSendTimeout; // 发送超时(ms)
    winux::uint32 _attrRecvTimeout; // 接收超时(ms)
    int _attrSendBufSize;   // 发送缓冲区大小
    int _attrRecvBufSize;   // 接收缓冲区大小
    bool _attrIpv6Only;     // IPV6套接字只开启IPV6功能

    // 属性种类
    enum AttrCategory
    {
        attrNone,           // 无意义
        attrBlocking,       // 是否阻塞
        attrBroadcast,      // 是否启用广播
        attrReUseAddr,      // 是否开启了地址重用
        attrSendTimeout,    // 发送超时(ms)
        attrRecvTimeout,    // 接收超时(ms)
        attrSendBufSize,    // 发送缓冲区大小
        attrRecvBufSize,    // 接收缓冲区大小
        attrIpv6Only,       // IPV6套接字只开启IPV6功能
    };
    std::vector<AttrCategory> _attrExecSets; // 执行属性设置的操作集

    // socket资源管理
    int sock;           // socket描述符
    bool isNewSock;     // 指示是否为新建socket。如果为true，则会在Socket对象析构时自动关闭sock

    Socket_Data() { this->zeroInit(); }

    // 初始化全部成员
    void zeroInit()
    {
        this->addrFamily = Socket::afUnspec;
        this->sockType = Socket::sockUnknown;
        this->protocol = Socket::protoUnspec;

        _attrBlocking = true;
        _attrBroadcast = false;
        _attrReUseAddr = false;
        _attrSendTimeout = 0U;
        _attrRecvTimeout = 0U;
        _attrSendBufSize = 0;
        _attrRecvBufSize = 0;
    #if defined(OS_WIN)
        _attrIpv6Only = true;   // IPV6套接字只开启IPV6功能
    #else
        _attrIpv6Only = false;  // IPV6套接字只开启IPV6功能
    #endif

        this->sock = (int)INVALID_SOCKET;
        this->isNewSock = false;
    }

    // 重置socket资源管理相关变量
    void reset()
    {
        this->sock = (int)INVALID_SOCKET;
        this->isNewSock = false;
    }
};

// class Socket ---------------------------------------------------------------------------
int const Socket::MsgDefault = 0;

#if defined(OS_WIN)
int const Socket::MsgOob = MSG_OOB;
int const Socket::MsgPeek = MSG_PEEK;
int const Socket::MsgDontRoute = MSG_DONTROUTE;
int const Socket::MsgWaitAll = MSG_WAITALL;
int const Socket::MsgPartial = MSG_PARTIAL;
int const Socket::MsgInterrupt = MSG_INTERRUPT;
int const Socket::MsgMaxIovLen = MSG_MAXIOVLEN;

int const Socket::SdReceive = SD_RECEIVE;
int const Socket::SdSend = SD_SEND;
int const Socket::SdBoth = SD_BOTH;

#else
int const Socket::MsgOob = MSG_OOB;                     /* Process out-of-band data.  */
int const Socket::MsgPeek = MSG_PEEK;                   /* Peek at incoming messages.  */
int const Socket::MsgDontRoute = MSG_DONTROUTE;         /* Don't use local routing.  */
#   ifdef __USE_GNU
/* DECnet uses a different name.  */
int const Socket::MsgTryHard = MSG_TRYHARD;
#   endif
int const Socket::MsgCTrunc = MSG_CTRUNC;               /* Control data lost before delivery.  */
int const Socket::MsgProxy = MSG_PROXY;                 /* Supply or ask second address.  */
int const Socket::MsgTrunc = MSG_TRUNC;
int const Socket::MsgDontWait = MSG_DONTWAIT;           /* Nonblocking IO.  */
int const Socket::MsgEor = MSG_EOR;                     /* End of record.  */
int const Socket::MsgWaitAll = MSG_WAITALL;             /* Wait for a full request.  */
int const Socket::MsgFin = MSG_FIN;
int const Socket::MsgSyn = MSG_SYN;
int const Socket::MsgConfirm = MSG_CONFIRM;             /* Confirm path validity.  */
int const Socket::MsgRst = MSG_RST;
int const Socket::MsgErrQueue = MSG_ERRQUEUE;           /* Fetch message from error queue.  */
int const Socket::MsgNoSignal = MSG_NOSIGNAL;           /* Do not generate SIGPIPE.  */
int const Socket::MsgMore = MSG_MORE;                   /* Sender will send more.  */
int const Socket::MsgWaitForOne = MSG_WAITFORONE;       /* Wait for at least one packet to return.*/

int const Socket::MsgCMsgCloexec = MSG_CMSG_CLOEXEC;    /* Set close_on_exit for file descriptor received through SCM_RIGHTS.  */

int const Socket::SdReceive = SHUT_RD;
int const Socket::SdSend = SHUT_WR;
int const Socket::SdBoth = SHUT_RDWR;

#endif

Socket::Socket( int sock, bool isNewSock )
{
    _self.create(); //

    _self->sock = sock;
    _self->isNewSock = isNewSock;
}

Socket::Socket( AddrFamily af, SockType sockType, Protocol proto )
{
    _self.create(); //

    this->setParams( af, sockType, proto );
}

#ifndef MOVE_SEMANTICS_DISABLED
Socket::Socket( Socket && other )
{
    //_self.create(); // 无需创建成员数据

    _self = std::move(other._self);
}

Socket & Socket::operator = ( Socket && other )
{
    if ( this != &other )
    {
        this->close();

        _self = std::move(other._self);
    }
    return *this;
}
#endif

Socket::~Socket()
{
    this->close();

    _self.destroy(); //
}

Socket::AddrFamily Socket::getAddrFamily() const
{
    return _self->addrFamily;
}

void Socket::setAddrFamily( AddrFamily af )
{
    _self->addrFamily = af;
}

Socket::SockType Socket::getSockType() const
{
    return _self->sockType;
}

void Socket::setSockType( SockType sockType )
{
    _self->sockType = sockType;
}

Socket::Protocol Socket::getProtocol() const
{
    return _self->protocol;
}

void Socket::setProtocol( Protocol proto )
{
    _self->protocol = proto;
}

void Socket::getParams( AddrFamily * af, SockType * sockType, Protocol * proto )
{
    if ( af )
        *af = _self->addrFamily;

    if ( sockType )
        *sockType = _self->sockType;

    if ( proto )
        *proto = _self->protocol;
}

void Socket::setParams( AddrFamily af, SockType sockType, Protocol proto )
{
    _self->addrFamily = af;
    _self->sockType = sockType;
    _self->protocol = proto;
}

bool Socket::create( AddrFamily af, SockType sockType, Protocol proto )
{
    this->setParams( af, sockType, proto );

    return this->create();
}

bool Socket::create()
{
    this->close();

    _self->sock = (int)socket( __addrFamilies[_self->addrFamily], __sockTypes[_self->sockType], __protocols[_self->protocol] );
    _self->isNewSock = true;

    if ( _self->sock < 0 )
    {
        _self->isNewSock = false;
    #if defined(SOCKET_EXCEPTION_USE)
        int err = socket_errno;
        throw SocketError( err, winux::FormatA( "An error occurred while creating the socket(af:%d,type:%d,proto:%d).", __addrFamilies[_self->addrFamily], __sockTypes[_self->sockType], __protocols[_self->protocol] ) );
    #else
        return false;
    #endif
    }

    // 延迟设置socket属性
    for ( Socket_Data::AttrCategory attrFlag : _self->_attrExecSets )
    {
        switch ( attrFlag )
        {
        case Socket_Data::attrBlocking:
            if ( !this->setBlocking(_self->_attrBlocking) )
                return false;
            break;
        case Socket_Data::attrBroadcast:
            if ( !this->setBroadcast(_self->_attrBroadcast) )
                return false;
            break;
        case Socket_Data::attrReUseAddr:
            if ( !this->setReUseAddr(_self->_attrReUseAddr) )
                return false;
            break;
        case Socket_Data::attrSendTimeout:
            if ( !this->setSendTimeout(_self->_attrSendTimeout) )
                return false;
            break;
        case Socket_Data::attrRecvTimeout:
            if ( !this->setRecvTimeout(_self->_attrRecvTimeout) )
                return false;
            break;
        case Socket_Data::attrSendBufSize:
            if ( !this->setSendBufSize(_self->_attrSendBufSize) )
                return false;
            break;
        case Socket_Data::attrRecvBufSize:
            if ( !this->setRecvBufSize(_self->_attrRecvBufSize) )
                return false;
            break;
        case Socket_Data::attrIpv6Only:
            if ( !this->setIpv6Only(_self->_attrIpv6Only) )
                return false;
        }
    }

    return true;
}

bool Socket::_tryCreate( AddrFamily af, bool hasAf, SockType sockType, bool hasSockType, Protocol proto, bool hasProto )
{
    if ( _self->sock == -1 ) // 如果套接字还没创建，则创建
    {
        if ( hasAf ) _self->addrFamily = af;
        if ( hasSockType ) _self->sockType = sockType;
        if ( hasProto ) _self->protocol = proto;

        return this->create();
    }
    return true;
}

int Socket::close()
{
    int rc = 0;
    if ( _self && _self->isNewSock )
    {
        rc = closesocket(_self->sock);
        _self->reset();
    }
    return rc;
}

int Socket::shutdown( int how )
{
    return ::shutdown( _self->sock, how );
}

int Socket::send( void const * data, size_t size, int msgFlags )
{
    int sentBytes = ::send( _self->sock, (char const *)data, (int)size, msgFlags );

#if defined(SOCKET_EXCEPTION_USE)
    int err = socket_errno;
    if ( sentBytes < 0 || sentBytes == 0 && err != 0 )
    {
        throw SocketError( err, winux::FormatA( "An error occurred while sending the data(data:%p,size:%u).", data, size ) );
    }
#endif

    return sentBytes;
}

bool Socket::sendUntil( size_t targetSize, void const * data, int msgFlags )
{
    size_t hadSent = 0;
    while ( hadSent < targetSize )
    {
        int oneSent = this->send( (winux::byte *)data + hadSent, targetSize - hadSent, msgFlags );
        if ( oneSent <= 0 ) // 连接可能关闭或者出错，发送数据失败，返回false。
            return false;
        hadSent += oneSent;
    }
    return true;
}

int Socket::sendWaitUntil( size_t targetSize, void const * data, size_t * hadSent, double sec, int * rcWait, FunctionSuccessCallback eachSuccessCallback, void * param, int msgFlags )
{
    int oneSent = 0;
    while ( *hadSent < targetSize )
    {
        *rcWait = io::SelectWrite(_self->sock).wait(sec);
        if ( *rcWait > 0 )
        {
            oneSent = this->send( (winux::byte*)data + *hadSent, targetSize - *hadSent, msgFlags );
            if ( oneSent > 0 )
            {
                *hadSent += oneSent;
                if ( eachSuccessCallback ) eachSuccessCallback( oneSent, param );
            }
            else
                break; // 连接可能出错，发送数据失败
        }
        else if ( *rcWait == 0 )
        {
            break;
        }
        else
        {
            oneSent = -1;
            break;
        }
    }
    return oneSent;
}

int Socket::recv( void * buf, size_t size, int msgFlags )
{
    int recvBytes = ::recv( _self->sock, (char *)buf, (int)size, msgFlags );

#if defined(SOCKET_EXCEPTION_USE)
    int err = socket_errno;
    if ( recvBytes < 0 || recvBytes == 0 && err != 0 )
    {
        throw SocketError( err, winux::FormatA( "An error occurred while sock(%d) receiving data(buf:%p,size:%u).", _self->sock, buf, size ) );
    }
#endif

    return recvBytes;
}

winux::Buffer Socket::recv( size_t size, int msgFlags )
{
    winux::Buffer buf;
    buf.alloc( size, false );

    int recvSize = this->recv( buf.getBuf(), buf.getCapacity(), msgFlags );

    if ( recvSize >= 0 )
        buf._setSize(recvSize);
    else // recvSize < 0, recv() occur an error.
        buf.free();

    return buf;
}

bool Socket::recvUntilTarget( winux::AnsiString const & target, winux::GrowBuffer * data, winux::GrowBuffer * extraData, int msgFlags )
{
    auto targetNextVal = winux::_Templ_KmpCalcNext<short>( target.c_str(), target.size() );
    // 起始搜索位置
    size_t startpos = 0, pos = -1;
    while ( data->getSize() - startpos < target.size() || ( pos = winux::_Templ_KmpMatchEx( data->getBuf<char>(), data->getSize(), target.c_str(), target.size(), startpos, targetNextVal ) ) == -1 )
    {
        if ( data->getSize() >= target.size() ) startpos = data->getSize() - target.size() + 1; // 计算下次搜索起始

        char buf[4096];
        int oneRead = 0;
        oneRead = this->recv( buf, 4096, msgFlags );
        if ( oneRead > 0 )
            data->append( buf, oneRead );
        else if ( oneRead == 0 ) // 连接关闭
            break;
        else // 连接出错
            break;
    }
    if ( pos == -1 ) return false;
    size_t searchedDataSize = pos + target.size(); // 搜到指定数据时的大小（含指定数据）
    extraData->_setSize(0);
    extraData->append( data->getBuf<char>() + searchedDataSize, data->getSize() - searchedDataSize ); // 额外收到的数据
    data->_setSize(searchedDataSize);

    return true;
}

int Socket::recvWaitUntilTarget( winux::AnsiString const & target, winux::GrowBuffer * data, winux::GrowBuffer * extraData, size_t * hadRead, size_t * startpos, size_t * pos, double sec, int * rcWait, FunctionSuccessCallback eachSuccessCallback, void * param, int msgFlags )
{
    auto targetNextVal = winux::_Templ_KmpCalcNext<short>( target.c_str(), target.size() );
    int oneRead = 0;
    while ( data->getSize() - *startpos < target.size() || ( *pos = winux::_Templ_KmpMatchEx( data->getBuf<char>(), data->getSize(), target.c_str(), target.size(), *startpos, targetNextVal ) ) == -1 )
    {
        if ( data->getSize() >= target.size() ) *startpos = data->getSize() - target.size() + 1; // 计算下次搜索起始
        *rcWait = io::SelectRead(_self->sock).wait(sec);
        if ( *rcWait > 0 )
        {
            char buf[4096];
            oneRead = this->recv( buf, 4096, msgFlags );
            if ( oneRead > 0 )
            {
                data->append( buf, oneRead );
                *hadRead += oneRead;
                if ( eachSuccessCallback ) eachSuccessCallback( oneRead, param );
            }
            else if ( oneRead == 0 ) // 连接关闭
                break;
            else // recv()出错
                break;
        }
        else if ( *rcWait == 0 ) // select()超时
        {
            break;
        }
        else // *rcWait < 0，select()出错
        {
            oneRead = -1;
            break;
        }
    }
    if ( *pos != -1 ) // 搜索到指定数据
    {
        size_t searchedDataSize = *pos + target.size(); // 搜到指定数据时的大小（含指定数据）
        extraData->_setSize(0);
        extraData->append( data->getBuf<char>() + searchedDataSize, data->getSize() - searchedDataSize ); // 额外收到的数据
        data->_setSize(searchedDataSize);
    }
    return oneRead;
}

bool Socket::recvUntilSize( size_t targetSize, winux::GrowBuffer * data, int msgFlags )
{
    size_t hadRead = 0;
    while ( hadRead < targetSize ) 
    {
        char tmp[128];
        size_t remaining = targetSize - hadRead; // 剩余读取的数据量
        size_t oneWant = remaining > sizeof(tmp) ? sizeof(tmp) : remaining; // 这次想读的数据量
        int oneRead = this->recv( tmp, oneWant, msgFlags );
        if ( oneRead <= 0 ) return false; // 连接可能关闭或者出错，接收数据失败，返回false。
        data->append( tmp, oneRead );
        hadRead += oneRead;
    }
    return true;
}

int Socket::recvWaitUntilSize( size_t targetSize, winux::GrowBuffer * data, size_t * hadRead, double sec, int * rcWait, FunctionSuccessCallback eachSuccessCallback, void * param, int msgFlags )
{
    int oneRead = 0;
    while ( *hadRead < targetSize )
    {
        char tmp[128];
        size_t remaining = targetSize - *hadRead; // 剩余读取的数据量
        size_t oneWant = remaining > sizeof(tmp) ? sizeof(tmp) : remaining; // 这次想读的数据量

        *rcWait = io::SelectRead(_self->sock).wait(sec);
        if ( *rcWait > 0 )
        {
            oneRead = this->recv( tmp, oneWant, msgFlags );
            if ( oneRead > 0 )
            {
                data->append( tmp, oneRead );
                *hadRead += oneRead;
                if ( eachSuccessCallback ) eachSuccessCallback( oneRead, param );
            }
            else if ( oneRead == 0 ) // 连接关闭
                break;
            else
                break; // recv()出错
        }
        else if ( *rcWait == 0 )
        {
            break;
        }
        else
        {
            oneRead = -1;
            break;
        }
    }
    return oneRead;
}

winux::Buffer Socket::recvAvail( int msgFlags )
{
    return this->recv( this->getAvailable(), msgFlags );
}

winux::Buffer Socket::recvWaitAvail( double sec, int * rcWait, int msgFlags )
{
    // 等待有可接收的数据
    *rcWait = io::SelectRead(_self->sock).wait(sec);
    if ( *rcWait > 0 )
    {
        return this->recvAvail(msgFlags);
    }
    else if ( *rcWait == 0 )
    {
        return winux::Buffer("");
    }
    else
    {
        return winux::Buffer();
    }
}

int Socket::sendTo( EndPoint const & ep, void const * data, size_t size, int msgFlags )
{
    if ( _self->sock == -1 ) // 如果套接字还没创建，则创建
    {
        _self->addrFamily = ep.getAddrFamily();
        _self->sockType = sockDatagram;

        if ( !this->create() ) return -1;
    }

    int sentBytes = ::sendto( _self->sock, (char const *)data, (int)size, msgFlags, ep.get<sockaddr>(), (int)ep.size() );

#if defined(SOCKET_EXCEPTION_USE)
    int err = socket_errno;
    if ( sentBytes < 0 || sentBytes == 0 && err != 0 )
    {
        throw SocketError( err, winux::FormatA( "An error occurred while sending the data(to:%s,data:%p,size:%u).", winux::StringToLocal( ep.toString() ).c_str(), data, size ) );
    }
#endif

    return sentBytes;
}

int Socket::recvFrom( EndPoint * ep, void * buf, size_t size, int msgFlags )
{
    if ( _self->sock == -1 ) // 如果套接字还没创建，则创建
    {
        _self->addrFamily = ep->getAddrFamily();
        _self->sockType = sockDatagram;

        if ( !this->create() ) return -1;
    }

    ep->size() = sizeof(sockaddr_in6);
    int recvBytes = ::recvfrom( _self->sock, (char *)buf, (int)size, msgFlags, ep->get<sockaddr>(), (socklen_t *)&ep->size() );

#if defined(SOCKET_EXCEPTION_USE)
    int err = socket_errno;
    if ( recvBytes < 0 || recvBytes == 0 && err != 0 )
    {
        throw SocketError( err, winux::FormatA( "An error occurred while receiving data(buf:%p,size:%u).", buf, size ) );
    }
#endif

    return recvBytes;
}

winux::Buffer Socket::recvFrom( EndPoint * ep, size_t size, int msgFlags )
{
    winux::Buffer buf;
    buf.alloc( size, false );

    int recvSize = this->recvFrom( ep, buf.getBuf(), buf.getCapacity(), msgFlags );

    if ( recvSize >= 0 )
        buf._setSize(recvSize);
    else
        buf.free();

    return buf;
}

bool Socket::connect( EndPoint const & ep )
{
    if ( _self->sock == -1 ) // 如果套接字还没创建，则创建
    {
        _self->addrFamily = ep.getAddrFamily();
        _self->sockType = sockStream;

        if ( !this->create() ) return false;
    }

    int rc = ::connect( _self->sock, (sockaddr*)ep.get(), ep.size() );
    if ( rc == SOCKET_ERROR )
    {
    #if defined(SOCKET_EXCEPTION_USE)
        int err = socket_errno;
        throw SocketError( err, winux::FormatA( "An error occurred while connecting to the server(addr:%s).", winux::StringToLocal( ep.toString() ).c_str() ) );
    #else
        return false;
    #endif
    }
    return true;
}

bool Socket::bind( EndPoint const & ep )
{
    if ( _self->sock == -1 ) // 如果套接字还没创建，则创建
    {
        _self->addrFamily = ep.getAddrFamily();

        if ( !this->create() ) return false;
    }

    int rc = ::bind( _self->sock, (sockaddr*)ep.get(), ep.size() );
    if ( rc == SOCKET_ERROR )
    {
    #if defined(SOCKET_EXCEPTION_USE)
        int err = socket_errno;
        throw SocketError( err, winux::FormatA( "An error occurred while binding the address(addr:%s).", winux::StringToLocal( ep.toString() ).c_str() ) );
    #else
        return false;
    #endif
    }
    return true;
}

bool Socket::listen( int backlog )
{
    int rc = ::listen( _self->sock, backlog );
    if ( rc == SOCKET_ERROR )
    {
    #if defined(SOCKET_EXCEPTION_USE)
        int err = socket_errno;
        throw SocketError( err, winux::FormatA( "An error occurred while listening(backlog:%d).", backlog ) );
    #else
        return false;
    #endif
    }
    return true;
}

bool Socket::accept( int * sock, EndPoint * ep )
{
    UnionAddr addr;
    socklen_t acceptAddrLen;  // 调用accept()用的socket地址结构长度.
    acceptAddrLen = sizeof(addr);
    if ( ep ) ep->size() = sizeof(addr);

    *sock = (int)::accept(
        _self->sock,
        ( ep ? (sockaddr *)ep->get() : (sockaddr *)&addr ),
        ( ep ? (socklen_t *)&ep->size(): &acceptAddrLen )
    );
    if ( *sock == INVALID_SOCKET )
    {
    #if defined(SOCKET_EXCEPTION_USE)
        int err = socket_errno;
        throw SocketError( err, winux::FormatA( "An error occurred while accepting." ) );
    #else
        return false;
    #endif
    }
    return true;
}

int Socket::getRecvBufSize() const
{
    int optval = 0;
    socklen_t len = sizeof(optval);
    int rc = getsockopt( _self->sock, SOL_SOCKET, SO_RCVBUF, (char*)&optval, &len );
    (void)rc;
    return optval;
}

bool Socket::setRecvBufSize( int optval )
{
    if ( _self->sock == -1 )
    {
        _self->_attrRecvBufSize = optval;
        _self->_attrExecSets.push_back(Socket_Data::attrRecvBufSize);
        return true;
    }

    socklen_t len = sizeof(optval);
    int rc = setsockopt( _self->sock, SOL_SOCKET, SO_RCVBUF, (char*)&optval, len );
    if ( rc == SOCKET_ERROR )
    {
    #if defined(SOCKET_EXCEPTION_USE)
        int err = socket_errno;
        throw SocketError( err, winux::FormatA( "An error occurred while setsockopt() with SO_RCVBUF." ) );
    #else
        return false;
    #endif
    }
    return true;
}

int Socket::getSendBufSize() const
{
    int optval = 0;
    socklen_t len = sizeof(optval);
    int rc = getsockopt( _self->sock, SOL_SOCKET, SO_SNDBUF, (char*)&optval, &len );
    (void)rc;
    return optval;
}

bool Socket::setSendBufSize( int optval )
{
    if ( _self->sock == -1 )
    {
        _self->_attrSendBufSize = optval;
        _self->_attrExecSets.push_back(Socket_Data::attrSendBufSize);
        return true;
    }

    socklen_t len = sizeof(optval);
    int rc = setsockopt( _self->sock, SOL_SOCKET, SO_SNDBUF, (char*)&optval, len );
    if ( rc == SOCKET_ERROR )
    {
    #if defined(SOCKET_EXCEPTION_USE)
        int err = socket_errno;
        throw SocketError( err, winux::FormatA( "An error occurred while setsockopt() with SO_SNDBUF." ) );
    #else
        return false;
    #endif
    }
    return true;
}

winux::uint32 Socket::getRecvTimeout() const
{
#if defined(OS_WIN)
    DWORD optval = 0;
    socklen_t len = sizeof(optval);
    int rc = getsockopt( _self->sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&optval, &len );
    (void)rc;
    return optval;
#else
    winux::uint32 optval = 0;
    struct timeval tv = { 0 };
    socklen_t len = sizeof(tv);
    int rc = getsockopt( _self->sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, &len );
    (void)rc;
    optval += tv.tv_sec * 1000;
    optval += tv.tv_usec / 1000;
    return optval;
#endif
}

bool Socket::setRecvTimeout( winux::uint32 optval )
{
    if ( _self->sock == -1 )
    {
        _self->_attrRecvTimeout = optval;
        _self->_attrExecSets.push_back(Socket_Data::attrRecvTimeout);
        return true;
    }

#if defined(OS_WIN)
    socklen_t len = sizeof(optval);
    int rc = setsockopt( _self->sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&optval, len );
#else
    struct timeval tv = { 0 };
    socklen_t len = sizeof(tv);
    tv.tv_sec = optval / 1000;
    tv.tv_usec = ( optval % 1000 ) * 1000;
    int rc = setsockopt( _self->sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, len );
#endif
    if ( rc == SOCKET_ERROR )
    {
    #if defined(SOCKET_EXCEPTION_USE)
        int err = socket_errno;
        throw SocketError( err, winux::FormatA( "An error occurred while setsockopt() with SO_RCVTIMEO." ) );
    #else
        return false;
    #endif
    }
    return true;
}

winux::uint32 Socket::getSendTimeout() const
{
#if defined(OS_WIN)
    DWORD optval = 0;
    socklen_t len = sizeof(optval);
    int rc = getsockopt( _self->sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&optval, &len );
    (void)rc;
    return optval;
#else
    winux::uint32 optval = 0;
    struct timeval tv = { 0 };
    socklen_t len = sizeof(tv);
    int rc = getsockopt( _self->sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, &len );
    (void)rc;
    optval += tv.tv_sec * 1000;
    optval += tv.tv_usec / 1000;
    return optval;
#endif
}

bool Socket::setSendTimeout( winux::uint32 optval )
{
    if ( _self->sock == -1 )
    {
        _self->_attrSendTimeout = optval;
        _self->_attrExecSets.push_back(Socket_Data::attrSendTimeout);
        return true;
    }

#if defined(OS_WIN)
    socklen_t len = sizeof(optval);
    int rc = setsockopt( _self->sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&optval, len );
#else
    struct timeval tv = { 0 };
    socklen_t len = sizeof(tv);
    tv.tv_sec = optval / 1000;
    tv.tv_usec = ( optval % 1000 ) * 1000;
    int rc = setsockopt( _self->sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, len );
#endif
    if ( rc == SOCKET_ERROR )
    {
    #if defined(SOCKET_EXCEPTION_USE)
        int err = socket_errno;
        throw SocketError( err, winux::FormatA( "An error occurred while setsockopt() with SO_SNDTIMEO." ) );
    #else
        return false;
    #endif
    }
    return true;
}

bool Socket::getReUseAddr() const
{
    int optval = 0;
    socklen_t len = sizeof(optval);
    int rc = getsockopt( _self->sock, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, &len );
    (void)rc;
    return optval != 0;
}

bool Socket::setReUseAddr( bool optval )
{
    if ( _self->sock == -1 )
    {
        _self->_attrReUseAddr = optval;
        _self->_attrExecSets.push_back(Socket_Data::attrReUseAddr);
        return true;
    }

    int b = optval;
    socklen_t len = sizeof(b);
    int rc = setsockopt( _self->sock, SOL_SOCKET, SO_REUSEADDR, (char*)&b, len );
    if ( rc == SOCKET_ERROR )
    {
    #if defined(SOCKET_EXCEPTION_USE)
        int err = socket_errno;
        throw SocketError( err, winux::FormatA( "An error occurred while setsockopt() with SO_REUSEADDR." ) );
    #else
        return false;
    #endif
    }
    return true;
}

bool Socket::getBroadcast() const
{
    int optval = 0;
    socklen_t len = sizeof(optval);
    int rc = getsockopt( _self->sock, SOL_SOCKET, SO_BROADCAST, (char*)&optval, &len );
    (void)rc;
    return optval != 0;
}

bool Socket::setBroadcast( bool optval )
{
    if ( _self->sock == -1 )
    {
        _self->_attrBroadcast = optval;
        _self->_attrExecSets.push_back(Socket_Data::attrBroadcast);
        return true;
    }

    int b = optval;
    socklen_t len = sizeof(b);
    int rc = setsockopt( _self->sock, SOL_SOCKET, SO_BROADCAST, (char*)&b, len );
    if ( rc == SOCKET_ERROR )
    {
    #if defined(SOCKET_EXCEPTION_USE)
        int err = socket_errno;
        throw SocketError( err, winux::FormatA( "An error occurred while setsockopt() with SO_BROADCAST." ) );
    #else
        return false;
    #endif
    }
    return true;
}

bool Socket::getIpv6Only() const
{
    int optval = 0;
    socklen_t len = sizeof(optval);
    int rc = getsockopt( _self->sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&optval, &len );
    (void)rc;
    return optval != 0;
}

bool Socket::setIpv6Only( bool optval )
{
    if ( _self->sock == -1 )
    {
        _self->_attrIpv6Only = optval;
        _self->_attrExecSets.push_back(Socket_Data::attrIpv6Only);
        return true;
    }

    int b = optval;
    socklen_t len = sizeof(b);
    int rc = setsockopt( _self->sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&b, len );
    if ( rc == SOCKET_ERROR )
    {
    #if defined(SOCKET_EXCEPTION_USE)
        int err = socket_errno;
        throw SocketError( err, winux::FormatA( "An error occurred while setsockopt() with level=IPPROTO_IPV6, opt=IPV6_V6ONLY." ) );
    #else
        return false;
    #endif
    }
    return true;
}

int Socket::getError() const
{
    int optval = 0;
    socklen_t len = sizeof(optval);
    int rc = getsockopt( _self->sock, SOL_SOCKET, SO_ERROR, (char*)&optval, &len );
    (void)rc;
    return optval;
}

Socket::SockType Socket::getType() const
{
    int optval = 0;
    socklen_t len = sizeof(optval);
    int rc = getsockopt( _self->sock, SOL_SOCKET, SO_TYPE, (char*)&optval, &len );

    size_t n = countof(__sockTypes);
    for ( size_t i = 0; i < n; ++i )
    {
        if ( __sockTypes[i] == optval )
        {
            return static_cast<SockType>(i);
        }
    }
    (void)rc;
    return sockUnknown;
}

bool Socket::isListening() const
{
    int optval = 0;
    socklen_t len = sizeof(optval);
    int rc = getsockopt( _self->sock, SOL_SOCKET, SO_ACCEPTCONN, (char*)&optval, &len );
    (void)rc;
    return optval != 0;
}

int Socket::getAvailable() const
{
    u_long avail = 0;
    int rc = ioctlsocket( _self->sock, FIONREAD, &avail );
    (void)rc;
    return (int)avail;
}

bool Socket::setBlocking( bool blocking )
{
    if ( _self->sock == -1 )
    {
        _self->_attrBlocking = blocking;
        _self->_attrExecSets.push_back(Socket_Data::attrBlocking);
        return true;
    }

#if defined(OS_WIN)
    u_long mode = (u_long)!blocking;
    int rc = ioctlsocket( _self->sock, FIONBIO, &mode );
    if ( rc == SOCKET_ERROR )
    {
    #if defined(SOCKET_EXCEPTION_USE)
        int err = socket_errno;
        throw SocketError( err, winux::FormatA( "An error occurred while ioctlsocket() with FIONBIO set %u.", mode ) );
    #else
        return false;
    #endif
    }
#else
    int opts, rc;
    opts = fcntl( _self->sock, F_GETFL );
    if ( opts < 0 )
    {
    #if defined(SOCKET_EXCEPTION_USE)
        int err = socket_errno;
        throw SocketError( err, "An error occurred while fcntl() with F_GETFL." );
    #else
        return false;
    #endif
    }
    if ( blocking )
        opts = opts & ~O_NONBLOCK;
    else
        opts = opts | O_NONBLOCK;
    rc = fcntl( _self->sock, F_SETFL, opts );
    if ( rc < 0 )
    {
    #if defined(SOCKET_EXCEPTION_USE)
        int err = socket_errno;
        throw SocketError( err, winux::FormatA( "An error occurred while fcntl() with F_SETFL %s O_NONBLOCK.", ( blocking ? "unset" : "set" ) ) );
    #else
        return false;
    #endif
    }
#endif

    return true;
}

int Socket::get() const
{
    return _self->sock;
}

int Socket::ErrNo()
{
    return socket_errno;
}

// class SocketStreamBuf ------------------------------------------------------------------
SocketStreamBuf::SocketStreamBuf(
    eiennet::Socket * sock,
    std::ios_base::openmode mode /*= std::ios_base::in | std::ios_base::out*/,
    size_t inputBufSize /*= (size_t)-1*/,
    size_t outputBufSize /*= (size_t)-1 */
) : _sock(sock)
{
    if ( mode & std::ios_base::in )
    {
        // input buffer begin -----------------------------------------------------------------------
        inputBufSize = inputBufSize == (size_t)-1 ? sock->getRecvBufSize() : inputBufSize;
        inputBufSize = inputBufSize > 0 ? inputBufSize : 4096;
        _inputBuf.alloc( inputBufSize, false );
        this->setg( _inputBuf.getBuf<char>(), _inputBuf.getBuf<char>(), _inputBuf.getBuf<char>() );
        // input buffer end -----------------------------------------------------------------------
    }

    if ( mode & std::ios_base::out )
    {
        // output buffer begin -----------------------------------------------------------------------
        outputBufSize = outputBufSize == (size_t)-1 ? sock->getSendBufSize() : outputBufSize;
        outputBufSize = outputBufSize > 0 ? outputBufSize : 4096;
        _outputBuf.alloc( outputBufSize, false );
        // 输出缓冲区留一个字节不设，待溢出时的那1字节填入，再一次性发出
        this->setp( _outputBuf.getBuf<char>(), _outputBuf.getBuf<char>() + _outputBuf.getCapacity() - 1 );
        // output buffer end -----------------------------------------------------------------------
    }
}

SocketStreamBuf::~SocketStreamBuf()
{
    // output buffer begin -----------------------------------------------------------------------
    this->sync();
    // output buffer end -----------------------------------------------------------------------
}

// input buffer begin -----------------------------------------------------------------------
std::streambuf::int_type SocketStreamBuf::underflow()
{
    int recvLen = _sock->recv( _inputBuf.getBuf<char>(), _inputBuf.getCapacity() );
    if ( recvLen < 1 ) return traits_type::eof();

    this->setg( _inputBuf.getBuf<char>(), _inputBuf.getBuf<char>(), _inputBuf.getBuf<char>() + recvLen );

    return *gptr();
}
// input buffer end -----------------------------------------------------------------------

// output buffer begin -----------------------------------------------------------------------
std::streambuf::int_type SocketStreamBuf::overflow( int_type c )
{
    if ( c != traits_type::eof() )
    {
        *pptr() = c;
        pbump(1);
    }

    if ( _flush() == -1 ) return traits_type::eof();

    return c;
}

int SocketStreamBuf::sync()
{
    if ( _flush() == -1 ) return -1;

    return 0;
}

int SocketStreamBuf::_flush()
{
    int len = (int)( pptr() - pbase() );

    int bytes = _sock->send( _outputBuf.getBuf(), len );
    if ( bytes < 1 )
    {
        return -1;
    }

    pbump(-len);

    return len;
}
// output buffer end -----------------------------------------------------------------------

// class SocketStreamOut -------------------------------------------------------------------
eiennet::SocketStreamOut & SocketStreamOut::writeAndFlush( winux::Buffer const & data )
{
    this->write( data.getBuf<char>(), (std::streamsize)data.getSize() ).flush();
    return *this;
}

// class SocketStreamIn --------------------------------------------------------------------
std::streamsize SocketStreamIn::getAvailable() const
{
    return _sockBuf->in_avail() + _sockBuf->getSocket()->getAvailable();
}

eiennet::SocketStreamIn & SocketStreamIn::readAvail( winux::Buffer * data )
{
    //data->alloc( (winux::uint)this->getAvailable() );
    //this->std::istream::read( data->getBuf<char>(), (std::streamsize)data->getSize() );
    //return *this;
    return this->read( data, (size_t)this->getAvailable() );
}

eiennet::SocketStreamIn & SocketStreamIn::read( winux::Buffer * data, size_t size )
{
    data->alloc(size);
    this->std::istream::read( data->getBuf<char>(), (std::streamsize)data->getSize() );
    return *this;
}

std::streamsize SocketStreamIn::waitAvail( double sec )
{
    std::streamsize avail = _sockBuf->in_avail() + _sockBuf->getSocket()->getAvailable();
    if ( avail > 0 ) return avail;
    int rc = io::SelectRead( _sockBuf->getSocket() ).wait(sec);
    if ( rc > 0 )
    {
        return _sockBuf->getSocket()->getAvailable();
    }
    else if ( rc == 0 )
    {
        return 0;
    }
    else
    {
        return -1;
    }
}


// namespace ip ---------------------------------------------------------------------------
namespace ip
{
// struct EndPoint_Data -------------------------------------------------------------------
struct EndPoint_Data
{
    UnionAddr addr;
    socklen_t len;

    EndPoint_Data()
    {
        memset( this, 0, sizeof(EndPoint_Data) );
    }
};

// EndPoint_Data related methods ----------------------------------------------------------
inline static void __ParseIpv4( winux::String const & ipStr, winux::String const & portStr, sockaddr_in * addr )
{
    addr->sin_family = __addrFamilies[Socket::afInet];

    // 解析IP部分
    winux::StringArray arrIpParts;
    winux::StrSplit( ipStr, TEXT("."), &arrIpParts, false );
    int i = 0;
    for ( auto & ipPart : arrIpParts )
    {
        *( (winux::byte *)&addr->sin_addr.s_addr + i ) = (winux::byte)winux::StrToUInt64( ipPart, 10 );
        i++;
    }

    // 解析端口部分
    if ( !portStr.empty() )
        addr->sin_port = htont( (winux::ushort)winux::StrToUInt64( portStr, 10 ) );
}

inline static void __ParseIpv6( winux::String const & ipStr, winux::String const & portStr, sockaddr_in6 * addr )
{
    addr->sin6_family = __addrFamilies[Socket::afInet6];

    // 解析IP部分
    // 搜索“::”
    size_t pos = winux::String::npos;
    winux::String doubleColon = TEXT("::");
    if ( ( pos = ipStr.find(doubleColon) ) != winux::String::npos ) // has "::"
    {
        winux::String beginPart = ipStr.substr( 0, pos );
        winux::String endPart = ipStr.substr( pos + doubleColon.length() );
        winux::StringArray arrBeginParts, arrEndParts;
        winux::StrSplit( beginPart, TEXT(":"), &arrBeginParts, false );
        winux::StrSplit( endPart, TEXT(":"), &arrEndParts, false );
        for ( size_t i = 0; i < arrBeginParts.size(); i++ )
        {
            addr->sin6_addr.s6_addr16[i] = htont<winux::ushort>( (winux::ushort)winux::StrToUInt64( arrBeginParts[i], 16 ) );
        }
        for ( int j = (int)arrEndParts.size() - 1, k = countof(addr->sin6_addr.s6_addr16) - 1; j >= 0; j--, k-- )
        {
            addr->sin6_addr.s6_addr16[k] = htont<winux::ushort>( (winux::ushort)winux::StrToUInt64( arrEndParts[j], 16 ) );
        }
    }
    else
    {
        winux::StringArray arrIpParts;
        winux::StrSplit( ipStr, TEXT(":"), &arrIpParts );
        int i = 0;
        for ( auto & ipPart : arrIpParts )
        {
            addr->sin6_addr.s6_addr16[i] = htont<winux::ushort>( (winux::ushort)winux::StrToUInt64( ipPart, 16 ) );
            i++;
        }
    }

    // 解析端口部分
    if ( !portStr.empty() )
        addr->sin6_port = htont( (winux::ushort)winux::StrToUInt64( portStr, 10 ) );
}

// 优先从ep中解析端口号，如果没有则依照port指定
inline static void __ParseEndPoint( winux::String const & ep, winux::ushort port, EndPoint_Data * pEpData )
{
    winux::String epStr = winux::StrTrim(ep);

    if ( epStr.empty() ) // is IPv4
    {
        pEpData->len = sizeof(pEpData->addr.addrInet);

        __ParseIpv4( epStr, TEXT(""), &pEpData->addr.addrInet );
        pEpData->addr.addrInet.sin_port = htont(port);
    }
    else if ( epStr.find('.') != winux::String::npos ) // is IPv4
    {
        pEpData->len = sizeof(pEpData->addr.addrInet);

        winux::String ipv4Str;
        winux::String portStr;

        size_t pos = winux::String::npos;
        if ( ( pos = epStr.find(':') ) != winux::String::npos ) // has port
        {
            ipv4Str = epStr.substr( 0, pos );
            portStr = epStr.substr( pos + 1 );
            __ParseIpv4( ipv4Str, portStr, &pEpData->addr.addrInet );
        }
        else
        {
            ipv4Str = epStr;
            __ParseIpv4( ipv4Str, portStr, &pEpData->addr.addrInet );
            pEpData->addr.addrInet.sin_port = htont(port);
        }
    }
    else if ( epStr[0] == ':' && epStr.find( ':', 1 ) == winux::String::npos ) // is IPv4
    {
        pEpData->len = sizeof(pEpData->addr.addrInet);

        __ParseIpv4( TEXT(""), epStr.substr(1), &pEpData->addr.addrInet );
    }
    else // is IPv6
    {
        pEpData->len = sizeof(pEpData->addr.addrInet6);

        winux::String ipv6Str;
        winux::String portStr;

        if ( epStr[0] == '[' )
        {
            size_t pos = winux::String::npos;
            if ( ( pos = epStr.find(']') ) != winux::String::npos ) // has ']'
            {
                ipv6Str = epStr.substr( 1, pos - 1 ); // between '[' and ']'

                if ( ( pos = epStr.find( ':', pos + 1 ) ) != winux::String::npos ) // has port
                {
                    portStr = epStr.substr( pos + 1 ); // skip ':'
                    __ParseIpv6( ipv6Str, portStr, &pEpData->addr.addrInet6 );
                }
                else
                {
                    __ParseIpv6( ipv6Str, portStr, &pEpData->addr.addrInet6 );
                    pEpData->addr.addrInet6.sin6_port = htont(port);
                }
            }
            else
            {
                ipv6Str = epStr.substr(1); // skip '['
                __ParseIpv6( ipv6Str, portStr, &pEpData->addr.addrInet6 );
                pEpData->addr.addrInet6.sin6_port = htont(port);
            }
        }
        else
        {
            ipv6Str = epStr;
            __ParseIpv6( ipv6Str, portStr, &pEpData->addr.addrInet6 );
            pEpData->addr.addrInet6.sin6_port = htont(port);
        }
    }
}

inline static winux::String __Ipv4ToString( in_addr addr )
{
    return winux::Format(
        TEXT("%u.%u.%u.%u"),
        ((winux::byte *)&addr.s_addr)[0],
        ((winux::byte *)&addr.s_addr)[1],
        ((winux::byte *)&addr.s_addr)[2],
        ((winux::byte *)&addr.s_addr)[3]
    );
}

inline static winux::String __Ipv6ToString( in6_addr const & addr )
{
    // addr.s6_addr16;
    // 找到连续0字最多的区间 { start, len }
    std::pair< short, short > range( -1, 0 ), maxRange( -1, 0 );
    for ( size_t i = 0; i < countof(addr.s6_addr16); i++ )
    {
        if ( addr.s6_addr16[i] == 0 )
        {
            if ( range.second == 0 ) range.first = (short)i;
            range.second++;
        }
        else
        {
            if ( range.second > 0 )
            {
                if ( range.second > maxRange.second )
                {
                    maxRange = range;
                }
                range.first = -1;
                range.second = 0;
            }
        }
    }
    if ( range.second > 0 )
    {
        if ( range.second > maxRange.second )
        {
            maxRange = range;
        }
        range.first = -1;
        range.second = 0;
    }

    // 输出
    winux::String ip;
    if ( maxRange.second == 0 ) // 说明没有连续的0字
    {
        for ( size_t i = 0; i < countof(addr.s6_addr16); i++ )
        {
            if ( i != 0 )
            {
                ip += ':';
            }
            winux::StringWriter( &ip, true ) << std::hex << ntoht(addr.s6_addr16[i]);
        }
    }
    else // 有连续0字
    {
        bool f = false; // 是否已经输出双冒号
        for ( size_t i = 0; i < countof(addr.s6_addr16); i++ )
        {
            if ( (short)i >= maxRange.first && (short)i < maxRange.first + maxRange.second )
            {
                if ( !f )
                {
                    ip += TEXT("::");
                }
                f = true;
            }
            else
            {
                if ( i != 0 && !f )
                {
                    ip += ':';
                }
                f = false;
                winux::StringWriter( &ip, true ) << std::hex << ntoht(addr.s6_addr16[i]);
            }
        }
    }
    return ip;
}

// class ip::EndPoint ---------------------------------------------------------------------
EndPoint::EndPoint( Socket::AddrFamily af )
{
    _self.create(); //

    this->init(af);
}

EndPoint::EndPoint( winux::Mixed const & ipAndPort )
{
    _self.create(); //

    this->init(ipAndPort);
}

EndPoint::EndPoint( winux::String const & ipAddr, winux::ushort port )
{
    _self.create(); //

    this->init( ipAddr, port );
}

EndPoint::EndPoint( EndPoint const & other )
{
    _self.create(); //

    _self = other._self;
}

EndPoint & EndPoint::operator = ( EndPoint const & other )
{
    if ( this != &other )
    {
        _self = other._self;
    }
    return *this;
}

#ifndef MOVE_SEMANTICS_DISABLED
EndPoint::EndPoint( EndPoint && other )
{
    _self = std::move(other._self);
}

EndPoint & EndPoint::operator = ( EndPoint && other )
{
    if ( this != &other )
    {
        _self = std::move(other._self);
    }
    return *this;
}
#endif

EndPoint::~EndPoint()
{

    _self.destroy(); //
}

void EndPoint::init( Socket::AddrFamily af )
{
    switch ( af )
    {
    case Socket::afInet:
        _self->addr.addrInet.sin_family = __addrFamilies[Socket::afInet];
        _self->len = sizeof(_self->addr.addrInet);
        break;
    case Socket::afInet6:
        _self->addr.addrInet6.sin6_family = __addrFamilies[Socket::afInet6];
        _self->len = sizeof(_self->addr.addrInet6);
        break;
    default:
        _self->addr.addrInet.sin_family = __addrFamilies[af];
        _self->len = 0;
        break;
    }
}

void EndPoint::init( winux::Mixed const & ipAndPort )
{
    if ( ipAndPort.isString() ) // "127.0.0.1:80"
    {
        __ParseEndPoint( ipAndPort.to<winux::String>(), 0, _self.get() );
    }
    else if ( ipAndPort.isArray() ) // [ "127.0.0.1", 80 ]
    {
        winux::String strIp;
        winux::ushort port = 0;
        size_t n = ipAndPort.getCount();

        if ( n > 0 ) strIp = ipAndPort[0].to<winux::String>();
        if ( n > 1 ) port = ipAndPort[1].toUShort();

        __ParseEndPoint( strIp, port, _self.get() );
    }
    else if ( ipAndPort.isCollection() ) // { "127.0.0.1": 80 }
    {
        if ( ipAndPort.getCount() > 0 )
        {
            auto pr = ipAndPort.getPair(0);
            winux::String strIp = pr.first.to<winux::String>();
            winux::ushort port = pr.second.toUShort();

            __ParseEndPoint( strIp, port, _self.get() );
        }
    }
}

void EndPoint::init( winux::String const & ipAddr, winux::ushort port )
{
    __ParseEndPoint( ipAddr, port, _self.get() );
}

void * EndPoint::get() const
{
    return _self.get();
}

winux::uint & EndPoint::size() const
{
    return (winux::uint &)_self.get()->len;
}

winux::String EndPoint::toString() const
{
    winux::String r;
    switch ( _self->addr.addr.sa_family )
    {
    case __addrFamilies[Socket::afInet]:
        {
            winux::String ip = __Ipv4ToString(_self->addr.addrInet.sin_addr);
            if ( _self->addr.addrInet.sin_port )
            {
                r = winux::Format( TEXT("%s:%u"), ip.c_str(), ntoht(_self->addr.addrInet.sin_port) );
            }
            else
            {
                r = ip;
            }
        }
        break;
    case __addrFamilies[Socket::afInet6]:
        {
            winux::String ip = __Ipv6ToString(_self->addr.addrInet6.sin6_addr);
            if ( _self->addr.addrInet6.sin6_port )
            {
                r = winux::Format( TEXT("[%s]:%u"), ip.c_str(), ntoht(_self->addr.addrInet6.sin6_port) );
            }
            else
            {
                r = ip;
            }
        }
        break;
    }

    return r;
}

eiennet::EndPoint * EndPoint::clone() const
{
    EndPoint * ep = new EndPoint();
    *ep = *this;
    return ep;
}

Socket::AddrFamily EndPoint::getAddrFamily() const
{
    Socket::AddrFamily af = Socket::afUnspec;
    for ( int i = 0; i < countof(__addrFamilies); i++ )
    {
        if ( _self->addr.addr.sa_family == __addrFamilies[i] )
        {
            af = (Socket::AddrFamily)i;
            break;
        }
    }
    return af;
}

EndPoint::operator winux::Mixed() const
{
    winux::Mixed endpoint;
    endpoint.addPair()( this->getIp(), this->getPort() );
    return endpoint;
}

winux::String EndPoint::getIp() const
{
    switch ( _self->addr.addr.sa_family )
    {
    case __addrFamilies[Socket::afInet]:
        return __Ipv4ToString(_self->addr.addrInet.sin_addr);
        break;
    case __addrFamilies[Socket::afInet6]:
        return __Ipv6ToString(_self->addr.addrInet6.sin6_addr);
        break;
    }
    return winux::String();
}

winux::ushort EndPoint::getPort() const
{
    switch ( _self->addr.addr.sa_family )
    {
    case __addrFamilies[Socket::afInet]:
        return ntoht(_self->addr.addrInet.sin_port);
        break;
    case __addrFamilies[Socket::afInet6]:
        return ntoht(_self->addr.addrInet6.sin6_port);
        break;
    }
    return ntoht(0);
}

// class Resolver -------------------------------------------------------------------------
void __ParseResolver( winux::String const & rvrStr, winux::String * hostname, winux::ushort * port )
{
    winux::String str = winux::StrTrim(rvrStr);
    hostname->clear();
    *port = 0;
    if ( !str.empty() )
    {
        size_t pos;
        if ( str[0] == '[' ) // [ipv6addr]:port
        {
            if ( ( pos = str.find(']') ) != winux::String::npos )
            {
                *hostname = str.substr( 1, pos - 1 );
                if ( ( pos = str.find( ':', pos + 1 ) ) != winux::String::npos )
                {
                    *port = winux::Mixed( str.substr( pos + 1 ) );
                }
            }
            else // 没有']'
            {
                *hostname = str.substr( 1, pos );
            }
        }
        else
        {
            if ( ( pos = str.rfind(':') ) != winux::String::npos )
            {
                *hostname = str.substr( 0, pos );
                *port = winux::Mixed( str.substr( pos + 1 ) );
            }
            else
            {
                *hostname = str;
            }
        }
    }
}

Resolver::Resolver( winux::Mixed const & hostAndPort )
{
    if ( hostAndPort.isString() ) // "fastdo.net:80"
    {
        winux::String hostname;
        winux::ushort port;
        __ParseResolver( hostAndPort, &hostname, &port );
        this->_resolve( hostname, port );
    }
    else if ( hostAndPort.isArray() ) // [ "fastdo.net", 80 ]
    {
        size_t n = hostAndPort.getCount();
        this->_resolve( hostAndPort[0], ( n > 1 ? winux::Mixed(hostAndPort[1]).toUShort() : 0U ) );
    }
    else if ( hostAndPort.isCollection() ) // { "fastdo.net": 80 }
    {
        if ( hostAndPort.getCount() > 0 )
        {
            auto pr = hostAndPort.getPair(0);
            this->_resolve( pr.first, pr.second );
        }
    }
}

Resolver::Resolver( winux::String const & hostName, winux::ushort port )
{
    this->_resolve( hostName, port );
}

winux::String Resolver::toString() const
{
    return winux::Format( TEXT("%s:%u"), _hostName.c_str(), _port );
}

Resolver::operator winux::Mixed() const
{
    winux::Mixed result;
    result.createCollection();
    winux::Mixed & epMixedArr = result[ this->toString() ].createArray();
    for ( auto && ep : _epArr )
    {
        epMixedArr.add( ep.operator winux::Mixed() );
    }
    return result;
}

size_t Resolver::_resolve( winux::String const & hostName, winux::ushort port )
{
    _hostName = hostName;
    _port = port;
    _epArr.clear();

    size_t cnt = 0;
#if defined(OS_WIN)
    ADDRINFOT * infoHead = nullptr;
    ADDRINFOT * info;
    int rc = GetAddrInfo( hostName.c_str(), NULL, NULL, &infoHead );
#else
    addrinfo * infoHead = nullptr;
    addrinfo * info;
    int rc = getaddrinfo( hostName.c_str(), NULL, NULL, &infoHead );
#endif
    if ( rc == 0 )
    {
        info = infoHead;
        while ( info != nullptr )
        {
            size_t i = _epArr.size();
            _epArr.emplace_back();

            ip::EndPoint & ep = _epArr[i];
            ep.size() = (winux::uint)info->ai_addrlen;
            memcpy( ep.get(), info->ai_addr, info->ai_addrlen );
            ep.get<sockaddr_in>()->sin_port = htont(port);

            cnt++;
            info = info->ai_next;
        }
    }

    if ( infoHead != nullptr )
    #if defined(OS_WIN)
        FreeAddrInfo(infoHead);
    #else
        freeaddrinfo(infoHead);
    #endif

    return cnt;
}

// namespace tcp --------------------------------------------------------------------------
namespace tcp
{

EIENNET_FUNC_IMPL(int) ConnectAttempt( Socket * sock, EndPoint const & ep, winux::uint32 timeoutMs )
{
    int rc = -1;
    sock->setBlocking(false); // 设置成非阻塞

    if ( sock->eiennet::Socket::connect(ep) )
    {
        rc = 0;
    }
    else
    {
        int err = 0;
        int r = io::SelectWrite(*sock).wait( timeoutMs / 1000.0 );
        if ( r > 0 )
        {
            err = sock->getError();
            if ( 0 == err ) rc = 0;
        }
        else if ( r == 0 ) // timeout
        {
            err = sock->getError();
            if ( 0 == err ) rc = 1;
        }
    }

    sock->setBlocking(true); // 设置成阻塞

    return rc;
}

EIENNET_FUNC_IMPL(int) ConnectAttempt( Socket * sock, Resolver const & resolver, winux::uint32 perCnnTimeoutMs )
{
    int rc = -1;
    sock->setBlocking(false); // 设置成非阻塞
    for ( auto epIt = resolver.begin(); epIt != resolver.end(); epIt++ )
    {
        if ( sock->eiennet::Socket::connect(*epIt) )
        {
            rc = 0;
        }
        else
        {
            int err = 0;
            int r = io::SelectWrite(*sock).wait( perCnnTimeoutMs / 1000.0 );
            if ( r > 0 )
            {
                err = sock->getError();
                if ( 0 == err ) rc = 0;
            }
            else if ( r == 0 ) // timeout
            {
                err = sock->getError();
                if ( 0 == err ) rc = 1;
            }
        }

        if ( rc == 0 ) // if success, `rc` returned directly.
            break;
        else if ( rc < 0 ) // if failure, continue.
            ;
        else // if timeout, continue.
            ;
    }
    sock->setBlocking(true); // 设置成阻塞
    return rc;
}

} // namespace tcp

// namespace udp --------------------------------------------------------------------------
namespace udp
{

} // namespace udp

} // namespace ip

// namespace io ---------------------------------------------------------------------------
namespace io
{
// struct SelectRead_Data -----------------------------------------------------------------
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

// class SelectRead -----------------------------------------------------------------------
SelectRead::SelectRead()
{
    _self.create(); //

}

SelectRead::SelectRead( Socket const & sock )
{
    _self.create(); //

    this->setReadSock(sock);
}

SelectRead::SelectRead( Socket const * sock )
{
    _self.create(); //

    this->setReadSock(sock);
}

SelectRead::SelectRead( int fd )
{
    _self.create(); //

    this->setReadFd(fd);
}

SelectRead::SelectRead( winux::Mixed const & fds )
{
    _self.create(); //

    this->setReadFds(fds);
}

SelectRead::~SelectRead()
{

    _self.destroy(); //
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

// struct SelectWrite_Data ----------------------------------------------------------------
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

// class SelectWrite ----------------------------------------------------------------------
SelectWrite::SelectWrite()
{
    _self.create(); //

}

SelectWrite::SelectWrite( Socket const & sock )
{
    _self.create(); //

    this->setWriteSock(sock);
}

SelectWrite::SelectWrite( Socket const * sock )
{
    _self.create(); //

    this->setWriteSock(sock);
}

SelectWrite::SelectWrite( int fd )
{
    _self.create(); //

    this->setWriteFd(fd);
}

SelectWrite::SelectWrite( winux::Mixed const & fds )
{
    _self.create(); //

    this->setWriteFds(fds);
}

SelectWrite::~SelectWrite()
{

    _self.destroy(); //
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

// struct SelectExcept_Data ---------------------------------------------------------------
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

// class SelectExcept ---------------------------------------------------------------------
SelectExcept::SelectExcept()
{
    _self.create(); //

}

SelectExcept::SelectExcept( Socket const & sock )
{
    _self.create(); //

    this->setExceptSock(sock);
}

SelectExcept::SelectExcept( Socket const * sock )
{
    _self.create(); //

    this->setExceptSock(sock);
}

SelectExcept::SelectExcept( int fd )
{
    _self.create(); //

    this->setExceptFd(fd);
}

SelectExcept::SelectExcept( winux::Mixed const & fds )
{
    _self.create(); //

    this->setExceptFds(fds);
}

SelectExcept::~SelectExcept()
{

    _self.destroy(); //
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

// class Select ---------------------------------------------------------------------------
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

// class ClientCtx ------------------------------------------------------------------------
ClientCtx::ClientCtx( Server * server, winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr ) :
    server(server),
    clientId(clientId),
    clientEpStr(clientEpStr),
    clientSockPtr(clientSockPtr),
    canRemove(false),
    processingEvent(false)
{

}

ClientCtx::~ClientCtx()
{
    if ( this->server && this->server->_verbose )
    {
        winux::ColorOutputLine( winux::fgBlue, this->getStamp(), "析构" );
    }
}

winux::String ClientCtx::getStamp() const
{
    winux::String stamp;
    winux::StringWriter(&stamp)
    #if defined(OS_WIN)
        << "{tid:" << GetCurrentThreadId() << "}"
    #else
        << "{tid:" << std::hex << pthread_self() << "}"
    #endif
        << "[客户-" << this->clientId << "]<" << this->clientEpStr << ">";
    return stamp;
}

// class Server ---------------------------------------------------------------------------
Server::Server() :
    _mtxServer(true),
    _cumulativeClientId(0),
    _stop(false),
    _servSockAIsListening(false),
    _servSockBIsListening(false),
    _isAutoReadData(true),
    _serverWait(0.002),
    _verboseInterval(0.01),
    _verbose(true)
{

}

Server::Server( bool autoReadData, ip::EndPoint const & ep, int threadCount, int backlog, double serverWait, double verboseInterval, bool verbose ) :
    _mtxServer(true),
    _cumulativeClientId(0),
    _stop(false),
    _servSockAIsListening(false),
    _servSockBIsListening(false),
    _isAutoReadData(autoReadData),
    _serverWait(serverWait),
    _verboseInterval(verboseInterval),
    _verbose(verbose)
{
    this->startup( autoReadData, ep, threadCount, backlog, serverWait, verboseInterval, verbose );
}

Server::~Server()
{

}

// Linux平台下
// 当socket先以IPv6监听时也会同时监听IPv4线路，此时再创建IPv4监听将失败。
// 当socket先以IPv4监听时，同样创建IPv6监听此端口也会失败。（这是因为Linux下IPV6套接字的IPV6_V6ONLY默认是false，而Windows下是true。）
// 需要如下设置才可以监听与IPv4相同的端口

// 启动IPv4、IPv6两个Sockets
void __StartupSockets( ip::EndPoint const & ep, int backlog, bool verbose, ip::EndPoint * pEp2, ip::tcp::Socket * sockA, ip::tcp::Socket * sockB )
{
    // 创建另一个EP
    ip::EndPoint & ep2 = *pEp2;
    switch ( ep.get<sockaddr>()->sa_family )
    {
    case __addrFamilies[Socket::afInet]:
        {
            auto p = ep2.get<sockaddr_in6>();
            p->sin6_family = __addrFamilies[Socket::afInet6];
            p->sin6_port = ep.get<sockaddr_in>()->sin_port;
            ep2.size() = sizeof(sockaddr_in6);

            sockA->create( Socket::afInet, Socket::sockStream, Socket::protoUnspec );
            sockB->create( Socket::afInet6, Socket::sockStream, Socket::protoUnspec );

            sockB->setIpv6Only(true);
        }
        break;
    case __addrFamilies[Socket::afInet6]:
        {
            auto p = ep2.get<sockaddr_in>();
            p->sin_family = __addrFamilies[Socket::afInet];
            p->sin_port = ep.get<sockaddr_in6>()->sin6_port;
            ep2.size() = sizeof(sockaddr_in);

            sockA->create( Socket::afInet6, Socket::sockStream, Socket::protoUnspec );
            sockB->create( Socket::afInet, Socket::sockStream, Socket::protoUnspec );

            sockA->setIpv6Only(true);
        }
        break;
    }

    sockA->setReUseAddr(true);
    sockB->setReUseAddr(true);

    // 同时监听2个端点
    if ( !( sockA->bind(ep) && sockA->listen(backlog) ) )
    {
        if ( verbose )
        {
            int err = Socket::ErrNo();
            winux::ColorOutputLine(
                winux::fgRed,
                "Socket#1启动失败",
                ", ep=", ep.toString(),
                ", err=", err
            );
        }
    }
    if ( !( sockB->bind(ep2) && sockB->listen(backlog) ) )
    {
        if ( verbose )
        {
            int err2 = Socket::ErrNo();
            winux::ColorOutputLine(
                winux::fgRed,
                "Socket#2启动失败",
                ", ep2=", ep2.toString(),
                ", err2=", err2
            );
        }
    }
}

bool Server::startup( bool autoReadData, ip::EndPoint const & ep, int threadCount, int backlog, double serverWait, double verboseInterval, bool verbose )
{
    _pool.startup(threadCount);

    _cumulativeClientId = 0;

    // 启动套接字监听
    ip::EndPoint ep2;
    __StartupSockets( ep, backlog, verbose, &ep2, &_servSockA, &_servSockB );

    // 两个都不处于监听中
    _servSockAIsListening = _servSockA.isListening();
    _servSockBIsListening = _servSockB.isListening();
    _stop = !_servSockAIsListening && !_servSockBIsListening;

    _isAutoReadData = autoReadData;

    _serverWait = serverWait;
    _verboseInterval = verboseInterval;
    _verbose = verbose;

    if ( this->_verbose )
    {
        if ( _stop )
        {
            winux::ColorOutputLine(
                winux::fgRed,
                "启动服务器失败",
                ", ep=", ep.toString(),
                ", ep2=", ep2.toString(),
                ", threads=", threadCount,
                ", backlog=", backlog,
                ", serverWait=", serverWait,
                ", verboseInterval=", verboseInterval,
                ", verbose=", verbose,
                ""
            );
        }
        else
        {
            winux::ColorOutputLine(
                winux::fgGreen,
                "启动服务器成功",
                ", ep=", ep.toString(),
                ", ep2=", ep2.toString(),
                ", threads=", threadCount,
                ", backlog=", backlog,
                ", serverWait=", serverWait,
                ", verboseInterval=", verboseInterval,
                ", verbose=", verbose,
                ""
            );
        }
    }

    return !_stop;
}

int Server::run( void * runParam )
{
    int counter = 0;
    while ( !_stop )
    {
        eiennet::io::Select sel;
        // 监视servSockA
        if ( _servSockAIsListening )
        {
            sel.setExceptSock(_servSockA);
            sel.setReadSock(_servSockA);
        }

        // 监视servSockB
        if ( _servSockBIsListening )
        {
            sel.setExceptSock(_servSockB);
            sel.setReadSock(_servSockB);
        }

        if ( true )
        {
            winux::ScopeGuard guard(this->_mtxServer);
            // 输出一些服务器状态信息
            size_t counter2 = static_cast<size_t>( this->_verboseInterval / this->_serverWait );
            counter2 = counter2 ? counter2 : 1;
            if ( this->_verbose && ++counter % counter2 == 0 )
            {
                winux::DateTimeL dtl;
                winux::ColorOutput(
                    winux::fgWhite,
                    dtl.fromCurrent(),
                    ", 总客户数:", this->_clients.size(),
                    ", 当前任务数:",
                    this->_pool.getTaskCount(),
                    winux::String( 20, ' ' ),
                    "\r"
                );
            }

            // 监视客户连接
            for ( auto it = this->_clients.begin(); it != this->_clients.end(); )
            {
                if ( it->second->canRemove ) // 删除标记为可删除的客户
                {
                    if ( this->_verbose ) winux::ColorOutputLine( winux::fgMaroon, it->second->getStamp(), "移除" );
                    it = this->_clients.erase(it);
                }
                else if ( it->second->processingEvent ) // 跳过有事件在处理中的客户
                {
                    it++;
                }
                else
                {
                    sel.setReadSock(*it->second->clientSockPtr.get());
                    sel.setExceptSock(*it->second->clientSockPtr.get());

                    it++;
                }
            }
        }

        int rc = sel.wait(this->_serverWait); // 返回就绪的套接字数
        if ( rc > 0 )
        {
            if ( this->_verbose ) winux::ColorOutputLine( winux::fgGreen, "io.Select()模型获取到就绪的套接字数:", rc );

            // 处理servSockA事件
            if ( _servSockAIsListening )
            {
                if ( sel.hasReadSock(_servSockA) )
                {
                    // 有一个客户连接到来
                    eiennet::ip::EndPoint clientEp;
                    auto clientSockPtr = _servSockA.accept(&clientEp);

                    if ( clientSockPtr )
                    {
                        winux::SharedPointer<ClientCtx> * pClientCtxPtr = nullptr;
                        if ( this->_addClient( clientEp, clientSockPtr, &pClientCtxPtr ) )
                        {
                            if ( this->_verbose ) winux::ColorOutputLine( winux::fgFuchsia, (*pClientCtxPtr)->getStamp(), "新加入服务器" );
                        }
                    }

                    rc--;
                }
                else if ( sel.hasExceptSock(_servSockA) )
                {
                    winux::ScopeGuard guard(this->_mtxServer);
                    _stop = true;

                    rc--;
                }
            }

            // 处理servSockB事件
            if ( _servSockBIsListening )
            {
                if ( sel.hasReadSock(_servSockB) )
                {
                    // 有一个客户连接到来
                    eiennet::ip::EndPoint clientEp;
                    auto clientSockPtr = _servSockB.accept(&clientEp);

                    if ( clientSockPtr )
                    {
                        winux::SharedPointer<ClientCtx> * pClientCtxPtr = nullptr;
                        if ( this->_addClient( clientEp, clientSockPtr, &pClientCtxPtr ) )
                        {
                            if ( this->_verbose ) winux::ColorOutputLine( winux::fgFuchsia, (*pClientCtxPtr)->getStamp(), "新加入服务器" );
                        }
                    }

                    rc--;
                }
                else if ( sel.hasExceptSock(_servSockB) )
                {
                    winux::ScopeGuard guard(this->_mtxServer);
                    _stop = true;

                    rc--;
                }
            }

            // 分发客户连接的相关IO事件
            if ( rc > 0 )
            {
                winux::ScopeGuard guard(this->_mtxServer);

                for ( auto it = this->_clients.begin(); it != this->_clients.end(); )
                {
                    if ( sel.hasReadSock(*it->second->clientSockPtr.get()) ) // 该套接字有数据可读
                    {
                        size_t readableSize = it->second->clientSockPtr->getAvailable();

                        if ( readableSize > 0 )
                        {
                            if ( this->_verbose ) winux::ColorOutputLine( winux::fgWhite, it->second->getStamp(), "有数据到达(bytes:", readableSize, ")" );

                            if ( _isAutoReadData )
                            {
                                // 收数据
                                winux::Buffer data = it->second->clientSockPtr->recv(readableSize);
                                if ( this->_verbose ) winux::ColorOutputLine( winux::fgGreen, it->second->getStamp(), "收到数据:", data.getSize() );

                                // 投递数据到达事件到线程池处理
                                this->_postTask( it->second, &Server::onClientDataArrived, this, it->second, std::move(data) );
                            }
                            else
                            {
                                // 投递数据到达事件到线程池处理
                                this->_postTask( it->second, &Server::onClientDataNotify, this, it->second, readableSize );
                            }
                        }
                        else // readableSize <= 0
                        {
                            if ( this->_verbose ) winux::ColorOutputLine( winux::fgRed, it->second->getStamp(), "有数据到达(bytes:", readableSize, ")，其关闭了连接" );

                            it->second->canRemove = true;
                        }

                        rc--;
                    }
                    else if ( sel.hasExceptSock(*it->second->clientSockPtr.get()) ) // 该套接字有错误
                    {
                        if ( this->_verbose ) winux::ColorOutputLine( winux::fgMaroon, it->second->getStamp(), "出错，标记可移除" );

                        it->second->canRemove = true;

                        rc--;
                    }

                    // 已经没有就绪的套接字，跳出
                    if ( rc == 0 ) break;

                    // 如果已经是end则不能再++it
                    if ( it != this->_clients.end() ) ++it;
                }
            }
        }
        else // rc = wait(), rc <= 0
        {
            // 什么都不做
        }
    }
    _pool.whenEmptyStopAndWait();
    return 0;
}

void Server::stop( bool b )
{
    winux::ScopeGuard guard(_mtxServer);
    static_cast<volatile bool &>(_stop) = b;
}

size_t Server::getClientsCount() const
{
    winux::ScopeGuard guard( const_cast<winux::RecursiveMutex &>(_mtxServer) );
    return _clients.size();
}

void Server::removeClient( winux::uint64 clientId )
{
    winux::ScopeGuard guard(_mtxServer);
    _clients.erase(clientId);
}

bool Server::_canAddClient( ClientCtx * clientCtx )
{
    return true;
}

bool Server::_addClient( ip::EndPoint const & clientEp, winux::SharedPointer<ip::tcp::Socket> clientSockPtr, winux::SharedPointer<ClientCtx> ** ppClientCtxPtr )
{
    ClientCtx * clientCtx = nullptr;
    {
        winux::ScopeGuard guard(_mtxServer);
        ++_cumulativeClientId;
    }

    clientCtx = this->onCreateClient( _cumulativeClientId, clientEp.toString(), clientSockPtr );

    if ( this->_canAddClient(clientCtx) )
    {
        winux::ScopeGuard guard(_mtxServer);
        *ppClientCtxPtr = &_clients[_cumulativeClientId];
        (*ppClientCtxPtr)->attachNew(clientCtx);
        return true;
    }
    else
    {
        delete clientCtx;
        return false;
    }
}

ClientCtx * Server::onCreateClient( winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<ip::tcp::Socket> clientSockPtr )
{
    if ( this->_CreateClientHandler )
    {
        return this->_CreateClientHandler( clientId, clientEpStr, clientSockPtr );
    }
    else
    {
        return new ClientCtx( this, clientId, clientEpStr, clientSockPtr );
    }
}


} // namespace eiennet
