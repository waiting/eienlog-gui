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
#include "eiennet_server.hpp"
#include "eienlog.hpp"

namespace eiennet
{
// class ClientCtx ----------------------------------------------------------------------------
ClientCtx::ClientCtx( Server * server, winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr ) :
    server(server),
    clientId(clientId),
    clientEpStr(clientEpStr),
    clientSockPtr(clientSockPtr),
    processingEvents(0),
    canRemove(false)
{

}

ClientCtx::~ClientCtx()
{
    if ( this->server && this->server->_verbose ) eienlog::VerboseOutput( this->server->_verbose, eienlog::vcaFgBlue | eienlog::vcaBgIgnore, this->getStamp(), " destruct" );
}

winux::String ClientCtx::getStamp() const
{
    winux::String stamp;
    winux::StringWriter(&stamp) << "{tid:" << winux::GetTid() << "}" << "[client-" << this->clientId << "]<" << this->clientEpStr << ">";
    return stamp;
}

void ClientCtx::postSend( winux::Buffer const & data )
{
    this->pendingSend.push( DataRecvSendCtx( data, 0, data.size() ) );
}

void ClientCtx::postSend( winux::Buffer && data )
{
    size_t size = data.size();
    this->pendingSend.push( DataRecvSendCtx( std::move(data), 0, size ) );
}

// class Server -------------------------------------------------------------------------------
Server::Server() :
    _mtxServer(true),
    _cumulativeClientId(0),
    _stop(false),
    _servSockAIsListening(false),
    _servSockBIsListening(false),
    _isAutoReadData(true),
    _serverWait(0.002),
    _verboseInterval(0.01),
    _verbose(eienlog::votConsole)
{

}

Server::Server( bool autoReadData, ip::EndPoint const & ep, int threadCount, int backlog, double serverWait, double verboseInterval, int verbose, winux::String const & logViewer ) :
    _mtxServer(true),
    _cumulativeClientId(0),
    _stop(false),
    _servSockAIsListening(false),
    _servSockBIsListening(false),
    _isAutoReadData(autoReadData),
    _serverWait(serverWait),
    _verboseInterval(verboseInterval),
    _verbose(verbose),
    _logViewer(logViewer)
{
    this->startup( autoReadData, ep, threadCount, backlog, serverWait, verboseInterval, verbose, logViewer );
}

Server::~Server()
{

}

// Linux平台下
// 当socket先以IPv6监听时也会同时监听IPv4线路，此时再创建IPv4监听将失败。
// 当socket先以IPv4监听时，同样创建IPv6监听此端口也会失败。（这是因为Linux下IPV6套接字的IPV6_V6ONLY默认是false，而Windows下是true。）
// 需要如下设置才可以监听与IPv4相同的端口

// 启动IPv4、IPv6两个Sockets
void __StartupSockets( ip::EndPoint const & ep, int backlog, eienlog::VerboseOutputType verbose, ip::EndPoint * pEp2, ip::tcp::Socket * sockA, ip::tcp::Socket * sockB )
{
    // 创建另一个EP
    ip::EndPoint & ep2 = *pEp2;
    switch ( ep.get<sockaddr>()->sa_family )
    {
    case AF_INET:
        {
            auto p = ep2.get<sockaddr_in6>();
            p->sin6_family = AF_INET6;
            p->sin6_port = ep.get<sockaddr_in>()->sin_port;
            ep2.size() = sizeof(sockaddr_in6);

            sockA->create( Socket::afInet, Socket::sockStream, Socket::protoUnspec );
            sockB->create( Socket::afInet6, Socket::sockStream, Socket::protoUnspec );

            sockB->setIpv6Only(true);
        }
        break;
    case AF_INET6:
        {
            auto p = ep2.get<sockaddr_in>();
            p->sin_family = AF_INET;
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
    if ( !( sockA->bind(ep) && sockA->listen(backlog) ) && verbose )
    {
        int err = Socket::ErrNo();
        eienlog::VerboseOutput(
            verbose,
            eienlog::vcaFgRed | eienlog::vcaBgIgnore,
            "Socket#1 startup failed",
            ", ep=", ep.toString(),
            ", err=", err
        );
    }
    if ( !( sockB->bind(ep2) && sockB->listen(backlog) ) && verbose )
    {
        int err2 = Socket::ErrNo();
        eienlog::VerboseOutput(
            verbose,
            eienlog::vcaFgRed | eienlog::vcaBgIgnore,
            "Socket#2 startup failed",
            ", ep2=", ep2.toString(),
            ", err2=", err2
        );
    }
}

bool Server::startup( bool autoReadData, ip::EndPoint const & ep, int threadCount, int backlog, double serverWait, double verboseInterval, int verbose, winux::String const & logViewer )
{
    _pool.startup(threadCount);

    _cumulativeClientId = 0;

    // 启动套接字监听
    ip::EndPoint ep2;
    __StartupSockets( ep, backlog, (eienlog::VerboseOutputType)verbose, &ep2, &_servSockA, &_servSockB );

    // 两个都不处于监听中
    _servSockAIsListening = _servSockA.isListening();
    _servSockBIsListening = _servSockB.isListening();
    _stop = !_servSockAIsListening && !_servSockBIsListening;

    _isAutoReadData = autoReadData;

    _serverWait = serverWait;
    _verboseInterval = verboseInterval;
    _verbose = verbose;
    _logViewer = logViewer;

    if ( !_logViewer.empty() )
    {
        ip::EndPoint ep(_logViewer);
        eienlog::EnableLog( ep.getIp(), ep.getPort() );
    }

    if ( this->_verbose ) eienlog::VerboseOutput(
        this->_verbose,
        _stop ? ( eienlog::vcaFgRed | eienlog::vcaBgIgnore ) : ( eienlog::vcaFgGreen | eienlog::vcaBgIgnore ),
        _stop ? "Server startup failed" : "Server startup success",
        ", ep=", ep.toString(),
        ", ep2=", ep2.toString(),
        ", threads=", threadCount,
        ", backlog=", backlog,
        ", serverWait=", serverWait,
        ", verboseInterval=", verboseInterval,
        ", verbose=", verbose,
        ", logViewer=", logViewer
    );

    return !_stop;
}

int Server::run( void * runParam )
{
    io::Select sel;
    int counter = 0;
    while ( !_stop )
    {
        sel.clear();

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
            if ( ++counter % counter2 == 0 )
            {
                winux::ColorOutput(
                    winux::fgWhite,
                    winux::DateTimeL::Current(),
                    ", Total clients:", this->_clients.size(),
                    ", Current tasks:", this->_pool.getTaskCount(),
                    winux::String( 20, ' ' ),
                    "\r"
                );
            }

            // 监视客户连接
            for ( auto it = this->_clients.begin(); it != this->_clients.end(); )
            {
                if ( it->second->canRemove ) // 删除标记为可删除的客户
                {
                    if ( this->_verbose ) eienlog::VerboseOutput( this->_verbose, eienlog::vcaFgMaroon | eienlog::vcaBgIgnore, it->second->getStamp(), " remove" );

                    it = this->_clients.erase(it);
                }
                else if ( it->second->processingEvents > 0 ) // 跳过有事件在处理中的客户
                {
                    it++;
                }
                else
                {
                    // 监视数据接收
                    sel.setReadSock(*it->second->clientSockPtr.get());
                    // 若未决发送队列不空则监视数据写入
                    if ( it->second->pendingSend.size() > 0 )
                        sel.setWriteSock(*it->second->clientSockPtr.get());
                    // 监视套接字出错
                    sel.setExceptSock(*it->second->clientSockPtr.get());

                    it++;
                }
            }
        }

        int rc = sel.wait(this->_serverWait); // 返回就绪的套接字数
        if ( rc > 0 )
        {
            if ( this->_verbose ) eienlog::VerboseOutput( this->_verbose, eienlog::vcaFgGreen | eienlog::vcaBgIgnore, "io.Select() model get the number of ready sockets:", rc );

            // 处理servSockA事件
            if ( _servSockAIsListening )
            {
                if ( sel.hasExceptSock(_servSockA) )
                {
                    winux::ScopeGuard guard(this->_mtxServer);
                    _stop = true;

                    rc--;
                }
                else if ( sel.hasReadSock(_servSockA) )
                {
                    // 有一个客户连接到来
                    eiennet::ip::EndPoint clientEp;
                    auto clientSockPtr = _servSockA.accept(&clientEp);

                    if ( clientSockPtr )
                    {
                        winux::SharedPointer<ClientCtx> * pClientCtxPtr = nullptr;
                        if ( this->_addClient( clientEp, clientSockPtr, &pClientCtxPtr ) )
                        {
                            if ( this->_verbose ) eienlog::VerboseOutput( this->_verbose, eienlog::vcaFgFuchsia | eienlog::vcaBgIgnore, (*pClientCtxPtr)->getStamp(), " new client join the server" );
                        }
                    }

                    rc--;
                }
            }

            // 处理servSockB事件
            if ( _servSockBIsListening )
            {
                if ( sel.hasExceptSock(_servSockB) )
                {
                    winux::ScopeGuard guard(this->_mtxServer);
                    _stop = true;

                    rc--;
                }
                else if ( sel.hasReadSock(_servSockB) )
                {
                    // 有一个客户连接到来
                    eiennet::ip::EndPoint clientEp;
                    auto clientSockPtr = _servSockB.accept(&clientEp);

                    if ( clientSockPtr )
                    {
                        winux::SharedPointer<ClientCtx> * pClientCtxPtr = nullptr;
                        if ( this->_addClient( clientEp, clientSockPtr, &pClientCtxPtr ) )
                        {
                            if ( this->_verbose ) eienlog::VerboseOutput( this->_verbose, eienlog::vcaFgFuchsia | eienlog::vcaBgIgnore, (*pClientCtxPtr)->getStamp(), " new client join the server" );
                        }
                    }

                    rc--;
                }
            }

            // 分发客户连接的相关IO事件
            if ( rc > 0 )
            {
                winux::ScopeGuard guard(this->_mtxServer);

                for ( auto it = this->_clients.begin(); it != this->_clients.end(); )
                {
                    if ( sel.hasExceptSock(*it->second->clientSockPtr.get()) ) // 该套接字有错误
                    {
                        if ( this->_verbose ) eienlog::VerboseOutput( this->_verbose, eienlog::vcaFgMaroon | eienlog::vcaBgIgnore, it->second->getStamp(), " error, mark it as removable" );

                        it->second->canRemove = true;

                        rc--;
                    }
                    else if ( sel.hasWriteSock(*it->second->clientSockPtr.get()) ) // 该套接字可写数据
                    {
                        if ( this->_verbose ) eienlog::VerboseOutput( this->_verbose, eienlog::vcaFgIgnore | eienlog::vcaBgIgnore, it->second->getStamp(), " sendable" );

                        // 投递数据发送事件到线程池处理
                        this->_postTask( it->second, &Server::onClientDataSend, this, it->second );

                        rc--;
                    }
                    else if ( sel.hasReadSock(*it->second->clientSockPtr.get()) ) // 该套接字有数据可读
                    {
                        size_t readableSize = it->second->clientSockPtr->getAvailable();

                        if ( readableSize > 0 )
                        {
                            if ( _isAutoReadData )
                            {
                                // 收数据
                                winux::Buffer data = it->second->clientSockPtr->recv(readableSize);
                                if ( this->_verbose ) eienlog::VerboseOutput( this->_verbose, eienlog::vcaFgWhite | eienlog::vcaBgIgnore, it->second->getStamp(), " data arrived(bytes:", data.getSize(), ")" );

                                // 投递数据到达事件到线程池处理
                                this->_postTask( it->second, &Server::onClientDataArrived, this, it->second, std::move(data) );
                            }
                            else
                            {
                                if ( this->_verbose ) eienlog::VerboseOutput( this->_verbose, eienlog::vcaFgWhite | eienlog::vcaBgIgnore, it->second->getStamp(), " data notify(bytes:", readableSize, ")" );

                                // 投递数据到达事件到线程池处理
                                this->_postTask( it->second, &Server::onClientDataNotify, this, it->second, readableSize );
                            }
                        }
                        else // readableSize <= 0
                        {
                            if ( this->_verbose ) eienlog::VerboseOutput( this->_verbose, eienlog::vcaFgRed | eienlog::vcaBgIgnore, it->second->getStamp(), " data arrived(bytes:", readableSize, "), the connection may be closed" );

                            it->second->canRemove = true;
                        }

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

void Server::onClientDataSend( winux::SharedPointer<ClientCtx> clientCtxPtr )
{
    auto & ctx = clientCtxPtr->pendingSend.front();
    if ( ctx.hadBytes < ctx.targetBytes )
    {
        // 发送数据
        int rc = clientCtxPtr->clientSockPtr->send( ctx.data.getAt(ctx.hadBytes), ctx.targetBytes - ctx.hadBytes );
        if ( rc >= 0 )
        {
            if ( this->_verbose ) eienlog::VerboseOutput( this->_verbose, eienlog::vcaFgIgnore | eienlog::vcaBgIgnore, clientCtxPtr->getStamp(), winux::FormatA( " data send(%u bytes / %u bytes)", ctx.hadBytes, ctx.targetBytes ) );
            ctx.hadBytes += rc;
        }
        else
        {
            if ( this->_verbose ) eienlog::VerboseOutput( this->_verbose, eienlog::vcaFgRed | eienlog::vcaBgIgnore, clientCtxPtr->getStamp(), winux::FormatA( " error, data send(%u bytes / %u bytes)", ctx.hadBytes, ctx.targetBytes ) );
            // 出错，删除
            clientCtxPtr->canRemove = true;
        }
    }

    if ( ctx.hadBytes == ctx.targetBytes )
    {
        if ( this->_verbose ) eienlog::VerboseOutput( this->_verbose, eienlog::vcaFgAtrovirens | eienlog::vcaBgIgnore, clientCtxPtr->getStamp(), " send queue pop" );
        clientCtxPtr->pendingSend.pop();
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
