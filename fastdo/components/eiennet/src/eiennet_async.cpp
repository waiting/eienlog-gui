#include "eiennet_base.hpp"
#include "eiennet_socket.hpp"
#include "eiennet_io.hpp"
#include "eiennet_async.hpp"
#include "eiennet_io_select.hpp"

#if defined(OS_WIN)
#else
#include <unistd.h>
#include <sys/timerfd.h>
#endif

namespace eiennet
{
namespace async
{
// class Socket -------------------------------------------------------------------------------
Socket::Socket( io::IoService & serv, int sock, bool isNewSock ) : eiennet::Socket( sock, isNewSock ), _serv(&serv), _data(nullptr), _thread(nullptr)
{
    this->setBlocking(false);
}

Socket::Socket( io::IoService & serv, AddrFamily af, SockType sockType, Protocol proto ) : eiennet::Socket( af, sockType, proto ), _serv(&serv), _data(nullptr), _thread(nullptr)
{
    this->setBlocking(false);
}

Socket::~Socket()
{
    // 减小线程负载
    if ( this->_thread ) this->_thread->decWeight();
}

SocketSharedPtr Socket::onCreateClient( io::IoService & serv, int sock, bool isNewSock )
{
    if ( this->_CreateClientHandler )
    {
        return this->_CreateClientHandler( serv, sock, isNewSock );
    }
    else
    {
        return winux::MakeShared( new Socket( serv, sock, isNewSock ) );
    }
}

// struct Timer_Data --------------------------------------------------------------------------
struct Timer_Data
{
#if defined(OS_WIN)
    static void CALLBACK _TimerCallback( PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER pTpTimer );
    PTP_TIMER _pTpTimer;
#else
    int _timerFd;
#endif

    Timer_Data() :
    #if defined(OS_WIN)
        _pTpTimer(nullptr)
    #else
        _timerFd(-1)
    #endif
    {
    }

};

// class Timer --------------------------------------------------------------------------------
Timer::Timer( io::IoService & serv ) : _timerCtx(nullptr), _posted(false), _serv(&serv), _thread(nullptr)
{
    this->create();

}

Timer::~Timer()
{
    // 减小线程负载
    if ( this->_thread ) this->_thread->decWeight();

    this->destroy();
}

void Timer::create()
{
    this->destroy();
    {
        winux::ScopeGuard guard(this->_mtx);
    #if defined(OS_WIN)
        _self->_pTpTimer = CreateThreadpoolTimer( &Timer_Data::_TimerCallback, this, nullptr );
    #else
        _self->_timerFd = timerfd_create( CLOCK_MONOTONIC, TFD_NONBLOCK );
    #endif
    }
}

void Timer::destroy()
{
    winux::ScopeGuard guard(this->_mtx);
#if defined(OS_WIN)
    if ( _self->_pTpTimer )
    {
        CloseThreadpoolTimer(_self->_pTpTimer);
        _self->_pTpTimer = nullptr;
    }
#else
    if ( _self->_timerFd != -1 )
    {
        close(_self->_timerFd);
        _self->_timerFd = -1;
    }
#endif
}

void Timer::set( winux::uint64 timeoutMs, bool periodic )
{
    winux::ScopeGuard guard(this->_mtx);
#if defined(OS_WIN)
    if ( _self->_pTpTimer )
    {
        union
        {
            LARGE_INTEGER li;
            FILETIME ft;
        } dueTime;
        dueTime.li.QuadPart = timeoutMs ? -( (winux::int64)timeoutMs * 10000 ) : -1;
        SetThreadpoolTimer( _self->_pTpTimer, &dueTime.ft, (DWORD)( periodic ? ( timeoutMs ? timeoutMs : 1 ) : 0 ), 0 );
    }
#else
    if ( _self->_timerFd != -1 )
    {
        itimerspec tm = { { 0, 0 }, { 0, 0 } };
        tm.it_value.tv_sec = timeoutMs / 1000;
        tm.it_value.tv_nsec = ( timeoutMs % 1000 ) * 1000000;
        if ( periodic )
        {
            tm.it_interval.tv_sec = timeoutMs / 1000;
            tm.it_interval.tv_nsec = ( timeoutMs % 1000 ) * 1000000;
        }
        timerfd_settime( _self->_timerFd, 0, &tm, nullptr );
    }
#endif
}

void Timer::unset()
{
    winux::ScopeGuard guard(this->_mtx);
#if defined(OS_WIN)
    if ( _self->_pTpTimer )
    {
        SetThreadpoolTimer( _self->_pTpTimer, nullptr, 0, 0 );
    }
#else
    if ( _self->_timerFd != -1 )
    {
        itimerspec tm = { { 0, 0 }, { 0, 0 } };
        timerfd_settime( _self->_timerFd, 0, &tm, nullptr );
    }
#endif
}

io::IoTimerCtx * Timer::stop()
{
    winux::ScopeGuard guard(this->_mtx);
    if ( this->_timerCtx )
    {
        {
            winux::ScopeUnguard unguard(this->_mtx);
            this->_timerCtx->cancel(io::cancelProactive); // Timer::unset() inside
            this->_timerCtx->changeState(io::stateCancel);
        }
        this->_timerCtx->periodic = false; // 设为非周期
        if ( this->_posted == false ) // 还未投递
        {
            auto * timerCtx = this->_timerCtx;
            this->_timerCtx = nullptr;
            return timerCtx;
        }
    }
    return nullptr;
}

intptr_t Timer::get() const
{
#if defined(OS_WIN)
    return (intptr_t)_self->_pTpTimer;
#else
    return (intptr_t)_self->_timerFd;
#endif
}

#if defined(OS_WIN)
void CALLBACK Timer_Data::_TimerCallback( PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER pTpTimer )
{
    auto timer = ((Timer *)Context)->sharedFromThis();
    {
        winux::ScopeGuard guard(timer->_mtx);
        if ( timer->_timerCtx )
        {
            timer->_posted = true;
            if ( timer->_thread )
            {
                timer->_thread->timerTrigger(timer->_timerCtx);
            }
            else
            {
                timer->_serv->timerTrigger(timer->_timerCtx);
            }
        }
    }
}
#else

#endif

} // namespace async


} // namespace eiennet

