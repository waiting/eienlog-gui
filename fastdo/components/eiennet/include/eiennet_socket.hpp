﻿#ifndef __SOCKET_HPP__
#define __SOCKET_HPP__

namespace eiennet
{
//#define SOCKET_EXCEPTION_USE  // 错误机制是使用错误异常机制，还是使用传统返回值模式

/** \brief Socket库初始化 */
class EIENNET_DLL SocketLib
{
public:
    SocketLib();
    ~SocketLib();

private:
    winux::Members<struct SocketLib_Data> _self;
    DISABLE_OBJECT_COPY(SocketLib)
};

/** \brief 套接字错误 */
class SocketError : public winux::Error
{
public:
    SocketError( int errType, winux::AnsiString const & errStr ) : winux::Error( errType, errStr ) { }
};

class EndPoint;

/** \brief 套接字基础类
 *
 *  若是新对象，套接字会延迟创建。只有当带`EndPoint`参数的方法被调用时才会实际创建套接字。相应的方法有：`bind()`、`connect()`、`sendTo()`。 */
class EIENNET_DLL Socket
{
public:
    // classes and types ----------------------------------------------------------------------

    /** \brief 地址族 */
    enum AddrFamily {
        afUnspec = 0,           /**< Unspecified.  */
        afLocal = 1,            /**< Local to host (pipes and file-domain).  */
        afUnix = afLocal,       /**< POSIX name for PF_LOCAL.  */
        afFile = afLocal,       /**< Another non-standard name for PF_LOCAL.  */
        afInet = 2,             /**< IP protocol family.  */
        afAx25 = 3,             /**< Amateur Radio AX.25.  */
        afIpx = 4,              /**< Novell Internet Protocol.  */
        afAppletalk = 5,        /**< Appletalk DDP.  */
        afNetrom = 6,           /**< Amateur radio NetROM.  */
        afBridge = 7,           /**< Multiprotocol bridge.  */
        afAtmpvc = 8,           /**< ATM PVCs.  */
        afX25 = 9,              /**< Reserved for X.25 project.  */
        afInet6 = 10,           /**< IP version 6.  */
        afRose = 11,            /**< Amateur Radio X.25 PLP.  */
        afDecnet = 12,          /**< Reserved for DECnet project.  */
        afNetbeui = 13,         /**< Reserved for 802.2LLC project.  */
        afSecurity = 14,        /**< Security callback pseudo AF.  */
        afKey = 15,             /**< PF_KEY key management API.  */
        afNetlink = 16,
        afRoute = afNetlink,    /**< Alias to emulate 4.4BSD.  */
        afPacket = 17,          /**< Packet family.  */
        afAsh = 18,             /**< Ash.  */
        afEconet = 19,          /**< Acorn Econet.  */
        afAtmsvc = 20,          /**< ATM SVCs.  */
        afRds = 21,             /**< RDS sockets.  */
        afSna = 22,             /**< Linux SNA Project */
        afIrda = 23,            /**< IRDA sockets.  */
        afPppox = 24,           /**< PPPoX sockets.  */
        afWanpipe = 25,         /**< Wanpipe API sockets.  */
        afLlc = 26,             /**< Linux LLC.  */
        afUnknown27 = 27,
        afUnknown28 = 28,
        afCan = 29,             /**< Controller Area Network.  */
        afTipc = 30,            /**< TIPC sockets.  */
        afBluetooth = 31,       /**< Bluetooth sockets.  */
        afIucv = 32,            /**< IUCV sockets.  */
        afRxrpc = 33,           /**< RxRPC sockets.  */
        afIsdn = 34,            /**< mISDN sockets.  */
        afPhonet = 35,          /**< Phonet sockets.  */
        afIeee802154 = 36,      /**< IEEE 802.15.4 sockets.  */
        afMax = 37              /**< For now..  */
    };

    /** \brief 套接字类型 */
    enum SockType {
        sockUnknown,
        sockStream,     /**< Sequenced, reliable, connection-based byte streams.  */
        sockDatagram,   /**< Connectionless, unreliable datagrams of fixed maximum length.  */
        sockRaw,        /**< Raw protocol interface.  */
        sockRdm,        /**< Reliably-delivered messages.  */
        sockSeqPacket,  /**< Sequenced, reliable, connection-based, datagrams of fixed maximum length.  */
        sockDccp,       /**< Datagram Congestion Control Protocol.  */
        sockPacket,     /**< Linux specific way of getting packets at the dev level.  For writing rarp and
                               other similar things on the user level. */

        /* Flags to be ORed into the type parameter of socket and socketpair and used for the flags parameter of paccept.  */

        sockCloexec,    /**< Atomically set close-on-exec flag for the new descriptor(s).  */
        sockNonblock    /**< Atomically mark descriptor(s) as non-blocking.  */
    };

    /** \brief 协议 */
    enum Protocol {
        protoUnspec = 0, protoIp = 0,    /**< Dummy protocol for TCP.  */
        protoIcmp,      /**< Internet Control Message Protocol.  */
        protoIgmp,      /**< Internet Group Management Protocol. */
        protoIpip, protoIpv4 = protoIpip, /**< IPIP tunnels (older KA9Q tunnels use 94).  */
        protoTcp,       /**< Transmission Control Protocol.  */
        protoEgp,       /**< Exterior Gateway Protocol.  */
        protoPup,       /**< PUP protocol.  */
        protoUdp,       /**< User Datagram Protocol.  */
        protoIdp,       /**< XNS IDP protocol.  */
        protoTp,        /**< SO Transport Protocol Class 4.  */
        protoDccp,      /**< Datagram Congestion Control Protocol.  */
        protoIpv6,      /**< IPv6 header.  */
        protoRsvp,      /**< Reservation Protocol.  */
        protoGre,       /**< General Routing Encapsulation.  */
        protoEsp,       /**< encapsulating security payload.  */
        protoAh,        /**< authentication header.  */
        protoMtp,       /**< Multicast Transport Protocol.  */
        protoBeetph,    /**< IP option pseudo header for BEET.  */
        protoEncap,     /**< Encapsulation Header.  */
        protoPim,       /**< Protocol Independent Multicast.  */
        protoComp,      /**< Compression Header Protocol.  */
        protoSctp,      /**< Stream Control Transmission Protocol.  */
        protoUdplite,   /**< UDP-Lite protocol.  */
        protoRaw,       /**< Raw IP packets.  */
        protoMax
    };

public:
    // static members -------------------------------------------------------------------------
    // send/recv's message flags
    static int const MsgDefault;
#if defined(_MSC_VER) || defined(WIN32)
    static int const MsgOob;
    static int const MsgPeek;
    static int const MsgDontRoute;
    static int const MsgWaitAll;
    static int const MsgPartial;
    static int const MsgInterrupt;
    static int const MsgMaxIovLen;
#else
    static int const MsgOob;
    static int const MsgPeek;
    static int const MsgDontRoute;
#   ifdef __USE_GNU
    /* DECnet uses a different name.  */
    static int const MsgTryHard;
#   endif
    static int const MsgCTrunc;
    static int const MsgProxy;
    static int const MsgTrunc;
    static int const MsgDontWait;
    static int const MsgEor;
    static int const MsgWaitAll;
    static int const MsgFin;
    static int const MsgSyn;
    static int const MsgConfirm;
    static int const MsgRst;
    static int const MsgErrQueue;
    static int const MsgNoSignal;
    static int const MsgMore;
    static int const MsgWaitForOne;
    static int const MsgCMsgCloexec;

#endif
    // shutdown's how flags
    static int const SdReceive;
    static int const SdSend;
    static int const SdBoth;

    typedef std::function< void ( size_t hadBytes, void * param ) > FunctionSuccessCallback;
public:
    // constructor/destructor -----------------------------------------------------------------

    /** \brief 构造函数1，包装现有socket描述符
     *
     *  \param sock socket描述符(Windows平台是socket句柄)
     *  \param isNewSock 指示是否为新建socket。如果为true，则会在Socket摧毁时自动close(sock) */
    explicit Socket( int sock = -1, bool isNewSock = false );

    /** \brief 构造函数2，指定socket的'地址簇'，'类型'，'协议' */
    Socket( AddrFamily af, SockType sockType, Protocol proto );

#ifndef MOVE_SEMANTICS_DISABLED
    /** \brief 移动构造函数 */
    Socket( Socket && other );
    /** \brief 移动赋值操作 */
    Socket & operator = ( Socket && other );
#endif

    /** \brief 析构函数 */
    virtual ~Socket();

public:
    // methods --------------------------------------------------------------------------------
    /** \brief 获取Socket的create()参数：'地址簇' */
    AddrFamily getAddrFamily() const;
    /** \brief 指定Socket的create()参数：'地址簇' */
    void setAddrFamily( AddrFamily af );

    /** \brief 获取Socket的create()参数：'类型' */
    SockType getSockType() const;
    /** \brief 指定Socket的create()参数：'类型' */
    void setSockType( SockType sockType );

    /** \brief 获取Socket的create()参数：'协议' */
    Protocol getProtocol() const;
    /** \brief 指定Socket的create()参数：'协议' */
    void setProtocol( Protocol proto );

    /** \brief 获取Socket的create()参数：'地址簇'，'类型'，'协议' */
    void getParams( AddrFamily * af, SockType * sockType, Protocol * proto );
    /** \brief 指定Socket的create()参数：'地址簇'，'类型'，'协议' */
    void setParams( AddrFamily af, SockType sockType, Protocol proto );

    /** \brief 根据'地址簇'，'类型'，'协议'创建一个socket */
    bool create( AddrFamily af, SockType sockType, Protocol proto );
    /** \brief 根据内部存储的'地址簇'，'类型'，'协议'创建一个socket */
    bool create();

protected:
    /** \brief 如果未创建Socket则尝试创建。如果创建成功或已创建则返回true，如果创建失败则返回false
     *
     *  可传递指定参数，若不传递则用默认参数 */
    bool _tryCreate( AddrFamily af, bool hasAf, SockType sockType, bool hasSockType, Protocol proto, bool hasProto );

public:
    /** \brief 关闭socket描述符 */
    int close();

    /** \brief 关掉socket的相应操作，但并不会close套接字。
     *
     *  `Socket#SdReceive`:关掉接收操作，`Socket#SdSend`:关掉发送操作，`Socket#SdBoth`:都关掉 */
    int shutdown( int how = SdSend );

    /** \brief 发送数据。返回已发送大小，出错返回-1。 */
    int send( void const * data, size_t size, int msgFlags = MsgDefault );
    /** \brief 发送数据。返回已发送大小，出错返回-1。 */
    int send( winux::AnsiString const & data, int msgFlags = MsgDefault ) { return this->send( data.c_str(), data.size(), msgFlags ); }
    /** \brief 发送数据。返回已发送大小，出错返回-1。 */
    int send( winux::Buffer const & data, int msgFlags = MsgDefault ) { return this->send( data.getBuf(), data.getSize(), msgFlags ); }

    /** \brief 发送数据，直到发送完指定大小的数据。
     *
     *  如果发送指定大小的数据成功返回true，否则返回false（可能是连接关闭或出错了）。 */
    bool sendUntil( size_t targetSize, void const * data, int msgFlags = MsgDefault );
    /** \brief 发送字符串，直到发送完该长度的字符串。
     *
     *  如果发送该长度的字符串成功返回true，否则返回false（可能是连接关闭或出错了）。 */
    bool sendUntil( winux::AnsiString const & data, int msgFlags = MsgDefault ) { return this->sendUntil( data.size(), data.c_str(), msgFlags ); }
    /** \brief 发送缓冲区，直到发送完该大小的缓冲区。
     *
     *  如果发送该大小的缓冲区成功返回true，否则返回false（可能是连接关闭或出错了）。 */
    bool sendUntil( winux::Buffer const & data, int msgFlags = MsgDefault ) { return this->sendUntil( data.getSize(), data.getBuf(), msgFlags ); }

    /** \brief 发送数据，直到发送完指定大小的数据或超时。
     *
     *  *hadSent表示已发送的数据请初始化为0；；sec表示超时值，sec<0则一直等待。\n
     *  *rcWait接收selec()的返回代码：*rcWait>0:表示可发送数据；*rcWait==0:表示超时；*rcWait<0:表示select()出错。\n
     *  只有当*rcWait>0时才会发送数据。\n
     *  返回一次send的数据大小；如果出错返回-1，具体错误查看错误代码getError()/ErrNo()。\n
     *  判断是否发送完全，需检测*hadSent==targetSize，（注：send()的默认行为是拷贝到socket发送缓冲区，可将发送缓冲区设置为0大小，send()立即发送数据）。\n
     *  eachSuccessCallback每次成功会调用的回调函数。 */
    int sendWaitUntil(
        size_t targetSize,
        void const * data,
        size_t * hadSent,
        double sec,
        int * rcWait,
        FunctionSuccessCallback eachSuccessCallback = FunctionSuccessCallback(),
        void * param = nullptr,
        int msgFlags = MsgDefault
    );

    int sendWaitUntil(
        winux::AnsiString const & data,
        size_t * hadSent,
        double sec,
        int * rcWait,
        FunctionSuccessCallback eachSuccessCallback = FunctionSuccessCallback(),
        void * param = nullptr,
        int msgFlags = MsgDefault
    ) { return this->sendWaitUntil( data.size(), data.c_str(), hadSent, sec, rcWait, std::move(eachSuccessCallback), param, msgFlags ); }

    int sendWaitUntil(
        winux::Buffer const & data,
        size_t * hadSent,
        double sec,
        int * rcWait,
        FunctionSuccessCallback eachSuccessCallback = FunctionSuccessCallback(),
        void * param = nullptr,
        int msgFlags = MsgDefault
    ) { return this->sendWaitUntil( data.getSize(), data.getBuf(), hadSent, sec, rcWait, std::move(eachSuccessCallback), param, msgFlags ); }

    /** \brief 发送一个`Plain of Data`类型的变量，若成功返回true，否则返回false。 */
    template < typename _PodType >
    bool sendUntilType( _PodType const & v, size_t size = sizeof(_PodType), int msgFlags = MsgDefault ) { return this->sendUntil( size, &v, msgFlags ); }

    /** \brief 尝试接收size大小数据。返回实际接收的数据大小，出错返回-1。 */
    int recv( void * buf, size_t size, int msgFlags = MsgDefault );

    /** \brief 尝试接收size大小数据，返回实际收到的数据Buffer。
     *
     *  返回的Buffer有三种状态：\n
     *  1、接收到数据，此时(bool)Buffer==true 且 Buffer.getSize() > 0；\n
     *  2、接收到了0字节数据，此时(bool)Buffer==true 且 Buffer.getSize()==0；\n
     *  3、recv()发生错误，此时(bool)Buffer==false */
    winux::Buffer recv( size_t size, int msgFlags = MsgDefault );

    /** \brief 接收数据，直到碰到target指定的数据。data返回接收到的数据，data里可以已有部分数据，extraData返回额外接收的数据。
     *
     *  如果碰到target指定的数据成功返回true，否则返回false（可能是连接关闭或出错了）。 */
    bool recvUntilTarget( winux::AnsiString const & target, winux::GrowBuffer * data, winux::GrowBuffer * extraData, int msgFlags = MsgDefault );

    /** \brief 接收数据，直到碰到target指定的数据或者超时。
     *
     *  *startpos表示搜索起始位置并返回下一次搜索位置；*pos返回搜到指定数据的位置，data返回接收到的数据，data里可已有数据；extraData返回额外接收的数据；sec表示超时值，sec<0则一直等待。\n
     *  请将*startpos赋初值0，*pos赋初值-1。\n
     *  *rcWait接收selec()的返回代码：*rcWait>0:表示有数据到达；*rcWait==0:表示超时；*rcWait<0:表示select()出错。\n
     *  只有当*rcWait>0时才会接收数据，数据可能是0大小，表示连接关闭信号。\n
     *  返回一次recv的数据大小；如果返回0，需检测*rcWait值（若*rcWait>0则表示连接关闭）；如果出错返回-1。\n
     *  判断是否接收完全，需检测*pos!=-1。\n
     *  eachSuccessCallback每次成功会调用的回调函数。 */
    int recvWaitUntilTarget(
        winux::AnsiString const & target,
        winux::GrowBuffer * data,
        winux::GrowBuffer * extraData,
        size_t * hadRead,
        size_t * startpos,
        size_t * pos,
        double sec,
        int * rcWait,
        FunctionSuccessCallback eachSuccessCallback = FunctionSuccessCallback(),
        void * param = nullptr,
        int msgFlags = MsgDefault
    );

    /** \brief 接收数据，直到收到指定大小的数据。data返回接收到的数据。
     *
     *  如果data==nullptr，则丢弃数据。
     *  如果收到指定大小的数据成功返回true，否则返回false（可能是连接关闭或出错了）。 */
    bool recvUntilSize( size_t targetSize, winux::GrowBuffer * data, int msgFlags = MsgDefault );

    /** \brief 接收数据，直到收到指定大小的数据或者超时。
     *
     *  如果data==nullptr，则丢弃数据。
     *  *hadRead表示已读的数据请初始设为0；data返回接收到的数据；sec表示超时值，sec<0则一直等待。\n
     *  *rcWait接收selec()的返回代码：*rcWait>0:表示有数据到达；*rcWait==0:表示超时；*rcWait<0:表示select()出错。\n
     *  只有当*rcWait>0时才会接收数据，数据可能是0大小，表示连接关闭信号。\n
     *  返回一次recv的数据大小；如果返回0，需检测*rcWait值（若*rcWait>0则表示连接关闭）；如果出错返回-1。\n
     *  判断是否接收完全，需检测*hadRead==targetSize。\n
     *  eachSuccessCallback每次成功会调用的回调函数。 */
    int recvWaitUntilSize(
        size_t targetSize,
        winux::GrowBuffer * data,
        size_t * hadRead,
        double sec,
        int * rcWait,
        FunctionSuccessCallback eachSuccessCallback = FunctionSuccessCallback(),
        void * param = nullptr,
        int msgFlags = MsgDefault
    );

    /** \brief 接收一个`Plain of Data`类型的变量，若成功返回true，否则返回false。 */
    template < typename _PodType >
    bool recvUntilType( _PodType * v, size_t size = sizeof(_PodType), int msgFlags = MsgDefault )
    {
        winux::GrowBuffer data;
        data.setBuf( v, 0, size, true );
        return this->recvUntilSize( size, &data, msgFlags );
    }

    /** \brief 接收不用阻塞即可接收的数据，返回收到的数据Buffer。
     *
     *  返回的Buffer有三种状态：\n
     *  1、接收到数据，此时(bool)Buffer==true 且 Buffer.getSize() > 0；\n
     *  2、接收到了0字节数据，此时(bool)Buffer==true 且 Buffer.getSize()==0；\n
     *  3、recv()发生错误，此时(bool)Buffer==false */
    winux::Buffer recvAvail( int msgFlags = MsgDefault );

    /** \brief 接收已到达的数据，如果没有到达数据则等待有数据到达或超过指定时间，返回收到的数据Buffer。
     *
     *  *rcWait接收selec()的返回代码：*rcWait>0:表示有数据到达；*rcWait==0:表示超时；*rcWait<0:表示select()出错。\n
     *  只有当*rcWait>0时才会接收数据，数据可能是0大小，表示连接关闭信号。\n
     *  返回的Buffer有三种状态：\n
     *  1、接收到数据，此时(bool)Buffer==true 且 Buffer.getSize() > 0；\n
     *  2、接收到了0字节数据，此时(bool)Buffer==true 且 Buffer.getSize()==0，也可能是等待超时，需用*rcWait的值判断；\n
     *  3、recv()发生错误，此时(bool)Buffer==false */
    winux::Buffer recvWaitAvail( double sec, int * rcWait, int msgFlags = MsgDefault );

    /** \brief 无连接模式发送数据到指定端点。返回已发送大小，出错返回-1。若套接字尚未创建则创建套接字 */
    int sendTo( EndPoint const & ep, void const * data, size_t size, int msgFlags = MsgDefault );
    /** \brief 无连接模式发送数据到指定端点。返回已发送大小，出错返回-1。 */
    int sendTo( EndPoint const & ep, winux::AnsiString const & data, int msgFlags = MsgDefault ) { return this->sendTo( ep, data.c_str(), data.size(), msgFlags ); }
    /** \brief 无连接模式发送数据到指定端点。返回已发送大小，出错返回-1。 */
    int sendTo( EndPoint const & ep, winux::Buffer const & data, int msgFlags = MsgDefault ) { return this->sendTo( ep, data.getBuf(), data.getSize(), msgFlags ); }

    /** \brief 无连接模式接收数据。返回已接收的大小，出错返回-1。
     *
     *  必须先调用`bind()`绑定地址，收到数据后ep会返回发送方的端点信息。 */
    int recvFrom( EndPoint * ep, void * buf, size_t size, int msgFlags = MsgDefault );
    /** \brief 无连接模式接收数据。返回实际收到的数据Buffer。
     *
     *  必须先调用`bind()`绑定地址，收到数据后ep会返回发送方的端点信息。\n
     *  返回的Buffer有三种状态：\n
     *  1、接收到数据，此时(bool)Buffer==true 且 Buffer.getSize() > 0；\n
     *  2、接收到了0字节数据，此时(bool)Buffer==true 且 Buffer.getSize()==0；\n
     *  3、recv()发生错误，此时(bool)Buffer==false */
    winux::Buffer recvFrom( EndPoint * ep, size_t size, int msgFlags = MsgDefault );

    /** \brief 连接服务器。若套接字尚未创建则创建套接字 */
    bool connect( EndPoint const & ep );

    /** \brief 绑定地址。若套接字尚未创建则创建套接字 */
    bool bind( EndPoint const & ep );

    /** \brief 监听 */
    bool listen( int backlog );

    /** \brief 接受一个客户连接
     *
     *  成功则*sock输出Socket句柄，调用者负责`close()` */
    bool accept( int * sock, EndPoint * ep = NULL );

    /** \brief 接受一个客户连接 */
    winux::SharedPointer<Socket> accept( EndPoint * ep = NULL )
    {
        int sock;
        return this->accept( &sock, ep ) ? winux::SharedPointer<Socket>( new Socket( sock, true ) ) : winux::SharedPointer<Socket>();
    }

    /** \brief 获取已绑定的`EndPoint` */
    bool getBoundEp( EndPoint * ep ) const;

    // socket's options -----------------------------------------------------------------------

    /** \brief 获取接收缓冲区大小 */
    int getRecvBufSize() const;
    /** \brief 设置接收缓冲区大小 */
    bool setRecvBufSize( int optval );

    /** \brief 获取发送缓冲区大小 */
    int getSendBufSize() const;
    /** \brief 设置发送缓冲区大小 */
    bool setSendBufSize( int optval );

    /** \brief 获取接收超时(ms) */
    winux::uint32 getRecvTimeout() const;
    /** \brief 设置接收超时(ms) */
    bool setRecvTimeout( winux::uint32 optval );

    /** \brief 获取发送超时(ms) */
    winux::uint32 getSendTimeout() const;
    /** \brief 设置发送超时(ms)
     *
     *  在Windows上，发送似乎由系统后台进行，`send()`总是立即返回全部发送的数据大小，设置超时似乎无用。 */
    bool setSendTimeout( winux::uint32 optval );

    /** \brief 获取是否开启了重用地址 */
    bool getReUseAddr() const;
    /** \brief 设置socket是否重用地址，默认false不重用 */
    bool setReUseAddr( bool optval );

    /** \brief 获取是否启用广播 */
    bool getBroadcast() const;
    /** \brief 设置socket是否广播，默认false非广播 */
    bool setBroadcast( bool optval );

    /** \brief 获取IPV6套接字是否只启用IPV6功能，Windows默认true，Linux默认false */
    bool getIpv6Only() const;
    /** \brief 设置IPV6套接字是否只启用IPV6功能，Windows默认true，Linux默认false */
    bool setIpv6Only( bool optval );

    /** \brief 通过getsockopt()+SO_ERROR获取仅属于socket的错误 */
    int getError() const;

    /** \brief 获取socket类型 */
    SockType getType() const;

    /** \brief socket是否为监听模式
     *
     *  默认从getsockopt(SO_ACCEPTCONN)读取 */
    bool isListening() const;

    // ioctls ---------------------------------------------------------------------------------

    /** \brief 获取可不阻塞接收的数据量 */
    int getAvailable() const;

    /** \brief 设置socket阻塞模式，true为阻塞，false为非阻塞。 */
    bool setBlocking( bool blocking );

    // attributes -----------------------------------------------------------------------------

    /** \brief Windows:socket句柄，或Linux:socket描述符 */
    int get() const;

    /** \brief 判断Socket是否有效 */
    operator bool() const { return this->get() > -1; }

    // static ---------------------------------------------------------------------------------

    /** \brief 从errno获取错误码 */
    static int ErrNo();

protected:
    // 初始化全部成员
    void _membersInit()
    {
        this->_addrFamily = afUnspec;
        this->_sockType = sockUnknown;
        this->_protocol = protoUnspec;

        this->_attrBlocking = true;
        this->_attrBroadcast = false;
        this->_attrReUseAddr = false;
        this->_attrSendTimeout = 0U;
        this->_attrRecvTimeout = 0U;
        this->_attrSendBufSize = 0;
        this->_attrRecvBufSize = 0;
    #if defined(OS_WIN)
        this->_attrIpv6Only = true;   // IPV6套接字只开启IPV6功能
    #else
        this->_attrIpv6Only = false;  // IPV6套接字只开启IPV6功能
    #endif

        this->_resetManaged();
    }

    // 重置socket资源管理相关变量
    void _resetManaged()
    {
        this->_sock = -1;
        this->_isNewSock = false;
    }

    // 延迟创建socket使用参数
    AddrFamily _addrFamily;  // 地址族
    SockType _sockType;      // 套接字类型
    Protocol _protocol;      // 协议

    // 延迟设置socket属性
    winux::uint32 _attrSendTimeout; // 发送超时(ms)
    winux::uint32 _attrRecvTimeout; // 接收超时(ms)
    int _attrSendBufSize;   // 发送缓冲区大小
    int _attrRecvBufSize;   // 接收缓冲区大小
    bool _attrBlocking;     // 是否阻塞
    bool _attrBroadcast;    // 是否启用广播
    bool _attrReUseAddr;    // 是否开启了地址重用
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
    int _sock;           // socket描述符
    bool _isNewSock;     // 指示是否为新建socket。如果为true，则会在Socket对象析构时自动关闭sock

    DISABLE_OBJECT_COPY(Socket)
};

/** \brief 端点基类(套接字地址对象基类) */
class EIENNET_DLL EndPoint
{
public:
    EndPoint() { }
    virtual ~EndPoint() { }

    /** \brief 以`void*`形式取得内部的`sockaddr_?`结构体指针 */
    virtual void * get() const = 0;
    /** \brief 以`_Ty*`形式取得内部的`sockaddr_?`结构体指针 */
    template < typename _Ty >
    _Ty * get() const { return reinterpret_cast<_Ty *>( this->get() ); }
    /** \brief 取得地址的数据大小，一般为内部地址结构体的大小 */
    virtual winux::uint & size() const = 0;
    /** \brief 把地址转换成一个字符串 */
    virtual winux::String toString() const = 0;
    /** \brief 克隆一个EndPoint */
    virtual EndPoint * clone() const = 0;
    /** \brief 获取地址簇 */
    virtual Socket::AddrFamily getAddrFamily() const = 0;
};

/** \brief 数据收发场景，存放数据收发过程中的一些变量 */
struct EIENNET_DLL DataRecvSendCtx
{
    enum
    {
        RetryCount = 10 //!< 默认重试次数
    };

    winux::GrowBuffer data; //!< 数据
    size_t startpos;        //!< 起始位置
    size_t pos;             //!< 找到位置
    size_t hadBytes;        //!< 已接收/发送数据量
    size_t targetBytes;     //!< 目标数据量
    size_t retryCount;      //!< 已重试次数

    DataRecvSendCtx();

    DataRecvSendCtx(
        winux::Buffer const & data,
        size_t hadBytes = 0,
        size_t targetBytes = 0
    );

    DataRecvSendCtx(
        winux::Buffer && data,
        size_t hadBytes = 0,
        size_t targetBytes = 0
    );

    /** \brief 重置数据为空 */
    void resetData();

    /** \brief 重置状态 */
    void resetStatus();

    /** \brief 添加数据到data */
    void append( winux::Buffer const & data );

    /** \brief 在`data`里查找`target`内容。`startpos`指定起始位置，`pos`接收搜索到的位置。
     *
     *  如果没找到，自动设置`startpos`为下次搜索起始位置 */
    template < typename _IndexType >
    bool find( winux::AnsiString const & target, std::vector<_IndexType> const & targetNextVal )
    {
        // 如果接收到的数据小于标记长度 或者 搜不到标记 则退出
        if ( this->data.size() - this->startpos < target.size() || ( this->pos = winux::_Templ_KmpMatchEx( this->data.get<char>(), this->data.size(), target.c_str(), target.size(), this->startpos, targetNextVal ) ) == winux::npos )
        {
            if ( this->data.size() >= target.size() ) this->startpos = this->data.size() - target.size() + 1; // 计算下次搜索起始
            return false;
        }
        else
        {
            return true;
        }
    }

    /** \brief 在`find()`到目标内容后，从`data`中取出指定大小的数据，并保留其余数据，最后重置状态。
     *
     *  \param extractDataSize 指定取出数据大小
     *  \param limitSpaceSize 限制空闲大小，超过这个大小的`data`空闲空间将会收缩 */
    winux::Buffer extract( size_t extractDataSize, size_t limitSpaceSize = 1024 );
};

///////////////////////////////////////////////////////////////////////////////////////////////
/** \brief 套接字流缓冲区 */
class EIENNET_DLL SocketStreamBuf : public std::streambuf
{
public:
    SocketStreamBuf(
        eiennet::Socket * sock,
        std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out,
        size_t inputBufSize = (size_t)-1,
        size_t outputBufSize = (size_t)-1
    );

    virtual ~SocketStreamBuf();

    Socket * getSocket() const { return _sock; }
    // input buffer begin ---------------------------------------------------------------------
protected:
    virtual int_type underflow();
    // input buffer end -----------------------------------------------------------------------

    // output buffer begin --------------------------------------------------------------------
protected:
    virtual int_type overflow( int_type c );
    virtual int sync();
    int _flush();
    // output buffer end ----------------------------------------------------------------------


private:
    eiennet::Socket * _sock;
    winux::Buffer _inputBuf;
    winux::Buffer _outputBuf;

    DISABLE_OBJECT_COPY(SocketStreamBuf)
};

/** \brief 套接字输出流 */
class EIENNET_DLL SocketStreamOut : public std::ostream
{
public:
    SocketStreamOut( SocketStreamBuf * sockBuf ) : std::ostream(sockBuf), _sockBuf(sockBuf) { }
    SocketStreamOut( SocketStreamBuf & sockBuf ) : std::ostream(&sockBuf), _sockBuf(&sockBuf) { }
    SocketStreamOut( winux::SimplePointer<SocketStreamBuf> & sockBuf ) : std::ostream(sockBuf.get()), _sockBuf(sockBuf.get()) { }
    SocketStreamOut( winux::SharedPointer<SocketStreamBuf> & sockBuf ) : std::ostream(sockBuf.get()), _sockBuf(sockBuf.get()) { }

    /** \brief 写数据并flush流缓冲区 */
    SocketStreamOut & writeAndFlush( winux::Buffer const & data );

private:
    SocketStreamBuf * _sockBuf;
};

/** \brief 套接字输入流 */
class EIENNET_DLL SocketStreamIn : public std::istream
{
public:
    SocketStreamIn( SocketStreamBuf * sockBuf ) : std::istream(sockBuf), _sockBuf(sockBuf) { }
    SocketStreamIn( SocketStreamBuf & sockBuf ) : std::istream(&sockBuf), _sockBuf(&sockBuf) { }
    SocketStreamIn( winux::SimplePointer<SocketStreamBuf> & sockBuf ) : std::istream(sockBuf.get()), _sockBuf(sockBuf.get()) { }
    SocketStreamIn( winux::SharedPointer<SocketStreamBuf> & sockBuf ) : std::istream(sockBuf.get()), _sockBuf(sockBuf.get()) { }

    /** \brief 流缓冲区内剩余的数据大小 + 套接字系统缓冲区内剩余的数据大小 */
    std::streamsize getAvailable() const;
    /** \brief 读取可不阻塞取得的数据 */
    SocketStreamIn & readAvail( winux::Buffer * data );
    /** \brief 读取指定大小的数据 */
    SocketStreamIn & read( winux::Buffer * data, size_t size );
    /** \brief 等待有数据到达，返回可无阻塞读取的数据量，若返回0超时，若返回<0出错 */
    std::streamsize waitAvail( double sec );

private:
    SocketStreamBuf * _sockBuf;
};

///////////////////////////////////////////////////////////////////////////////////////////////
/** \brief IP地址族套接字 */
namespace ip
{
/** \brief IP端点对象 */
class EIENNET_DLL EndPoint : public eiennet::EndPoint
{
public:
    /** \brief 从已绑定地址的套接字上获取绑定的`EndPoint` */
    static EndPoint FromBound( Socket const * sock );

    /** \brief 构造函数0，通过`sockaddr`构建EP */
    EndPoint( void const * sockaddr, size_t len );
    /** \brief 构造函数1，通过地址族构建EP */
    EndPoint( Socket::AddrFamily af = Socket::afUnspec );
    /** \brief 构造函数2，ipAndPort可以是下面几种类型："IPv4:port"、"[IPv6]:port"、[ "IP", port ]、{ "IP" : port }。 */
    EndPoint( winux::Mixed const & ipAndPort );
    /** \brief 构造函数3，分别指定IP地址和端口号
     *
     *  addr为""则视为IPv4(0.0.0.0)，为"[]"则视为IPv6(0:0:0:0:0:0:0:0) */
    EndPoint( winux::tchar const * ipAddr, winux::ushort port );
    /** \brief 构造函数4，分别指定IP地址和端口号
     *
     *  ipAddr为""则视为IPv4(0.0.0.0)，为"[]"则视为IPv6(0:0:0:0:0:0:0:0) */
    EndPoint( winux::String const & ipAddr, winux::ushort port );

    EndPoint( EndPoint const & other );
    EndPoint & operator = ( EndPoint const & other );

#ifndef MOVE_SEMANTICS_DISABLED
    EndPoint( EndPoint && other );
    EndPoint & operator = ( EndPoint && other );
#endif

    virtual ~EndPoint();

    /** \brief 初始化0 */
    void init( void const * sockaddr, size_t len );
    /** \brief 初始化1 */
    void init( Socket::AddrFamily af = Socket::afUnspec );
    /** \brief 初始化2，ipAndPort可以是下面几种类型："IPv4:port"、"[IPv6]:port"、[ "IP", port ]、{ "IP" : port }。 */
    void init( winux::Mixed const & ipAndPort );
    /** \brief 初始化3，分别指定IP地址和端口号
     *
     *  ipAddr为""则视为IPv4(0.0.0.0)，为"[]"则视为IPv6(0:0:0:0:0:0:0:0) */
    void init( winux::String const & ipAddr, winux::ushort port );

    /** \brief 以`void*`形式取得内部的`sockaddr_?`结构体指针 */
    virtual void * get() const override;
    /** \brief 以`_Ty*`形式取得内部的`sockaddr_?`结构体指针 */
    template < typename _Ty >
    _Ty * get() const { return reinterpret_cast<_Ty *>( this->get() ); }
    /** \brief 取得内部的`sockaddr`结构大小 */
    virtual winux::uint & size() const override;
    /** \brief 转换成"IP:port"的字符串形式 */
    virtual winux::String toString() const override;
    /** \brief 克隆一个EndPoint */
    virtual eiennet::EndPoint * clone() const override;
    /** \brief 获取地址簇 */
    virtual Socket::AddrFamily getAddrFamily() const override;

    /** \brief 转换成Mixed类型，一个Collection：{ "IP" : port } */
    operator winux::Mixed() const;

    /** \brief 获取IP字符串 */
    winux::String getIp() const;
    /** \brief 获取端口号 */
    winux::ushort getPort() const;

private:
    winux::PlainMembers<struct EndPoint_Data, 32> _self;
};

/** \brief 主机名解析器（可以把域名解析为一个或多个IP端点） */
class EIENNET_DLL Resolver
{
public:
    typedef std::vector<ip::EndPoint> EndPointArray;
    /** \brief 构造函数1，hostAndPort可以是下面几种类型："hostname:port"、[ "hostname", port ]、{ "hostname" : port } */
    Resolver( winux::Mixed const & hostAndPort );
    /** \brief 构造函数2，分别指定主机名和端口号 */
    Resolver( winux::String const & hostName, winux::ushort port );

    EndPointArray::iterator begin() { return _epArr.begin(); }
    EndPointArray::const_iterator begin() const { return _epArr.begin(); }
    EndPointArray::iterator end() { return _epArr.end(); }
    EndPointArray::const_iterator end() const { return _epArr.end(); }

    /** \brief 获取解析到的IP端点数 */
    size_t count() const { return _epArr.size(); }
    /** \brief 获取主机名 */
    winux::String const & getHostname() const { return _hostName; }
    /** \brief 获取端口号 */
    winux::ushort getPort() const { return _port; }

    EndPointArray::value_type const & operator [] ( size_t i ) const { return _epArr[i]; }
    EndPointArray::value_type & operator [] ( size_t i ) { return _epArr[i]; }

    EndPointArray::value_type const & at( size_t i ) const { return _epArr[i]; }
    EndPointArray::value_type & at( size_t i ) { return _epArr[i]; }

    EndPointArray & epArr() { return _epArr; }
    EndPointArray const & epArr() const { return _epArr; }

    /** \brief 转换成"hostname:port"的字符串形式 */
    virtual winux::String toString() const;
    /** \brief 转换成Mixed类型，一个Collection：{ "hostname:port" : [ { "IPv4" : port }, ... ] } */
    operator winux::Mixed() const;

private:
    size_t _resolve( winux::String const & hostName, winux::ushort port );
    winux::String _hostName;
    winux::ushort _port;
    EndPointArray _epArr;
};

namespace tcp
{
/** \brief TCP/IP套接字 */
class EIENNET_DLL Socket : public eiennet::Socket
{
public:
    typedef eiennet::Socket BaseClass;

    /** \brief 构造函数1，包装现有socket描述符
     *
     *  \param sock socket描述符(Windows平台是socket句柄)
     *  \param isNewSock 指示是否为新建socket。如果为true，则会在Socket destroy时自动close(sock) */
    explicit Socket( int sock, bool isNewSock = false ) : BaseClass( sock, isNewSock ) { }

    /** \brief 构造函数2. */
    Socket() : BaseClass( BaseClass::afInet, BaseClass::sockStream, BaseClass::protoUnspec ) { }

    /** \brief 接受一个客户连接 */
    winux::SharedPointer<Socket> accept( EndPoint * ep = NULL )
    {
        int sock;
        return BaseClass::accept( &sock, ep ) ? winux::SharedPointer<Socket>( new Socket( sock, true ) ) : winux::SharedPointer<Socket>();
    }
};

/** \brief 阻塞模式Socket连接尝试，连接成功返回0，超时返回1，失败返回-1 */
EIENNET_FUNC_DECL(int) ConnectAttempt( Socket * sock, EndPoint const & ep, winux::uint32 timeoutMs );

/** \brief 阻塞模式Socket连接尝试，连接成功返回0，超时返回1，失败返回-1
 *
 *  \param sock Socket*
 *  \param resolver Resolver
 *  \param perCnnTimeoutMs uint32 表示每次尝试连接设定的超时值
 *  \return int */
EIENNET_FUNC_DECL(int) ConnectAttempt( Socket * sock, Resolver const & resolver, winux::uint32 perCnnTimeoutMs );

} // namespace tcp

namespace udp
{
/** \brief UDP/IP套接字 */
class EIENNET_DLL Socket : public eiennet::Socket
{
public:
    typedef eiennet::Socket BaseClass;

    /** \brief 构造函数1，包装现有socket描述符
     *
     *  \param sock socket描述符(Windows平台是socket句柄)
     *  \param isNewSock 指示是否为新建socket。如果为true，则会在Socket destroy时自动close(sock) */
    explicit Socket( int sock, bool isNewSock = false ) : BaseClass( sock, isNewSock ) { }

    /** \brief 构造函数2. */
    Socket() : BaseClass( BaseClass::afInet, BaseClass::sockDatagram, BaseClass::protoUnspec ) { }
};


} // namespace udp


} // namespace ip


} // namespace eiennet


#endif // __SOCKET_HPP__
