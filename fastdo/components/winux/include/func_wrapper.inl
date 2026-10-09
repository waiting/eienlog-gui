#pragma once

// 函数包装
template < size_t _ArgCount, typename _Fx1, typename _StdFunc >
struct FuncWrapper;

template < typename _Fx1, typename _StdFunc >
struct FuncWrapper<0, _Fx1, _StdFunc>
{
    static _StdFunc Wrap( _Fx1 && fn )
    {
        return std::bind( std::forward<_Fx1>(fn) );
    }
};
template < typename _Fx1, typename _StdFunc >
struct FuncWrapper<1, _Fx1, _StdFunc>
{
    static _StdFunc Wrap( _Fx1 && fn )
    {
        return std::bind( std::forward<_Fx1>(fn), std::placeholders::_1 );
    }
};
template < typename _Fx1, typename _StdFunc >
struct FuncWrapper<2, _Fx1, _StdFunc>
{
    static _StdFunc Wrap( _Fx1 && fn )
    {
        return std::bind(
            std::forward<_Fx1>(fn),
            std::placeholders::_1,
            std::placeholders::_2
        );
    }
};
template < typename _Fx1, typename _StdFunc >
struct FuncWrapper<3, _Fx1, _StdFunc>
{
    static _StdFunc Wrap( _Fx1 && fn )
    {
        return std::bind(
            std::forward<_Fx1>(fn),
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3
        );
    }
};
template < typename _Fx1, typename _StdFunc >
struct FuncWrapper<4, _Fx1, _StdFunc>
{
    static _StdFunc Wrap( _Fx1 && fn )
    {
        return std::bind(
            std::forward<_Fx1>(fn),
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4
        );
    }
};
template < typename _Fx1, typename _StdFunc >
struct FuncWrapper<5, _Fx1, _StdFunc>
{
    static _StdFunc Wrap( _Fx1 && fn )
    {
        return std::bind(
            std::forward<_Fx1>(fn),
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4,
            std::placeholders::_5
        );
    }
};
template < typename _Fx1, typename _StdFunc >
struct FuncWrapper<6, _Fx1, _StdFunc>
{
    static _StdFunc Wrap( _Fx1 && fn )
    {
        return std::bind(
            std::forward<_Fx1>(fn),
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4,
            std::placeholders::_5,
            std::placeholders::_6
        );
    }
};
template < typename _Fx1, typename _StdFunc >
struct FuncWrapper<7, _Fx1, _StdFunc>
{
    static _StdFunc Wrap( _Fx1 && fn )
    {
        return std::bind(
            std::forward<_Fx1>(fn),
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4,
            std::placeholders::_5,
            std::placeholders::_6,
            std::placeholders::_7
        );
    }
};
template < typename _Fx1, typename _StdFunc >
struct FuncWrapper<8, _Fx1, _StdFunc>
{
    static _StdFunc Wrap( _Fx1 && fn )
    {
        return std::bind(
            std::forward<_Fx1>(fn),
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4,
            std::placeholders::_5,
            std::placeholders::_6,
            std::placeholders::_7,
            std::placeholders::_8
        );
    }
};
template < typename _Fx1, typename _StdFunc >
struct FuncWrapper<9, _Fx1, _StdFunc>
{
    static _StdFunc Wrap( _Fx1 && fn )
    {
        return std::bind(
            std::forward<_Fx1>(fn),
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4,
            std::placeholders::_5,
            std::placeholders::_6,
            std::placeholders::_7,
            std::placeholders::_8,
            std::placeholders::_9
        );
    }
};
template < typename _Fx1, typename _StdFunc >
struct FuncWrapper<10, _Fx1, _StdFunc>
{
    static _StdFunc Wrap( _Fx1 && fn )
    {
        return std::bind(
            std::forward<_Fx1>(fn),
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4,
            std::placeholders::_5,
            std::placeholders::_6,
            std::placeholders::_7,
            std::placeholders::_8,
            std::placeholders::_9,
            std::placeholders::_10
        );
    }
};
template < typename _Fx1, typename _StdFunc >
struct FuncWrapper<11, _Fx1, _StdFunc>
{
    static _StdFunc Wrap( _Fx1 && fn )
    {
        return std::bind(
            std::forward<_Fx1>(fn),
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4,
            std::placeholders::_5,
            std::placeholders::_6,
            std::placeholders::_7,
            std::placeholders::_8,
            std::placeholders::_9,
            std::placeholders::_10,
            std::placeholders::_11
        );
    }
};
template < typename _Fx1, typename _StdFunc >
struct FuncWrapper<12, _Fx1, _StdFunc>
{
    static _StdFunc Wrap( _Fx1 && fn )
    {
        return std::bind(
            std::forward<_Fx1>(fn),
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4,
            std::placeholders::_5,
            std::placeholders::_6,
            std::placeholders::_7,
            std::placeholders::_8,
            std::placeholders::_9,
            std::placeholders::_10,
            std::placeholders::_11,
            std::placeholders::_12
        );
    }
};

template < typename _StdFunc >
struct FuncWrapper<0, std::nullptr_t, _StdFunc>
{
    static _StdFunc Wrap( std::nullptr_t )
    {
        return _StdFunc(nullptr);
    }
};

template < typename _Fx, typename _Fx1 >
typename FuncTraits<_Fx>::StdFunction FuncWrap( _Fx1 && fn )
{
    return FuncWrapper< FuncTraits<_Fx1>::Arity, _Fx1, typename FuncTraits<_Fx>::StdFunction >::Wrap( std::forward<_Fx1>(fn) );
}
