#ifndef __EIENNET_BASE_HPP__
#define __EIENNET_BASE_HPP__

#include "winux.hpp"

#ifdef EIENNET_DLL_USE
    #if defined(_MSC_VER) || defined(WIN32)
        #pragma warning( disable: 4251 )
        #ifdef  EIENNET_DLL_EXPORTS
            #define EIENNET_DLL  __declspec(dllexport)
        #else
            #define EIENNET_DLL  __declspec(dllimport)
        #endif

        #define EIENNET_API __stdcall
    #else
        #define EIENNET_DLL
        #define EIENNET_API
    #endif
#else
    #define EIENNET_DLL
    #define EIENNET_API
#endif

#define EIENNET_FUNC_DECL(ret) EIENNET_DLL ret EIENNET_API
#define EIENNET_FUNC_IMPL(ret) ret EIENNET_API

/** \brief 网络通信库 */
namespace eiennet
{

// 主机序转为网络序
template < typename _Ty >
inline _Ty htont( _Ty v )
{
    constexpr winux::uint16 judgeHostOrder = 0x0102;
    if ( *(winux::byte*)&judgeHostOrder == 0x02 ) // 小端主机
    {
        for ( size_t i = 0; i < sizeof(_Ty) / 2; ++i )
        {
            winux::byte t = reinterpret_cast<winux::byte*>(&v)[i];
            reinterpret_cast<winux::byte*>(&v)[i] = reinterpret_cast<winux::byte*>(&v)[sizeof(_Ty) - 1 - i];
            reinterpret_cast<winux::byte*>(&v)[sizeof(_Ty) - 1 - i] = t;
        }
    }
    return v;
}

// 网络序转为主机序
template < typename _Ty >
inline _Ty ntoht( _Ty v )
{
    constexpr winux::uint16 judgeHostOrder = 0x0102;
    if ( *(winux::uint8*)&judgeHostOrder == 0x02 ) // 小端主机
    {
        for ( size_t i = 0; i < sizeof(_Ty) / 2; ++i )
        {
            winux::byte t = reinterpret_cast<winux::byte*>(&v)[i];
            reinterpret_cast<winux::byte*>(&v)[i] = reinterpret_cast<winux::byte*>(&v)[sizeof(_Ty) - 1 - i];
            reinterpret_cast<winux::byte*>(&v)[sizeof(_Ty) - 1 - i] = t;
        }
    }
    return v;
}

}

#endif // __EIENNET_BASE_HPP__
