#ifndef __EIENNET_SERVER_HPP__
#define __EIENNET_SERVER_HPP__

namespace eiennet
{
class Server;

/** \brief 基础客户场景类 */
class EIENNET_DLL ClientCtx
{
public:
    ClientCtx( Server * server, winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr );

    virtual ~ClientCtx();

    /** \brief 客户端戳 */
    winux::String getStamp() const;

    /** \brief 投递发送数据 */
    void postSend( winux::Buffer const & data );

    /** \brief 投递发送数据 */
    void postSend( winux::Buffer && data );

    Server * server;            //!< 服务器
    winux::uint64 clientId;     //!< 客户Id
    winux::String clientEpStr;  //!< 客户终端字符串
    winux::SharedPointer<ip::tcp::Socket> clientSockPtr; //!< 客户套接字

    winux::SafeQueue<DataRecvSendCtx> pendingSend;    //!< 待发送队列

    winux::uint16 processingEvents;  //!< 等待处理或正在处理中的事件，保证同一个客户连接仅投递一个事件到线程池中
    bool canRemove;             //!< 是否标记为可以移除

private:
    DISABLE_OBJECT_COPY(ClientCtx)
};

/** \brief 基础服务器类
 *
 *  直接使用时，需要给定事件处理；继承时需要override相应的事件虚函数。\n
 *  事件：\n
 *  ClientDataNotify/ClientDataArrived - 客户数据通知/客户数据到达\n
 *  CreateClient - 创建客户场景时 */
class EIENNET_DLL Server
{
public:
    /** \brief 构造函数0，不会启动服务，必须手动调用`startup()` */
    Server();

    /** \brief 构造函数1，会启动服务
     *
     *  \param autoReadData 是否自动读取数据。当为true时，客户数据达到时调用ClientDataArrived事件，否则调用ClientDataNotify事件
     *  \param ep 服务监听的`EndPoint`
     *  \param threadCount 线程池线程数量
     *  \param backlog listen(backlog)
     *  \param serverWait 服务器IO等待时间
     *  \param verboseInterval verbose信息刷新间隔
     *  \param verbose 输出提示信息方式 */
    Server(
        bool autoReadData,
        ip::EndPoint const & ep,
        int threadCount = 4,
        int backlog = 0,
        double serverWait = 0.002,
        double verboseInterval = 0.01,
        int verbose = 1,
        winux::String const & logViewer = $T("127.0.0.1:22345")
    );

    virtual ~Server();

    /** \brief 启动服务器
     *
     *  \param autoReadData 是否自动读取数据。当为true时，客户数据达到时调用ClientDataArrived事件，否则调用ClientDataNotify事件
     *  \param ep 服务监听的`EndPoint`
     *  \param threadCount 线程池线程数量
     *  \param backlog listen(backlog)
     *  \param serverWait 服务器IO等待时间
     *  \param verboseInterval verbose信息刷新间隔
     *  \param verbose 输出提示信息方式 */
    bool startup(
        bool autoReadData,
        ip::EndPoint const & ep,
        int threadCount = 4,
        int backlog = 0,
        double serverWait = 0.002,
        double verboseInterval = 0.01,
        int verbose = 1,
        winux::String const & logViewer = $T("127.0.0.1:22345")
    );

    /** \brief 运行 */
    virtual int run( void * runParam );

    /** \brief 是否停止服务运行 */
    void stop( bool b = true );

    /** \brief 获取客户连接数 */
    size_t getClientsCount() const;

    /** \brief 移除客户连接 */
    void removeClient( winux::uint64 clientId );

protected:
    /** \brief 是否添加一个客户连接。返回true添加，返回false不添加 */
    virtual bool _canAddClient( ClientCtx * clientCtx );
    /** \brief 添加一个客户连接，会触发调用`onCreateClient()`创建客户场景对象，会调用`_canAddClient()`判断是否添加到服务器客户列表 */
    bool _addClient( ip::EndPoint const & clientEp, winux::SharedPointer<ip::tcp::Socket> clientSockPtr, winux::SharedPointer<ClientCtx> ** ppClientCtxPtr );
    /** \brief 往线程池投递任务 */
    template < typename _Fx, typename... _ArgType >
    void _postTask( winux::SharedPointer<ClientCtx> clientCtxPtr, _Fx fn, _ArgType&& ... arg )
    {
        auto routine = MakeSimple( NewRunable( fn, std::forward<_ArgType>(arg)... ) );
        // 标记为处理事件中
        clientCtxPtr->processingEvents++;
        this->_pool.task( [routine, clientCtxPtr] () {
            routine->invoke();
            // 事件处理完毕，可再次select()事件
            clientCtxPtr->processingEvents--;
        } ).post();
    }

    // 客户数据通知
    /** \brief 客户数据通知，当`Server#_isAutoReadData`为false时有效
     *
     *  \param clientCtxPtr 客户场景
     *  \param readableSize 可无阻塞读取的数据量 */
    DEFINE_CUSTOM_EVENT(
        ClientDataNotify,
        ( winux::SharedPointer<ClientCtx> clientCtxPtr, size_t readableSize ),
        ( clientCtxPtr, readableSize )
    )

    // 客户数据到达
    /** \brief 客户数据到达，当`Server#_isAutoReadData`为true时有效
     *
     *  \param clientCtxPtr 客户场景
     *  \param data 数据 */
    DEFINE_CUSTOM_EVENT(
        ClientDataArrived,
        ( winux::SharedPointer<ClientCtx> clientCtxPtr, winux::Buffer & data ),
        ( clientCtxPtr, data )
    )

    // 客户数据发送
    /** \brief 客户数据发送
     *
     *  \param clientCtxPtr 客户场景
     *  \param data 数据 */
    DEFINE_CUSTOM_EVENT_RETURN_EX(
        void,
        ClientDataSend,
        ( winux::SharedPointer<ClientCtx> clientCtxPtr )
    );

    // 当创建客户连接对象
    /** \brief 创建客户连接对象
     *
     *  \param clientId 客户唯一标识（64位数字）
     *  \param clientEpStr `ip#EndPoint`字符串
     *  \param clientSockPtr 客户套接字 */
    DEFINE_CUSTOM_EVENT_RETURN_EX(
        ClientCtx *,
        CreateClient,
        ( winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<ip::tcp::Socket> clientSockPtr )
    );

protected:
    winux::ThreadPool _pool;            //!< 线程池
    winux::RecursiveMutex _mtxServer;   //!< 互斥量保护服务器共享数据
    ip::tcp::Socket _servSockA;         //!< 服务器监听套接字A
    ip::tcp::Socket _servSockB;         //!< 服务器监听套接字B
    std::map< winux::uint64, winux::SharedPointer<ClientCtx> > _clients; //!< 客户映射表

    winux::uint64 _cumulativeClientId;  //!< 客户唯一标识
    bool _stop;                         //!< 是否停止
    bool _servSockAIsListening;         //!< servSockA是否处于监听中
    bool _servSockBIsListening;         //!< servSockB是否处于监听中
    bool _isAutoReadData;               //!< 是否自动读取客户到达的数据。当为true时，客户数据达到时调用ClientDataArrived事件，否则调用ClientDataNotify事件

    double _serverWait;                 //!< 服务器IO等待时间间隔（秒）
    double _verboseInterval;            //!< Verbose信息刷新间隔（秒）
    int _verbose;                       //!< 提示信息输出方式
    winux::String _logViewer;           //!< 日志查看器主机

    friend class ClientCtx;

    DISABLE_OBJECT_COPY(Server)
};


} // namespace eiennet

#endif // __EIENNET_SERVER_HPP__
