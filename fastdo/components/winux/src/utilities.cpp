#include "system_detection.inl"

#if defined(CL_MINGW) // for mingw
    #ifdef __STRICT_ANSI__
    #undef __STRICT_ANSI__
    #endif
#endif

#include "utilities.hpp"
#include "strings.hpp"
#include "json.hpp"
#include "time.hpp"

#include <iomanip>
#include <string.h>

#if defined(OS_WIN)
    #include <mbstring.h>
    #include <tchar.h>

#else // OS_LINUX
    #include <unistd.h>
    #include <errno.h>

    // linux别名
    #define _stricmp strcasecmp
    #define _wcsicmp wcscasecmp
    #define _close close
    #define _open open
    #define _read read
    #define _write write

    #ifdef UNICODE
        #define _vsntprintf vswprintf
        #define _tcsstr wcsstr
        #define _tcsicmp wcscasecmp
        #define _tcscmp wcscmp
    #else
        #define _vsntprintf vsnprintf
        #define _tcsstr strstr
        #define _tcsicmp strcasecmp
        #define _tcscmp strcmp
    #endif

#endif


namespace winux
{
WINUX_FUNC_IMPL(void) InvertByteOrder( void * buf, size_t size )
{
    byte * p1 = (byte *)buf;
    byte * p2 = (byte *)buf + size - 1;
    while ( p1 < p2 )
    {
        byte bt = *p1;
        *p1 = *p2;
        *p2 = bt;

        p1++;
        p2--;
    }
}

WINUX_FUNC_IMPL(int) MemoryCompare( void const * buf1, size_t n1, void const * buf2, size_t n2 )
{
    size_t n = n1 < n2 ? n1 : n2;
    int diff = memcmp( buf1, buf2, n );
    if ( diff != 0 ) return diff;
    return (int)( n1 - n2 );
}

WINUX_FUNC_IMPL(int) MemoryCompareI( void const * buf1, size_t n1, void const * buf2, size_t n2 )
{
#if defined(OS_WIN)
    size_t n = n1 < n2 ? n1 : n2;
    int diff = memicmp( buf1, buf2, n );
    if ( diff != 0 ) return diff;
    return (int)( n1 - n2 );
#else
    auto p1 = (byte const *)buf1;
    auto p2 = (byte const *)buf2;
    size_t n = n1 < n2 ? n1 : n2;
    for ( size_t i = 0; i < n; ++i )
    {
        int diff = tolower(p1[i]) - tolower(p2[i]);
        if ( diff != 0 ) return diff;
    }
    return (int)( n1 - n2 );
#endif
}

WINUX_FUNC_IMPL(bool) CollectionLess( Collection const & coll1, Collection const & coll2 )
{
    size_t count1 = coll1.getCount(), count2 = coll2.getCount();
    auto it1 = coll1.refPairList().begin();
    auto it2 = coll2.refPairList().begin();
    auto & coll = count1 < count2 ? coll1 : coll2;
    if ( !coll.traverse( [&it1, &it2, &coll1, &coll2] ( MixedMixedPair const & ) {
        if ( it1->second != it2->second ) return false;
        it1++;
        it2++;
        return true;
    } ) )
    {
        return it1->second < it2->second;
    }
    else
    {
        return count1 < count2;
    }
}

WINUX_FUNC_IMPL(bool) CollectionGreater( Collection const & coll1, Collection const & coll2 )
{
    size_t count1 = coll1.getCount(), count2 = coll2.getCount();
    auto it1 = coll1.refPairList().begin();
    auto it2 = coll2.refPairList().begin();
    auto & coll = count1 < count2 ? coll1 : coll2;
    if ( !coll.traverse( [&it1, &it2, &coll1, &coll2] ( MixedMixedPair const & ) {
        if ( it1->second != it2->second ) return false;
        it1++;
        it2++;
        return true;
    } ) )
    {
        return it1->second > it2->second;
    }
    else
    {
        return count1 > count2;
    }
}

WINUX_FUNC_IMPL(bool) CollectionEqual( Collection const & coll1, Collection const & coll2 )
{
    size_t count1 = coll1.getCount(), count2 = coll2.getCount();
    auto it1 = coll1.refPairList().begin();
    auto it2 = coll2.refPairList().begin();
    auto & coll = count1 < count2 ? coll1 : coll2;
    if ( !coll.traverse( [&it1, &it2, &coll1, &coll2] ( MixedMixedPair const & ) {
        if ( it1->second != it2->second ) return false;
        it1++;
        it2++;
        return true;
    } ) )
    {
        return false;
    }
    else
    {
        return count1 == count2;
    }
}

WINUX_FUNC_IMPL(bool) ValueIsInArray( StringArray const & arr, String const & val, bool caseInsensitive )
{
    int (*pStrCmp)( tchar const *, tchar const * ) = caseInsensitive ? &_tcsicmp: &_tcscmp;
    for ( auto it = arr.begin(); it != arr.end(); ++it )
    {
        if ( (*pStrCmp)( (*it).c_str(), val.c_str() ) == 0 )
        {
            return true;
        }
    }
    return false;
}

WINUX_FUNC_IMPL(int) Random( int n1, int n2 )
{
    static int seedInit = VoidReturnInt( srand, (unsigned int)GetUtcTimeMs() );
    (void)seedInit;
    return abs( rand() * rand() ) % ( abs( n2 - n1 ) + 1 ) + ( n1 < n2 ? 1 : -1 ) * n1;
}

// 类型解析功能 ---------------------------------------------------------------------------------
template < typename _ChTy >
inline static bool Impl_ParseBoolean( XString<_ChTy> const & str, bool * boolVal )
{
    if (
        StringCompareI<_ChTy>( str, Literal<_ChTy>::boolTrueStr ) == 0 ||
        StringCompareI<_ChTy>( str, Literal<_ChTy>::tStr ) == 0 ||
        StringCompareI<_ChTy>( str, Literal<_ChTy>::boolYesStr ) == 0 ||
        StringCompareI<_ChTy>( str, Literal<_ChTy>::boolOnStr ) == 0
    )
    {
        *boolVal = true;
    }
    else if (
        StringCompareI<_ChTy>( str, Literal<_ChTy>::boolFalseStr ) == 0 ||
        StringCompareI<_ChTy>( str, Literal<_ChTy>::fStr ) == 0 ||
        StringCompareI<_ChTy>( str, Literal<_ChTy>::boolNoStr ) == 0 ||
        StringCompareI<_ChTy>( str, Literal<_ChTy>::boolOffStr ) == 0
    )
    {
        *boolVal = false;
    }
    else
    {
        ulong ul;
        ParseULong( str, &ul );
        *boolVal = ul != 0;
    }
    return true;
}

WINUX_FUNC_IMPL(bool) ParseBool( AnsiString const & str, bool * boolVal )
{
    return Impl_ParseBoolean( str, boolVal );
}

WINUX_FUNC_IMPL(bool) ParseBool( UnicodeString const & str, bool * boolVal )
{
    return Impl_ParseBoolean( str, boolVal );
}

template < typename _ChTy, typename _ToFx, typename _Ty >
inline static bool Impl_ParseInteger( XString<_ChTy> const & str, _ToFx && tofn, _Ty * val )
{
    if ( str.length() > 1 && str[0] == Literal<_ChTy>::zeroChar && ( str[1] | 0x20 ) == Literal<_ChTy>::xChar ) // 16进制数
    {
        *val = tofn( str.c_str() + 2, 16 );
    }
    else if ( str.length() > 1 && str[0] == Literal<_ChTy>::zeroChar ) // 8进制数
    {
        *val = tofn( str.c_str() + 1, 8 );
    }
    else
    {
        *val = tofn( str.c_str(), 10 );
    }
    return true;
}

WINUX_FUNC_IMPL(bool) ParseInt( AnsiString const & str, int * iVal )
{
    return Impl_ParseInteger(
        str,
        [] ( AnsiString::value_type const * str, int radix ) { return (int)strtol( str, nullptr, radix ); },
        iVal
    );
}

WINUX_FUNC_IMPL(bool) ParseInt( UnicodeString const & str, int * iVal )
{
    return Impl_ParseInteger(
        str,
        [] ( UnicodeString::value_type const * str, int radix ) { return (int)wcstol( str, nullptr, radix ); },
        iVal
    );
}

WINUX_FUNC_IMPL(bool) ParseUInt( AnsiString const & str, uint * uiVal )
{
    return Impl_ParseInteger(
        str,
        [] ( AnsiString::value_type const * str, int radix ) { return (uint)strtoul( str, nullptr, radix ); },
        uiVal
    );
}

WINUX_FUNC_IMPL(bool) ParseUInt( UnicodeString const & str, uint * uiVal )
{
    return Impl_ParseInteger(
        str,
        [] ( UnicodeString::value_type const * str, int radix ) { return (uint)wcstoul( str, nullptr, radix ); },
        uiVal
    );
}

WINUX_FUNC_IMPL(bool) ParseLong( AnsiString const & str, long * lVal )
{
    return Impl_ParseInteger(
        str,
        [] ( AnsiString::value_type const * str, int radix ) { return strtol( str, nullptr, radix ); },
        lVal
    );
}

WINUX_FUNC_IMPL(bool) ParseLong( UnicodeString const & str, long * lVal )
{
    return Impl_ParseInteger(
        str,
        [] ( UnicodeString::value_type const * str, int radix ) { return wcstol( str, nullptr, radix ); },
        lVal
    );
}

WINUX_FUNC_IMPL(bool) ParseULong( AnsiString const & str, ulong * ulVal )
{
    return Impl_ParseInteger(
        str,
        [] ( AnsiString::value_type const * str, int radix ) { return strtoul( str, nullptr, radix ); },
        ulVal
    );
}

WINUX_FUNC_IMPL(bool) ParseULong( UnicodeString const & str, ulong * ulVal )
{
    return Impl_ParseInteger(
        str,
        [] ( UnicodeString::value_type const * str, int radix ) { return wcstoul( str, nullptr, radix ); },
        ulVal
    );
}

WINUX_FUNC_IMPL(bool) ParseInt64( AnsiString const & str, int64 * i64Val )
{
    return Impl_ParseInteger( str, StrToInt64<char>, i64Val );
}

WINUX_FUNC_IMPL(bool) ParseInt64( UnicodeString const & str, int64 * i64Val )
{
    return Impl_ParseInteger( str, StrToInt64<wchar>, i64Val );
}

WINUX_FUNC_IMPL(bool) ParseUInt64( AnsiString const & str, uint64 * ui64Val )
{
    return Impl_ParseInteger( str, StrToUInt64<char>, ui64Val );
}

WINUX_FUNC_IMPL(bool) ParseUInt64( UnicodeString const & str, uint64 * ui64Val )
{
    return Impl_ParseInteger( str, StrToUInt64<wchar>, ui64Val );
}

template < typename _ChTy, typename _ToIntFx, typename _ToFx, typename _Ty >
inline static bool Impl_ParseFloat( XString<_ChTy> const & str, _ToIntFx && tofnInt, _ToFx && tofn, _Ty * val )
{
    if ( str.length() > 1 && str[0] == Literal<_ChTy>::zeroChar && ( str[1] | 0x20 ) == Literal<_ChTy>::xChar )
    {
        *val = (_Ty)tofnInt( str.c_str() + 2, 16 );
    }
    else if ( str.length() > 1 && str[0] == Literal<_ChTy>::zeroChar && str[1] != Literal<_ChTy>::periodChar && ( str[1] | 0x20 ) != Literal<_ChTy>::eChar )
    {
        *val = (_Ty)tofnInt( str.c_str() + 1, 8 );
    }
    else
    {
        *val = tofn( str.c_str(), nullptr );
    }
    return true;
}

WINUX_FUNC_IMPL(bool) ParseFloat( AnsiString const & str, float * fltVal )
{
    return Impl_ParseFloat( str, StrToInt64<char>, strtof, fltVal );
}

WINUX_FUNC_IMPL(bool) ParseFloat( UnicodeString const & str, float * fltVal )
{
    return Impl_ParseFloat( str, StrToInt64<wchar>, wcstof, fltVal );
}

WINUX_FUNC_IMPL(bool) ParseDouble( AnsiString const & str, double * dblVal )
{
    return Impl_ParseFloat( str, StrToInt64<char>, strtod, dblVal );
}

WINUX_FUNC_IMPL(bool) ParseDouble( UnicodeString const & str, double * dblVal )
{
    return Impl_ParseFloat( str, StrToInt64<wchar>, wcstod, dblVal );
}

// class Buffer -------------------------------------------------------------------------------
void * Buffer::Alloc( size_t size )
{
    return ::malloc(size);
}

void * Buffer::Realloc( void * p, size_t newSize )
{
    return ::realloc( p, newSize );
}

void Buffer::Free( void * p )
{
    ::free(p);
}

// Constructors
Buffer::Buffer()
{
    this->_zeroInit();
}

Buffer::Buffer( void const * buf, size_t size, bool isPeek )
{
    this->_copyConstruct( buf, size, isPeek );
}

Buffer::~Buffer()
{
    this->free();
}

Buffer::Buffer( Buffer const & other )
{
    this->_copyConstruct( other._buf, other._dataSize, other._isPeek );
}

Buffer & Buffer::operator = ( Buffer const & other )
{
    if ( this != &other )
    {
        this->setBuf( other._buf, other._dataSize, other._isPeek );
    }
    return *this;
}

#ifndef MOVE_SEMANTICS_DISABLED

Buffer::Buffer( Buffer && other ) noexcept :
    _buf( std::move(other._buf) ),
    _dataSize( std::move(other._dataSize) ),
    _capacity( std::move(other._capacity) ),
    _isPeek( std::move(other._isPeek) )
{
    other._zeroInit();
}

Buffer & Buffer::operator = ( Buffer && other ) noexcept
{
    if ( this != &other )
    {
        this->free();

        this->_buf = std::move(other._buf);
        this->_dataSize = std::move(other._dataSize);
        this->_capacity = std::move(other._capacity);
        this->_isPeek = std::move(other._isPeek);

        other._zeroInit();
    }
    return *this;
}

Buffer::Buffer( GrowBuffer && other ) noexcept :
    _buf( std::move(other._buf) ),
    _dataSize( std::move(other._dataSize) ),
    _capacity( std::move(other._capacity) ),
    _isPeek( std::move(other._isPeek) )
{
    other._zeroInit();
}

Buffer & Buffer::operator = ( GrowBuffer && other ) noexcept
{
    if ( this != &other )
    {
        this->free();

        this->_buf = std::move(other._buf);
        this->_dataSize = std::move(other._dataSize);
        this->_capacity = std::move(other._capacity);
        this->_isPeek = std::move(other._isPeek);

        other._zeroInit();
    }
    return *this;
}

#endif

void Buffer::setBuf( void const * buf, size_t size, size_t capacity, bool isPeek )
{
    this->free();
    this->_buf = const_cast<void *>(buf);
    this->_dataSize = size;
    this->_capacity = capacity;
    this->_isPeek = isPeek;
    if ( !this->_isPeek ) // 如果不是窥探模式，则需要拷贝数据
    {
        this->_buf = Alloc(this->_capacity);
        if ( buf != NULL )
            memcpy( this->_buf, buf, capacity );
        else
            memset( this->_buf, 0, capacity );
    }
}

void Buffer::alloc( size_t capacity, bool setDataSize )
{
    this->free();
    if ( setDataSize ) this->_dataSize = capacity;
    this->_capacity = capacity;
    this->_isPeek = false;
    this->_buf = Alloc(capacity); // 分配空间
    memset( this->_buf, 0, capacity );
}

void Buffer::realloc( size_t newCapacity )
{
    if ( this->_isPeek ) // 如果是窥探模式首先要变为拷贝模式
    {
        this->peekCopy(true);
    }
    this->_buf = Realloc( this->_buf, newCapacity );
    if ( newCapacity > this->_capacity ) // 当新容量大于当前容量，则初始化大于的部分为0
    {
        memset( ((byte *)this->_buf) + this->_capacity, 0, newCapacity - this->_capacity );
    }
    this->_capacity = newCapacity;

    if ( newCapacity < this->_dataSize ) // 当新容量小于数据量，数据被丢弃
    {
        this->_dataSize = newCapacity;
    }
}

bool Buffer::peekCopy( bool copyCapacity )
{
    if ( this->_isPeek )
    {
        if ( this->_buf != NULL )
        {
            void * buf = this->_buf;
            if ( copyCapacity )
            {
                this->_buf = Alloc(this->_capacity);
                memcpy( this->_buf, buf, this->_capacity );
            }
            else
            {
                this->_buf = Alloc(this->_dataSize);
                memcpy( this->_buf, buf, this->_dataSize );
                this->_capacity = this->_dataSize;
            }
        }
        this->_isPeek = false;
        return true;
    }
    return false;
}

void * Buffer::detachBuf( size_t * size )
{
    void * buf = this->_buf;
    ASSIGN_PTR(size) = this->_dataSize;
    if ( this->_buf != NULL && !this->_isPeek )
    {
        this->_zeroInit();
    }
    return buf;
}

void Buffer::free() noexcept
{
    if ( this->_buf != NULL && !this->_isPeek )
    {
        Free(this->_buf);
        this->_zeroInit();
    }
}

void Buffer::_zeroInit() noexcept
{
    this->_buf = nullptr;
    this->_dataSize = 0U;
    this->_capacity = 0U;
    this->_isPeek = false;
}

void Buffer::_copyConstruct( void const * buf, size_t size, bool isPeek )
{
    this->_buf = const_cast<void *>(buf);
    this->_dataSize = size;
    this->_capacity = size;
    this->_isPeek = isPeek;
    if ( !this->_isPeek ) // 如果不是窥探模式，则需要拷贝数据
    {
        this->_buf = Alloc(this->_capacity);
        if ( buf != NULL )
            memcpy( this->_buf, buf, size );
        else
            memset( this->_buf, 0, size );
    }
}

// class GrowBuffer ---------------------------------------------------------------------------
GrowBuffer::GrowBuffer( size_t capacity )
{
    if ( capacity )
    {
        this->alloc( capacity, false );
    }
}

GrowBuffer::GrowBuffer( GrowBuffer const & other )
{
    this->setBuf( other._buf, other._dataSize, other._capacity, other._isPeek );
}

GrowBuffer & GrowBuffer::operator = ( GrowBuffer const & other )
{
    if ( this != &other )
    {
        this->setBuf( other._buf, other._dataSize, other._capacity, other._isPeek );
    }
    return *this;
}

GrowBuffer::GrowBuffer( Buffer const & other ) : Buffer(other)
{
}

GrowBuffer & GrowBuffer::operator = ( Buffer const & other )
{
    Buffer::operator = (other);
    return *this;
}

#ifndef MOVE_SEMANTICS_DISABLED

GrowBuffer::GrowBuffer( GrowBuffer && other ) noexcept : Buffer( std::move(other) )
{
}

GrowBuffer & GrowBuffer::operator = ( GrowBuffer && other ) noexcept
{
    if ( this != &other )
    {
        Buffer::operator = ( std::move(other) );
    }
    return *this;
}

GrowBuffer::GrowBuffer( Buffer && other ) noexcept : Buffer( std::move(other) )
{
}

GrowBuffer & GrowBuffer::operator = ( Buffer && other ) noexcept
{
    if ( this != &other )
    {
        Buffer::operator = ( std::move(other) );
    }
    return *this;
}

#endif

inline static size_t __AutoIncrement( size_t n )
{
    return ( n + n / 3 + 3 ) / 4 * 4 + 1;
}

void GrowBuffer::append( void const * data, size_t size )
{
    if ( this->_dataSize + size > this->_capacity ) // 需要重新分配大小
    {
        this->realloc( __AutoIncrement( this->_dataSize + size ) );
    }
    if ( data != nullptr ) memcpy( (byte *)this->_buf + this->_dataSize, data, size );
    this->_dataSize += size;
}

// 1 2 3 4 5 6 7 8 9 10
// 0 1 2 3 4 5 6 7 8 9
void GrowBuffer::erase( size_t start, size_t count )
{
    if ( (offset_t)start >= 0 && start < _dataSize )
    {
        if ( count == (size_t)-1 || count >= _dataSize - start ) // 如果count=-1或者count>=数据可删除量，就全部删除
        {
            _dataSize = start;
        }
        else
        {
            // 将后面数据覆盖掉删除的数据，并调整数据大小
            memmove( (byte *)_buf + start, (byte *)_buf + start + count, _dataSize - ( start + count ) );
            _dataSize -= count;
        }

        if ( _dataSize > 0 )
        {
            double delta = _capacity / (double)_dataSize;
            if ( delta > 1.5 )
                this->realloc(_dataSize);
        }
        else
        {
            if ( _capacity - _dataSize > 100 )
            {
                this->realloc(_dataSize);
            }
        }
    }
}

// class MixedLess ----------------------------------------------------------------------------
bool MixedLess::operator () ( Mixed const & v1, Mixed const & v2 ) const
{
    return v1 < v2;
}

// class MixedLessI ---------------------------------------------------------------------------
bool MixedLessI::operator () ( Mixed const & v1, Mixed const & v2 ) const
{
    if ( v1._type == v2._type )
    {
        switch ( v1._type )
        {
        case Mixed::MT_ANSI:
            return StringCompareI( *v1._pStr, *v2._pStr ) < 0;
            break;
        case Mixed::MT_UNICODE:
            return StringCompareI( *v1._pWStr, *v2._pWStr ) < 0;
            break;
        case Mixed::MT_BINARY:
            return BufferCompareI( *v1._pBuf, *v2._pBuf ) < 0;
            break;
        }
    }
    else if ( v1._type > v2._type )
    {
        switch ( v1._type )
        {
        case Mixed::MT_ANSI:
            return StringCompareI( *v1._pStr, v2.operator AnsiString() ) < 0;
            break;
        case Mixed::MT_UNICODE:
            return StringCompareI( *v1._pWStr, v2.operator UnicodeString() ) < 0;
            break;
        case Mixed::MT_BINARY:
            return BufferCompareI( *v1._pBuf, v2.operator Buffer() ) < 0;
            break;
        }
    }
    else // v1._type < v2._type
    {
        switch ( v2._type )
        {
        case Mixed::MT_ANSI:
            return StringCompareI( v1.operator AnsiString(), *v2._pStr ) < 0;
            break;
        case Mixed::MT_UNICODE:
            return StringCompareI( v1.operator UnicodeString(), *v2._pWStr ) < 0;
            break;
        case Mixed::MT_BINARY:
            return BufferCompareI( v1.operator Buffer(), *v2._pBuf ) < 0;
            break;
        }
    }

    return v1 < v2;
}

// class Collection ---------------------------------------------------------------------------
Collection::Collection( bool caseInsensitive )
{
    this->_zeroInit();
    this->create(caseInsensitive);
}

Collection::~Collection()
{
    this->destroy();
}

Collection::Collection( Collection const & other )
{
    this->_copyConstruct(other);
}

Collection & Collection::operator = ( Collection const & other )
{
    if ( this == &other ) return *this;
    if ( this->_pPairList && ( this->_pListItMap || this->_pListItMapI ) ) // 当前已经存在分配的列表和映射表
    {
        if ( other._pPairList )
            *this->_pPairList = *other._pPairList;
        else
            this->_pPairList->clear();

        if ( this->_caseInsensitive )
        {
            // 当前是大小写无关的，但是目标是大小写相关的，删除当前的创建全新的
            if ( other._caseInsensitive == false )
            {
                this->_caseInsensitive = other._caseInsensitive;
                delete this->_pListItMapI;
                this->_pListItMapI = nullptr;
                this->_pListItMap = new MixedListIteratorMap();
                this->_buildMapByPairList();
            }
            else
            {
                this->_pListItMapI->clear();
                this->_buildMapByPairList();
            }
        }
        else
        {
            // 当前是大小写相关的，但是目标是大小写无关的，删除当前的创建全新的
            if ( other._caseInsensitive )
            {
                this->_caseInsensitive = other._caseInsensitive;
                delete this->_pListItMap;
                this->_pListItMap = nullptr;
                this->_pListItMapI = new MixedListIteratorMapI();
                this->_buildMapByPairList();
            }
            else
            {
                this->_pListItMap->clear();
                this->_buildMapByPairList();
            }
        }
    }
    else
    {
        this->destroy();
        this->_copyConstruct(other);
    }
    return *this;
}

Collection::Collection( Collection && other ) noexcept
{
    memcpy( this, &other, sizeof(*this) );
    other._zeroInit();
}

Collection & Collection::operator = ( Collection && other ) noexcept
{
    if ( this != &other )
    {
        this->destroy();
        memcpy( this, &other, sizeof(*this) );
        other._zeroInit();
    }
    return *this;
}

Collection::Collection( $c && coll, bool caseInsensitive )
{
    this->_zeroInit();
    this->assign( std::move(coll), caseInsensitive );
}

Collection::Collection( Mixed const & coll, bool caseInsensitive )
{
    this->_zeroInit();
    this->create(caseInsensitive);

    if ( coll._type == Mixed::MT_COLLECTION )
    {
        coll._pColl->traverse( [this] ( MixedMixedPair const & pr ) {
            this->setPair( pr.first, pr.second );
            return true;
        } );
    }
    else
    {
        Collection tmpColl = coll.operator Collection();
        tmpColl.traverse( [this] ( MixedMixedPair const & pr ) {
            this->setPair( pr.first, pr.second );
            return true;
        } );
    }
}

void Collection::create( bool caseInsensitive )
{
    this->destroy();
    this->_caseInsensitive = caseInsensitive;
    this->_pPairList = new PairList();
    if ( this->_caseInsensitive )
        this->_pListItMapI = new MixedListIteratorMapI();
    else
        this->_pListItMap = new MixedListIteratorMap();
}

void Collection::destroy() noexcept
{
    if ( this->_pPairList ) delete this->_pPairList;
    if ( this->_caseInsensitive )
    {
        if ( this->_pListItMapI ) delete this->_pListItMapI;
    }
    else
    {
        if ( this->_pListItMap ) delete this->_pListItMap;
    }
    this->_zeroInit();
}

void Collection::clear() noexcept
{
    if ( this->_pPairList ) this->_pPairList->clear();
    if ( this->_caseInsensitive )
    {
        if ( this->_pListItMapI ) this->_pListItMapI->clear();
    }
    else
    {
        if ( this->_pListItMap ) this->_pListItMap->clear();
    }
}

Mixed & Collection::operator [] ( Mixed const & k )
{
    if ( this->_caseInsensitive )
    {
        auto itListIt = this->_pListItMapI->find(k);
        if ( itListIt != this->_pListItMapI->end() ) // 存在此key，直接得到列表节点数据引用
        {
            return itListIt->second->second;
        }
        else // 不存在此key，新建列表节点，并存下节点迭代器，返回数据引用
        {
            this->_pPairList->emplace_back( k, mxNull );
            auto it = std::prev( this->_pPairList->end() );
            (*this->_pListItMapI)[k] = it;
            return it->second;
        }
    }
    else
    {
        auto itListIt = this->_pListItMap->find(k);
        if ( itListIt != this->_pListItMap->end() ) // 存在此key，直接得到列表节点数据引用
        {
            return itListIt->second->second;
        }
        else // 不存在此key，新建列表节点，并存下节点迭代器，返回数据引用
        {
            this->_pPairList->emplace_back( k, mxNull );
            auto it = std::prev( this->_pPairList->end() );
            (*this->_pListItMap)[k] = it;
            return it->second;
        }
    }
}

Mixed & Collection::at( Mixed const & k )
{
    return this->operator [] (k);
}

Mixed const & Collection::at( Mixed const & k ) const
{
    if ( this->_caseInsensitive )
        return static_cast<MixedListIteratorMapI const *>(this->_pListItMapI)->at(k)->second;
    else
        return static_cast<MixedListIteratorMap const *>(this->_pListItMap)->at(k)->second;
}

MixedMixedPair & Collection::getFirstPair()
{
    return *this->_pPairList->begin();
}

MixedMixedPair const & Collection::getFirstPair() const
{
    return *static_cast<PairList const *>(this->_pPairList)->begin();
}

void Collection::setNewFirstPair( Mixed const & k, Mixed const & v )
{
    auto itFirstPair = this->_pPairList->begin();
    if ( this->_caseInsensitive )
    {
        this->_pListItMapI->erase(itFirstPair->first);
        (*this->_pListItMapI)[k] = itFirstPair;
    }
    else
    {
        this->_pListItMap->erase(itFirstPair->first);
        (*this->_pListItMap)[k] = itFirstPair;
    }
    itFirstPair->first = k;
    itFirstPair->second = v;
}

void Collection::setNewPair( Mixed const & oldK, Mixed const & k, Mixed const & v )
{
    if ( this->_caseInsensitive )
    {
        auto itListIt = this->_pListItMapI->find(oldK);
        if ( itListIt != this->_pListItMapI->end() ) // 旧key存在
        {
            auto it = itListIt->second;
            this->_pListItMapI->erase(itListIt);
            (*this->_pListItMapI)[k] = it;
            it->first = k;
            it->second = v;
        }
        else // 旧key不存在
        {
            this->setPair( k, v );
        }
    }
    else
    {
        auto itListIt = this->_pListItMap->find(oldK);
        if ( itListIt != this->_pListItMap->end() ) // 旧key存在
        {
            auto it = itListIt->second;
            this->_pListItMap->erase(itListIt);
            (*this->_pListItMap)[k] = it;
            it->first = k;
            it->second = v;
        }
        else // 旧key不存在
        {
            this->setPair( k, v );
        }
    }
}

void Collection::setNewPair( Mixed const & oldK, Mixed const & k, Mixed && v )
{
    if ( this->_caseInsensitive )
    {
        auto itListIt = this->_pListItMapI->find(oldK);
        if ( itListIt != this->_pListItMapI->end() ) // 旧key存在
        {
            auto it = itListIt->second;
            this->_pListItMapI->erase(itListIt);
            (*this->_pListItMapI)[k] = it;
            it->first = k;
            it->second = std::move(v);
        }
        else // 旧key不存在
        {
            this->setPair( k, std::move(v) );
        }
    }
    else
    {
        auto itListIt = this->_pListItMap->find(oldK);
        if ( itListIt != this->_pListItMap->end() ) // 旧key存在
        {
            auto it = itListIt->second;
            this->_pListItMap->erase(itListIt);
            (*this->_pListItMap)[k] = it;
            it->first = k;
            it->second = std::move(v);
        }
        else // 旧key不存在
        {
            this->setPair( k, std::move(v) );
        }
    }
}

void Collection::setPair( Mixed const & k, Mixed const & v )
{
    if ( this->_caseInsensitive )
    {
        auto itListIt = this->_pListItMapI->find(k);
        if ( itListIt != this->_pListItMapI->end() ) // 存在此key
        {
            itListIt->second->second = v;
        }
        else // 不存在此key
        {
            this->_pPairList->emplace_back( k, v );
            auto it = std::prev( this->_pPairList->end() );
            (*this->_pListItMapI)[k] = it;
        }
    }
    else
    {
        auto itListIt = this->_pListItMap->find(k);
        if ( itListIt != this->_pListItMap->end() ) // 存在此key
        {
            itListIt->second->second = v;
        }
        else // 不存在此key
        {
            this->_pPairList->emplace_back( k, v );
            auto it = std::prev( this->_pPairList->end() );
            (*this->_pListItMap)[k] = it;
        }
    }
}

void Collection::setPair( Mixed const & k, Mixed && v )
{
    if ( this->_caseInsensitive )
    {
        auto itListIt = this->_pListItMapI->find(k);
        if ( itListIt != this->_pListItMapI->end() ) // 存在此key
        {
            itListIt->second->second = std::move(v);
        }
        else // 不存在此key
        {
            this->_pPairList->emplace_back( k, std::move(v) );
            auto it = std::prev( this->_pPairList->end() );
            (*this->_pListItMapI)[k] = it;
        }
    }
    else
    {
        auto itListIt = this->_pListItMap->find(k);
        if ( itListIt != this->_pListItMap->end() ) // 存在此key
        {
            itListIt->second->second = std::move(v);
        }
        else // 不存在此key
        {
            this->_pPairList->emplace_back( k, std::move(v) );
            auto it = std::prev( this->_pPairList->end() );
            (*this->_pListItMap)[k] = it;
        }
    }
}

bool Collection::del( Mixed const & k )
{
    if ( this->_caseInsensitive )
    {
        auto itListIt = this->_pListItMapI->find(k);
        if ( itListIt == this->_pListItMapI->end() ) return false;
        this->_pPairList->erase(itListIt->second);
        this->_pListItMapI->erase(itListIt);
    }
    else
    {
        auto itListIt = this->_pListItMap->find(k);
        if ( itListIt == this->_pListItMap->end() ) return false;
        this->_pPairList->erase(itListIt->second);
        this->_pListItMap->erase(itListIt);
    }
    return true;
}

void Collection::reverse()
{
    this->_pPairList->reverse();
}

bool Collection::traverse( TraverseFn cb, TraverseOrder order, bool inverted )
{
    if ( inverted )
    {
        switch ( order )
        {
        case orderInsert:
            for ( auto it = this->_pPairList->rbegin(); it != this->_pPairList->rend(); it++ )
            {
                if ( !cb(*it) )
                {
                    return false;
                }
            }
            break;
        case orderKey:
            for (
                auto itListIt = ( this->_caseInsensitive ? this->_pListItMapI->rbegin() : this->_pListItMap->rbegin() );
                itListIt != ( this->_caseInsensitive ? this->_pListItMapI->rend() : this->_pListItMap->rend() );
                itListIt++
            )
            {
                if ( !cb(*itListIt->second) )
                {
                    return false;
                }
            }
            break;
        }
    }
    else
    {
        switch ( order )
        {
        case orderInsert:
            for ( auto it = this->_pPairList->begin(); it != this->_pPairList->end(); it++ )
            {
                if ( !cb(*it) )
                {
                    return false;
                }
            }
            break;
        case orderKey:
            for (
                auto itListIt = ( this->_caseInsensitive ? this->_pListItMapI->begin() : this->_pListItMap->begin() );
                itListIt != ( this->_caseInsensitive ? this->_pListItMapI->end() : this->_pListItMap->end() );
                itListIt++
            )
            {
                if ( !cb(*itListIt->second) )
                {
                    return false;
                }
            }
            break;
        }
    }
    return true;
}

bool Collection::traverse( ConstTraverseFn cb, TraverseOrder order, bool inverted ) const
{
    if ( inverted )
    {
        switch ( order )
        {
        case orderInsert:
            for ( auto it = this->_pPairList->rbegin(); it != this->_pPairList->rend(); it++ )
            {
                if ( !cb(*it) )
                {
                    return false;
                }
            }
            break;
        case orderKey:
            for (
                auto itListIt = ( this->_caseInsensitive ? this->_pListItMapI->rbegin() : this->_pListItMap->rbegin() );
                itListIt != ( this->_caseInsensitive ? this->_pListItMapI->rend() : this->_pListItMap->rend() );
                itListIt++
            )
            {
                if ( !cb(*itListIt->second) )
                {
                    return false;
                }
            }
            break;
        }
    }
    else
    {
        switch ( order )
        {
        case orderInsert:
            for ( auto it = this->_pPairList->begin(); it != this->_pPairList->end(); it++ )
            {
                if ( !cb(*it) )
                {
                    return false;
                }
            }
            break;
        case orderKey:
            for (
                auto itListIt = ( this->_caseInsensitive ? this->_pListItMapI->begin() : this->_pListItMap->begin() );
                itListIt != ( this->_caseInsensitive ? this->_pListItMapI->end() : this->_pListItMap->end() );
                itListIt++
            )
            {
                if ( !cb(*itListIt->second) )
                {
                    return false;
                }
            }
            break;
        }
    }
    return true;
}

void Collection::_copyConstruct( Collection const & other )
{
    this->_caseInsensitive = other._caseInsensitive;
    this->_pPairList = other._pPairList ? new PairList(*other._pPairList) : new PairList();
    if ( this->_caseInsensitive )
    {
        this->_pListItMapI = new MixedListIteratorMapI();
        this->_buildMapByPairList();
    }
    else
    {
        this->_pListItMap = new MixedListIteratorMap();
        this->_buildMapByPairList();
    }
}

void Collection::_buildMapByPairList()
{
    if ( this->_caseInsensitive )
    {
        for ( auto it = this->_pPairList->begin(); it != this->_pPairList->end(); it++ )
        {
            (*this->_pListItMapI)[it->first] = it;
        }
    }
    else
    {
        for ( auto it = this->_pPairList->begin(); it != this->_pPairList->end(); it++ )
        {
            (*this->_pListItMap)[it->first] = it;
        }
    }
}


// class Mixed --------------------------------------------------------------------------------
// union _LongUlongUnion
union _LongUlongUnion
{
    long l;
    ulong ul;
    _LongUlongUnion( long v ) : l(v) { }
    _LongUlongUnion( ulong v ) : ul(v) { }
};

// union _Int64Uint64Union
union _Int64Uint64Union
{
    int64 i64;
    uint64 ui64;
    _Int64Uint64Union( int64 v ) : i64(v) { }
    _Int64Uint64Union( uint64 v ) : ui64(v) { }
};

// enum Mixed::MixedType strings --------------------------------------------------------------
static AnsiString __MixedTypeStringsA[] = {
    "MT_NULL",
    MIXED_TYPE_LIST(MIXED_TYPE_ENUM_ITEMSTRINGA)
};

static UnicodeString __MixedTypeStringsW[] = {
    L"MT_NULL",
    MIXED_TYPE_LIST(MIXED_TYPE_ENUM_ITEMSTRINGW)
};

WINUX_FUNC_IMPL(AnsiString const &) TypeStringA( Mixed const & v )
{
    return __MixedTypeStringsA[v._type];
}

WINUX_FUNC_IMPL(UnicodeString const &) TypeStringW( Mixed const & v )
{
    return __MixedTypeStringsW[v._type];
}

// Constructors -------------------------------------------------------------------------------
Mixed::Mixed()
{
    _zeroInit();
}

Mixed::Mixed( std::nullptr_t )
{
    _zeroInit();
}

// 基本类型构造函数 -----------------------------------------------------------------------------
Mixed::Mixed( bool boolVal )
{
    _zeroInit();
    this->assign(boolVal);
}

Mixed::Mixed( char chVal )
{
    _zeroInit();
    this->assign(chVal);
}

Mixed::Mixed( byte btVal )
{
    _zeroInit();
    this->assign(btVal);
}

Mixed::Mixed( short shVal )
{
    _zeroInit();
    this->assign(shVal);
}

Mixed::Mixed( ushort ushVal )
{
    _zeroInit();
    this->assign(ushVal);
}

Mixed::Mixed( int iVal )
{
    _zeroInit();
    this->assign(iVal);
}

Mixed::Mixed( uint uiVal )
{
    _zeroInit();
    this->assign(uiVal);
}

Mixed::Mixed( long lVal )
{
    _zeroInit();
    this->assign(lVal);
}

Mixed::Mixed( ulong ulVal )
{
    _zeroInit();
    this->assign(ulVal);
}

Mixed::Mixed( float fltVal )
{
    _zeroInit();
    this->assign(fltVal);
}

Mixed::Mixed( int64 i64Val )
{
    _zeroInit();
    this->assign(i64Val);
}

Mixed::Mixed( uint64 ui64Val )
{
    _zeroInit();
    this->assign(ui64Val);
}

Mixed::Mixed( double dblVal )
{
    _zeroInit();
    this->assign(dblVal);
}

// 字符串构造函数 -------------------------------------------------------------------------------
Mixed::Mixed( AnsiString const & str )
{
    _zeroInit();
    this->assign(str);
}

Mixed::Mixed( UnicodeString const & str )
{
    _zeroInit();
    this->assign(str);
}

Mixed::Mixed( AnsiString && str )
{
    _zeroInit();
    this->assign( std::move(str) );
}

Mixed::Mixed( UnicodeString && str )
{
    _zeroInit();
    this->assign( std::move(str) );
}

Mixed::Mixed( char const * str, size_t len )
{
    _zeroInit();
    this->assign( str, len );
}

Mixed::Mixed( wchar const * str, size_t len )
{
    _zeroInit();
    this->assign( str, len );
}

// Buffer构造函数 ------------------------------------------------------------------------------
Mixed::Mixed( Buffer const & buf )
{
    _zeroInit();
    this->assign(buf);
}

Mixed::Mixed( void const * binaryData, size_t size, bool isPeek )
{
    _zeroInit();
    this->assign( binaryData, size, isPeek );
}

Mixed::Mixed( Buffer && buf )
{
    _zeroInit();
    this->assign( std::move(buf) );
}

Mixed::Mixed( GrowBuffer && buf )
{
    _zeroInit();
    this->assign( std::move(buf) );
}

// Array构造函数 -------------------------------------------------------------------------------
Mixed::Mixed( Mixed * arr, size_t count )
{
    _zeroInit();
    this->assign( arr, count );
}

Mixed::Mixed( std::initializer_list<Mixed> && list )
{
    _zeroInit();
    this->assign( std::move(list) );
}

Mixed::Mixed( $a && arr )
{
    _zeroInit();
    this->assign( std::move(arr) );
}

// Collection构造函数 --------------------------------------------------------------------------
Mixed::Mixed( $c && coll, bool caseInsensitive )
{
    _zeroInit();
    this->assign( std::move(coll), caseInsensitive );
}

Mixed::Mixed( Collection const & coll, bool caseInsensitive )
{
    _zeroInit();
    this->assign( coll, caseInsensitive );
}

// 拷贝和移动 ----------------------------------------------------------------------------------
Mixed::Mixed( Mixed const & other )
{
    this->_copyConstruct(other);
}

Mixed & Mixed::operator = ( Mixed const & other )
{
    if ( this == &other ) return *this;
    this->_copyAssignment(other);
    return *this;
}

Mixed::Mixed( Mixed && other ) noexcept
{
    memcpy( this, &other, sizeof(Mixed) );
    other._zeroInit();
}

Mixed & Mixed::operator = ( Mixed && other ) noexcept
{
    if ( this != &other )
    {
        this->free();

        memcpy( this, &other, sizeof(Mixed) );
        other._zeroInit();
    }
    return *this;
}

Mixed::~Mixed()
{
    this->free();
}

void Mixed::free() noexcept
{
    switch ( this->_type )
    {
    case MT_ANSI:
        if ( this->_pStr != nullptr )
        {
            delete this->_pStr;
        }
        break;
    case MT_UNICODE:
        if ( this->_pWStr != nullptr )
        {
            delete this->_pWStr;
        }
        break;
    case MT_BINARY:
        if ( this->_pBuf != nullptr )
        {
            delete this->_pBuf;
        }
        break;
    case MT_ARRAY:
        if ( this->_pArr != nullptr )
        {
            delete this->_pArr;
        }
        break;
    case MT_COLLECTION:
        if ( this->_pColl != nullptr )
        {
            delete this->_pColl;
        }
        break;
    }
    this->_zeroInit();
}

// --------------------------------------------------------------------------------------------
Mixed::operator bool() const
{
    bool b = false;
    switch ( this->_type )
    {
    case MT_BOOLEAN:
        b = this->_boolVal;
        break;
    case MT_CHAR:
        b = this->_chVal != 0;
        break;
    case MT_BYTE:
        b = this->_btVal != 0;
        break;
    case MT_SHORT:
        b = this->_shVal != 0;
        break;
    case MT_USHORT:
        b = this->_ushVal != 0;
        break;
    case MT_INT:
        b = this->_iVal != 0;
        break;
    case MT_UINT:
        b = this->_uiVal != 0;
        break;
    case MT_LONG:
        b = this->_lVal != 0;
        break;
    case MT_ULONG:
        b = this->_ulVal != 0;
        break;
    case MT_INT64:
        b = this->_i64Val != 0;
        break;
    case MT_UINT64:
        b = this->_ui64Val != 0;
        break;
    case MT_FLOAT:
        b = this->_fltVal != 0;
        break;
    case MT_DOUBLE:
        b = this->_dblVal != 0;
        break;
    case MT_ANSI: // 字符串转成bool型，空串为false，否则为true
        b = !this->_pStr->empty();
        break;
    case MT_UNICODE:
        b = !this->_pWStr->empty();
        break;
    case MT_BINARY:
        b = this->_pBuf->getSize() > 0;
        break;
    case MT_ARRAY:
        b = !this->_pArr->empty();
        break;
    case MT_COLLECTION:
        b = this->_pColl->getCount() > 0;
        break;
    }
    return b;
}

template < typename _Ty >
inline static _Ty __MixedPlainTypeConv( Mixed const * v )
{
    switch ( v->_type )
    {
    case Mixed::MT_NULL:
        return 0;
    case Mixed::MT_BOOLEAN:
        return v->_boolVal ? (_Ty)1 : (_Ty)0;
    case Mixed::MT_CHAR:
        return (_Ty)v->_chVal;
    case Mixed::MT_BYTE:
        return (_Ty)v->_btVal;
    case Mixed::MT_SHORT:
        return (_Ty)v->_shVal;
    case Mixed::MT_USHORT:
        return (_Ty)v->_ushVal;
    case Mixed::MT_INT:
        return (_Ty)v->_iVal;
    case Mixed::MT_UINT:
        return (_Ty)v->_uiVal;
    case Mixed::MT_LONG:
        return (_Ty)v->_lVal;
    case Mixed::MT_ULONG:
        return (_Ty)v->_ulVal;
    case Mixed::MT_INT64:
        return (_Ty)v->_i64Val;
    case Mixed::MT_UINT64:
        return (_Ty)v->_ui64Val;
    case Mixed::MT_FLOAT:
        return (_Ty)v->_fltVal;
    case Mixed::MT_DOUBLE:
        return (_Ty)v->_dblVal;
    case Mixed::MT_ANSI:
    case Mixed::MT_UNICODE:
    case Mixed::MT_BINARY:
    case Mixed::MT_ARRAY:
    case Mixed::MT_COLLECTION:
    default:
        throw MixedError( MixedError::meCantConverted, TypeStringA(*v) + " is not base type, can't convert to numeric" );
        break;
    }
}

char Mixed::toChar() const
{
    char chVal = 0;
    switch ( this->_type )
    {
    case MT_CHAR:
        chVal = this->_chVal;
        break;
    case MT_ANSI:
        chVal = (char)this->operator int();
        break;
    case MT_UNICODE:
        chVal = (char)this->operator int();
        break;
    case MT_BINARY:
        if ( this->_pBuf->getSize() > 0 )
        {
            chVal = *this->_pBuf->get<char>();
        }
        break;
    case MT_ARRAY:
        if ( this->_pArr->size() > 0 )
        {
            chVal = this->_pArr->at(0).to<char>();
        }
        break;
    case MT_COLLECTION:
        if ( this->_pColl->getCount() > 0 )
        {
            chVal = this->_pColl->getFirstPair().second.to<char>();
        }
        break;
    default:
        chVal = __MixedPlainTypeConv<char>(this);
        break;
    }
    return chVal;
}

Mixed::operator byte() const
{
    byte btVal = 0U;
    switch ( this->_type )
    {
    case MT_BYTE:
        btVal = this->_btVal;
        break;
    case MT_ANSI:
        btVal = (byte)this->operator uint();
        break;
    case MT_UNICODE:
        btVal = (byte)this->operator uint();
        break;
    case MT_BINARY:
        if ( this->_pBuf->getSize() > 0 )
        {
            btVal = *this->_pBuf->get<byte>();
        }
        break;
    case MT_ARRAY:
        if ( this->_pArr->size() > 0 )
        {
            btVal = this->_pArr->at(0).to<byte>();
        }
        break;
    case MT_COLLECTION:
        if ( this->_pColl->getCount() > 0 )
        {
            btVal = this->_pColl->getFirstPair().second.to<byte>();
        }
        break;
    default:
        btVal = __MixedPlainTypeConv<byte>(this);
        break;
    }
    return btVal;
}

Mixed::operator short() const
{
    short shVal = 0;
    switch ( this->_type )
    {
    case MT_SHORT:
        shVal = this->_shVal;
        break;
    case MT_ANSI:
        shVal = (short)this->operator int();
        break;
    case MT_UNICODE:
        shVal = (short)this->operator int();
        break;
    case MT_BINARY:
        if ( this->_pBuf->getSize() > sizeof(short) - 1 )
        {
            shVal = *this->_pBuf->get<short>();
        }
        else if ( this->_pBuf->getSize() > sizeof(char) - 1 )
        {
            shVal = *this->_pBuf->get<char>();
        }
        break;
    case MT_ARRAY:
        if ( this->_pArr->size() > 0 )
        {
            shVal = this->_pArr->at(0).to<short>();
        }
        break;
    case MT_COLLECTION:
        if ( this->_pColl->getCount() > 0 )
        {
            shVal = this->_pColl->getFirstPair().second.to<short>();
        }
        break;
    default:
        shVal = __MixedPlainTypeConv<short>(this);
        break;
    }
    return shVal;
}

Mixed::operator ushort() const
{
    ushort ushVal = 0;
    switch ( this->_type )
    {
    case MT_USHORT:
        ushVal = this->_ushVal;
        break;
    case MT_ANSI:
        ushVal = (ushort)this->operator uint();
        break;
    case MT_UNICODE:
        ushVal = (ushort)this->operator uint();
        break;
    case MT_BINARY:
        if ( this->_pBuf->getSize() > sizeof(ushort) - 1 )
        {
            ushVal = *this->_pBuf->get<ushort>();
        }
        else if ( this->_pBuf->getSize() > sizeof(byte) - 1 )
        {
            ushVal = *this->_pBuf->get<byte>();
        }
        break;
    case MT_ARRAY:
        if ( this->_pArr->size() > 0 )
        {
            ushVal = this->_pArr->at(0).to<ushort>();
        }
        break;
    case MT_COLLECTION:
        if ( this->_pColl->getCount() > 0 )
        {
            ushVal = this->_pColl->getFirstPair().second.to<ushort>();
        }
        break;
    default:
        ushVal = __MixedPlainTypeConv<ushort>(this);
        break;
    }
    return ushVal;
}

Mixed::operator int() const
{
    int iVal = 0;
    switch ( this->_type )
    {
    case MT_INT:
        iVal = this->_iVal;
        break;
    case MT_ANSI:
        ParseInt( *this->_pStr, &iVal );
        break;
    case MT_UNICODE:
        ParseInt( *this->_pWStr, &iVal );
        break;
    case MT_BINARY:
        if ( this->_pBuf->getSize() > sizeof(int) - 1 )
        {
            iVal = *this->_pBuf->get<int>();
        }
        else if ( this->_pBuf->getSize() > sizeof(short) - 1 )
        {
            iVal = *this->_pBuf->get<short>();
        }
        else if ( this->_pBuf->getSize() > sizeof(char) - 1 )
        {
            iVal = *this->_pBuf->get<char>();
        }
        break;
    case MT_ARRAY:
        if ( this->_pArr->size() > 0 )
        {
            iVal = this->_pArr->at(0).to<int>();
        }
        break;
    case MT_COLLECTION:
        if ( this->_pColl->getCount() > 0 )
        {
            iVal = this->_pColl->getFirstPair().second.to<int>();
        }
        break;
    default:
        iVal = __MixedPlainTypeConv<int>(this);
        break;
    }
    return iVal;
}

Mixed::operator uint() const
{
    uint uiVal = 0U;
    switch ( this->_type )
    {
    case MT_UINT:
        uiVal = this->_uiVal;
        break;
    case MT_ANSI:
        ParseUInt( *this->_pStr, &uiVal );
        break;
    case MT_UNICODE:
        ParseUInt( *this->_pWStr, &uiVal );
        break;
    case MT_BINARY:
        if ( this->_pBuf->getSize() > sizeof(uint) - 1 )
        {
            uiVal = *this->_pBuf->get<uint>();
        }
        else if ( this->_pBuf->getSize() > sizeof(ushort) - 1 )
        {
            uiVal = *this->_pBuf->get<ushort>();
        }
        else if ( this->_pBuf->getSize() > sizeof(byte) - 1 )
        {
            uiVal = *this->_pBuf->get<byte>();
        }
        break;
    case MT_ARRAY:
        if ( this->_pArr->size() > 0 )
        {
            uiVal = this->_pArr->at(0).to<uint>();
        }
        break;
    case MT_COLLECTION:
        if ( this->_pColl->getCount() > 0 )
        {
            uiVal = this->_pColl->getFirstPair().second.to<uint>();
        }
        break;
    default:
        uiVal = __MixedPlainTypeConv<uint>(this);
        break;
    }
    return uiVal;
}

Mixed::operator long() const
{
    long lVal = 0L;
    switch ( this->_type )
    {
    case MT_LONG:
        lVal = this->_lVal;
        break;
    case MT_ANSI:
        ParseLong( *this->_pStr, &lVal );
        break;
    case MT_UNICODE:
        ParseLong( *this->_pWStr, &lVal );
        break;
    case MT_BINARY:
        if ( this->_pBuf->getSize() > sizeof(long) - 1 )
        {
            lVal = *this->_pBuf->get<long>();
        }
        else if ( this->_pBuf->getSize() > sizeof(short) - 1 )
        {
            lVal = *this->_pBuf->get<short>();
        }
        else if ( this->_pBuf->getSize() > sizeof(char) - 1 )
        {
            lVal = *this->_pBuf->get<char>();
        }
        break;
    case MT_ARRAY:
        if ( this->_pArr->size() > 0 )
        {
            lVal = this->_pArr->at(0).to<long>();
        }
        break;
    case MT_COLLECTION:
        if ( this->_pColl->getCount() > 0 )
        {
            lVal = this->_pColl->getFirstPair().second.to<long>();
        }
        break;
    default:
        lVal = __MixedPlainTypeConv<long>(this);
        break;
    }
    return lVal;
}

Mixed::operator ulong() const
{
    ulong ulVal = 0UL;
    switch ( this->_type )
    {
    case MT_ULONG:
        ulVal = this->_ulVal;
        break;
    case MT_ANSI:
        ParseULong( *this->_pStr, &ulVal );
        break;
    case MT_UNICODE:
        ParseULong( *this->_pWStr, &ulVal );
        break;
    case MT_BINARY:
        if ( this->_pBuf->getSize() > sizeof(ulong) - 1 )
        {
            ulVal = *this->_pBuf->get<ulong>();
        }
        else if ( this->_pBuf->getSize() > sizeof(ushort) - 1 )
        {
            ulVal = *this->_pBuf->get<ushort>();
        }
        else if ( this->_pBuf->getSize() > sizeof(byte) - 1 )
        {
            ulVal = *this->_pBuf->get<byte>();
        }
        break;
    case MT_ARRAY:
        if ( this->_pArr->size() > 0 )
        {
            ulVal = this->_pArr->at(0).to<ulong>();
        }
        break;
    case MT_COLLECTION:
        if ( this->_pColl->getCount() > 0 )
        {
            ulVal = this->_pColl->getFirstPair().second.to<ulong>();
        }
        break;
    default:
        ulVal = __MixedPlainTypeConv<ulong>(this);
        break;
    }
    return ulVal;
}

Mixed::operator float() const
{
    float fltVal = 0.0f;
    switch ( this->_type )
    {
    case MT_FLOAT:
        fltVal = this->_fltVal;
        break;
    case MT_ANSI:
        ParseFloat( *this->_pStr, &fltVal );
        break;
    case MT_UNICODE:
        ParseFloat( *this->_pWStr, &fltVal );
        break;
    case MT_BINARY:
        if ( this->_pBuf->getSize() > sizeof(float) - 1 )
        {
            fltVal = *this->_pBuf->get<float>();
        }
        else if ( this->_pBuf->getSize() > sizeof(short) - 1 )
        {
            fltVal = *this->_pBuf->get<short>();
        }
        else if ( this->_pBuf->getSize() > sizeof(char) - 1 )
        {
            fltVal = *this->_pBuf->get<char>();
        }
        break;
    case MT_ARRAY:
        if ( this->_pArr->size() > 0 )
        {
            fltVal = this->_pArr->at(0).to<float>();
        }
        break;
    case MT_COLLECTION:
        if ( this->_pColl->getCount() > 0 )
        {
            fltVal = this->_pColl->getFirstPair().second.to<float>();
        }
        break;
    default:
        fltVal = __MixedPlainTypeConv<float>(this);
        break;
    }
    return fltVal;
}

Mixed::operator int64() const
{
    int64 i64Val = 0;
    switch ( this->_type )
    {
    case MT_INT64:
        i64Val = this->_i64Val;
        break;
    case MT_ANSI:
        ParseInt64( *this->_pStr, &i64Val );
        break;
    case MT_UNICODE:
        ParseInt64( *this->_pWStr, &i64Val );
        break;
    case MT_BINARY:
        if ( this->_pBuf->getSize() > sizeof(int64) - 1 )
        {
            i64Val = *this->_pBuf->get<int64>();
        }
        else if ( this->_pBuf->getSize() > sizeof(long) - 1 )
        {
            i64Val = *this->_pBuf->get<long>();
        }
        else if ( this->_pBuf->getSize() > sizeof(short) - 1 )
        {
            i64Val = *this->_pBuf->get<short>();
        }
        else if ( this->_pBuf->getSize() > sizeof(char) - 1 )
        {
            i64Val = *this->_pBuf->get<char>();
        }
        break;
    case MT_ARRAY:
        if ( this->_pArr->size() > 0 )
        {
            i64Val = this->_pArr->at(0).to<int64>();
        }
        break;
    case MT_COLLECTION:
        if ( this->_pColl->getCount() > 0 )
        {
            i64Val = this->_pColl->getFirstPair().second.to<int64>();
        }
        break;
    default:
        i64Val = __MixedPlainTypeConv<int64>(this);
        break;
    }
    return i64Val;
}

Mixed::operator uint64() const
{
    uint64 ui64Val = 0U;
    switch ( this->_type )
    {
    case MT_UINT64:
        ui64Val = this->_ui64Val;
        break;
    case MT_ANSI:
        ParseUInt64( *this->_pStr, &ui64Val );
        break;
    case MT_UNICODE:
        ParseUInt64( *this->_pWStr, &ui64Val );
        break;
    case MT_BINARY:
        if ( this->_pBuf->getSize() > sizeof(uint64) - 1 )
        {
            ui64Val = *this->_pBuf->get<uint64>();
        }
        else if ( this->_pBuf->getSize() > sizeof(ulong) - 1 )
        {
            ui64Val = *this->_pBuf->get<ulong>();
        }
        else if ( this->_pBuf->getSize() > sizeof(ushort) - 1 )
        {
            ui64Val = *this->_pBuf->get<ushort>();
        }
        else if ( this->_pBuf->getSize() > sizeof(byte) - 1 )
        {
            ui64Val = *this->_pBuf->get<byte>();
        }
        break;
    case MT_ARRAY:
        if ( this->_pArr->size() > 0 )
        {
            ui64Val = this->_pArr->at(0).to<uint64>();
        }
        break;
    case MT_COLLECTION:
        if ( this->_pColl->getCount() > 0 )
        {
            ui64Val = this->_pColl->getFirstPair().second.to<uint64>();
        }
        break;
    default:
        ui64Val = __MixedPlainTypeConv<uint64>(this);
        break;
    }
    return ui64Val;
}

Mixed::operator double() const
{
    double dblVal = 0.0;
    switch ( this->_type )
    {
    case MT_DOUBLE:
        dblVal = this->_dblVal;
        break;
    case MT_ANSI:
        ParseDouble( *this->_pStr, &dblVal );
        break;
    case MT_UNICODE:
        ParseDouble( *this->_pWStr, &dblVal );
        break;
    case MT_BINARY:
        if ( this->_pBuf->getSize() > sizeof(double) - 1 )
        {
            dblVal = *this->_pBuf->get<double>();
        }
        else if ( this->_pBuf->getSize() > sizeof(ulong) - 1 )
        {
            dblVal = *this->_pBuf->get<ulong>();
        }
        else if ( this->_pBuf->getSize() > sizeof(ushort) - 1 )
        {
            dblVal = *this->_pBuf->get<ushort>();
        }
        else if ( this->_pBuf->getSize() > sizeof(byte) - 1 )
        {
            dblVal = *this->_pBuf->get<byte>();
        }
        break;
    case MT_ARRAY:
        if ( this->_pArr->size() > 0 )
        {
            dblVal = this->_pArr->at(0).to<double>();
        }
        break;
    case MT_COLLECTION:
        if ( this->_pColl->getCount() > 0 )
        {
            dblVal = this->_pColl->getFirstPair().second.to<double>();
        }
        break;
    default:
        dblVal = __MixedPlainTypeConv<double>(this);
        break;
    }
    return dblVal;
}

Mixed::operator AnsiString() const
{
    // other to string
    AnsiString s;
    switch ( this->_type )
    {
    case MT_BOOLEAN:
        s = this->_boolVal ? "1" : "";
        break;
    case MT_CHAR:
        s = this->_chVal;
        break;
    case MT_BYTE:
        {
            char buf[10] = { 0 };
            sprintf( buf, "0x%02X", (uint)this->_btVal );
            s = buf;
        }
        break;
    case MT_SHORT:
        {
            std::ostringstream sout;
            sout << this->_shVal;
            s = sout.str();
        }
        break;
    case MT_USHORT:
        {
            std::ostringstream sout;
            sout << this->_ushVal;
            s = sout.str();
        }
        break;
    case MT_INT:
        {
            std::ostringstream sout;
            sout << this->_iVal;
            s = sout.str();
        }
        break;
    case MT_UINT:
        {
            std::ostringstream sout;
            sout << this->_uiVal;
            s = sout.str();
        }
        break;
    case MT_LONG:
        {
            std::ostringstream sout;
            sout << this->_lVal;
            s = sout.str();
        }
        break;
    case MT_ULONG:
        {
            std::ostringstream sout;
            sout << this->_ulVal;
            s = sout.str();
        }
        break;
    case MT_INT64:
        {
            char buf[30] = { 0 };
        #if defined(_MSC_VER) || defined(WIN32)
            sprintf( buf, "%I64d", this->_i64Val );
        #else
            sprintf( buf, "%lld", this->_i64Val );
        #endif // IS_WINDOWS
            s = buf;
        }
        break;
    case MT_UINT64:
        {
            char buf[30] = { 0 };
        #if defined(_MSC_VER) || defined(WIN32)
            sprintf( buf, "%I64u", this->_ui64Val );
        #else
            sprintf( buf, "%llu", this->_ui64Val );
        #endif
            s = buf;
        }
        break;
    case MT_FLOAT:
        {
            std::ostringstream sout;
            sout << std::setprecision(6) << this->_fltVal;
            s = sout.str();
        }
        break;
    case MT_DOUBLE:
        {
            std::ostringstream sout;
            sout << std::setprecision(15) << this->_dblVal;
            s = sout.str();
        }
        break;
    case MT_ANSI:
        s = *this->_pStr;
        break;
    case MT_UNICODE:
        s = UnicodeToLocal(*this->_pWStr);
        break;
    case MT_BINARY:
        s = this->_pBuf->toAnsi();
        break;
    case MT_ARRAY:
        s = MixedToJsonA( *this, false );
        break;
    case MT_COLLECTION:
        s = MixedToJsonA( *this, false );
        break;
    default:
        //s = "null";
        break;
    }
    return s;
}

Mixed::operator UnicodeString() const
{
    // other to unicode
    UnicodeString s;
    switch ( this->_type )
    {
    case MT_BOOLEAN:
        s = this->_boolVal ? L"1" : L"";
        break;
    case MT_CHAR:
        s = (UnicodeString::value_type)this->_chVal;
        break;
    case MT_BYTE:
        {
            wchar buf[10] = { 0 };
        #if defined(_MSC_VER) || defined(WIN32)
            swprintf( buf, L"0x%02X", this->_btVal );
        #else
            swprintf( buf, 4, L"0x%02X", this->_btVal );
        #endif
            s = buf;
        }
        break;
    case MT_SHORT:
        {
            std::wostringstream sout;
            sout << this->_shVal;
            s = sout.str();
        }
        break;
    case MT_USHORT:
        {
            std::wostringstream sout;
            sout << this->_ushVal;
            s = sout.str();
        }
        break;
    case MT_INT:
        {
            std::wostringstream sout;
            sout << this->_iVal;
            s = sout.str();
        }
        break;
    case MT_UINT:
        {
            std::wostringstream sout;
            sout << this->_uiVal;
            s = sout.str();
        }
        break;
    case MT_LONG:
        {
            std::wostringstream sout;
            sout << this->_lVal;
            s = sout.str();
        }
        break;
    case MT_ULONG:
        {
            std::wostringstream sout;
            sout << this->_ulVal;
            s = sout.str();
        }
        break;
    case MT_INT64:
        {
            wchar buf[30] = { 0 };
        #if defined(_MSC_VER) || defined(WIN32)
            swprintf( buf, L"%I64d", this->_i64Val );
        #else
            swprintf( buf, 30, L"%lld", this->_i64Val );
        #endif
            s = buf;

        }
        break;
    case MT_UINT64:
        {
            wchar buf[30] = { 0 };
        #if defined(_MSC_VER) || defined(WIN32)
            swprintf( buf, L"%I64u", this->_ui64Val );
        #else
            swprintf( buf, 30, L"%llu", this->_ui64Val );
        #endif
            s = buf;
        }
        break;
    case MT_FLOAT:
        {
            std::wostringstream sout;
            sout << std::setprecision(6) << this->_fltVal;
            s = sout.str();
        }
        break;
    case MT_DOUBLE:
        {
            std::wostringstream sout;
            sout << std::setprecision(15) << this->_dblVal;
            s = sout.str();
        }
        break;
    case MT_ANSI:
        s = LocalToUnicode(*this->_pStr);
        break;
    case MT_UNICODE:
        s = *this->_pWStr;
        break;
    case MT_BINARY:
        s = this->_pBuf->toUnicode();
        break;
    case MT_ARRAY:
        s = MixedToJsonW( *this, false );
        break;
    case MT_COLLECTION:
        s = MixedToJsonW( *this, false );
        break;
    default:
        //s = L"null";
        break;
    }
    return s;
}

Mixed::operator Buffer() const
{
    Buffer buf;
    switch ( this->_type )
    {
    case MT_BOOLEAN:
        {
            buf.alloc( sizeof(this->_boolVal) );
            memcpy( buf.getBuf(), &this->_boolVal, buf.getSize() );
        }
        break;
    case MT_CHAR:
        {
            buf.alloc( sizeof(this->_chVal) );
            memcpy( buf.getBuf(), &this->_chVal, buf.getSize() );
        }
        break;
    case MT_BYTE:
        {
            buf.alloc( sizeof(this->_btVal) );
            memcpy( buf.getBuf(), &this->_btVal, buf.getSize() );
        }
        break;
    case MT_SHORT:
        {
            buf.alloc( sizeof(this->_shVal) );
            memcpy( buf.getBuf(), &this->_shVal, buf.getSize() );
        }
        break;
    case MT_USHORT:
        {
            buf.alloc( sizeof(this->_ushVal) );
            memcpy( buf.getBuf(), &this->_ushVal, buf.getSize() );
        }
        break;
    case MT_INT:
        {
            buf.alloc( sizeof(this->_iVal) );
            memcpy( buf.getBuf(), &this->_iVal, buf.getSize() );
        }
        break;
    case MT_UINT:
        {
            buf.alloc( sizeof(this->_uiVal) );
            memcpy( buf.getBuf(), &this->_uiVal, buf.getSize() );
        }
        break;
    case MT_LONG:
        {
            buf.alloc( sizeof(this->_lVal) );
            memcpy( buf.getBuf(), &this->_lVal, buf.getSize() );
        }
        break;
    case MT_ULONG:
        {
            buf.alloc( sizeof(this->_ulVal) );
            memcpy( buf.getBuf(), &this->_ulVal, buf.getSize() );
        }
        break;
    case MT_INT64:
        {
            buf.alloc( sizeof(this->_i64Val) );
            memcpy( buf.getBuf(), &this->_i64Val, buf.getSize() );
        }
        break;
    case MT_UINT64:
        {
            buf.alloc( sizeof(this->_ui64Val) );
            memcpy( buf.getBuf(), &this->_ui64Val, buf.getSize() );
        }
        break;
    case MT_FLOAT:
        {
            buf.alloc( sizeof(this->_fltVal) );
            memcpy( buf.getBuf(), &this->_fltVal, buf.getSize() );
        }
        break;
    case MT_DOUBLE:
        {
            buf.alloc( sizeof(this->_dblVal) );
            memcpy( buf.getBuf(), &this->_dblVal, buf.getSize() );
        }
        break;
    case MT_ANSI:
        {
            buf.alloc( this->_pStr->size() * sizeof(AnsiString::value_type) );
            memcpy( buf.getBuf(), this->_pStr->c_str(), buf.getSize() );
        }
        break;
    case MT_UNICODE:
        {
            buf.alloc( this->_pWStr->size() * sizeof(UnicodeString::value_type) );
            memcpy( buf.getBuf(), this->_pWStr->c_str(), buf.getSize() );
        }
        break;
    case MT_BINARY:
        {
            buf.setBuf( this->_pBuf->getBuf(), this->_pBuf->getSize(), false );
        }
        break;
    case MT_ARRAY:
        {
            GrowBuffer tmpBuf(64);
            size_t n = this->_pArr->size();
            for ( size_t i = 0; i < n; ++i )
            {
                Buffer aBuf = this->_pArr->at(i).operator Buffer();
                tmpBuf.append( aBuf.getBuf(), aBuf.getSize() );
            }
            buf = std::move(tmpBuf);
        }
        break;
    case MT_COLLECTION:
        {
            GrowBuffer tmpBuf(64);
            this->_pColl->traverse( [&tmpBuf] ( MixedMixedPair & pr ) {
                Buffer aBuf = pr.second.operator Buffer();
                tmpBuf.append( aBuf.getBuf(), aBuf.getSize() );
                return true;
            } );
            buf = std::move(tmpBuf);
        }
        break;
    default:
        // empty buffer.
        break;
    }
    return buf;
}

Mixed::operator MixedArray() const
{
    MixedArray arr;
    switch ( this->_type )
    {
    case MT_BOOLEAN:
        arr.push_back(this->_boolVal);
        break;
    case MT_CHAR:
        arr.push_back(this->_chVal);
        break;
    case MT_BYTE:
        arr.push_back(this->_btVal);
        break;
    case MT_SHORT:
        arr.push_back(this->_shVal);
        break;
    case MT_USHORT:
        arr.push_back(this->_ushVal);
        break;
    case MT_INT:
        arr.push_back(this->_iVal);
        break;
    case MT_UINT:
        arr.push_back(this->_uiVal);
        break;
    case MT_LONG:
        arr.push_back(this->_lVal);
        break;
    case MT_ULONG:
        arr.push_back(this->_ulVal);
        break;
    case MT_INT64:
        arr.push_back(this->_i64Val);
        break;
    case MT_UINT64:
        arr.push_back(this->_ui64Val);
        break;
    case MT_FLOAT:
        arr.push_back(this->_fltVal);
        break;
    case MT_DOUBLE:
        arr.push_back(this->_dblVal);
        break;
    case MT_ANSI:
        arr.push_back(*this->_pStr);
        break;
    case MT_UNICODE:
        arr.push_back(*this->_pWStr);
        break;
    case MT_BINARY:
        arr.push_back(*this->_pBuf);
        break;
    case MT_ARRAY:
        arr = *this->_pArr;
        break;
    case MT_COLLECTION:
        this->_pColl->traverse( [this, &arr] ( MixedMixedPair const & kv ) {
            Mixed pr;
            pr.setPair( this->_pColl->getCaseInsensitive() )( kv.first, kv.second );
            arr.push_back( std::move(pr) );
            return true;
        } );
        break;
    default:
        // empty array.
        break;
    }
    return arr;
}

Mixed::operator Collection() const
{
    Collection coll;
    switch ( this->_type )
    {
    case MT_BOOLEAN:
        coll.setPair( MT_BOOLEAN, this->_boolVal );
        break;
    case MT_CHAR:
        coll.setPair( MT_CHAR, this->_chVal );
        break;
    case MT_BYTE:
        coll.setPair( MT_BYTE, this->_btVal );
        break;
    case MT_SHORT:
        coll.setPair( MT_SHORT, this->_shVal );
        break;
    case MT_USHORT:
        coll.setPair( MT_USHORT, this->_ushVal );
        break;
    case MT_INT:
        coll.setPair( MT_INT, this->_iVal );
        break;
    case MT_UINT:
        coll.setPair( MT_UINT, this->_uiVal );
        break;
    case MT_LONG:
        coll.setPair( MT_LONG, this->_lVal );
        break;
    case MT_ULONG:
        coll.setPair( MT_ULONG, this->_ulVal );
        break;
    case MT_INT64:
        coll.setPair( MT_INT64, this->_i64Val );
        break;
    case MT_UINT64:
        coll.setPair( MT_UINT64, this->_ui64Val );
        break;
    case MT_FLOAT:
        coll.setPair( MT_FLOAT, this->_fltVal );
        break;
    case MT_DOUBLE:
        coll.setPair( MT_DOUBLE, this->_dblVal );
        break;
    case MT_ANSI:
        coll.setPair( MT_ANSI, *this->_pStr );
        break;
    case MT_UNICODE:
        coll.setPair( MT_UNICODE, *this->_pWStr );
        break;
    case MT_BINARY:
        coll.setPair( MT_BINARY, *this->_pBuf );
        break;
    case MT_ARRAY:
        for ( size_t i = 0; i < this->_pArr->size(); ++i )
        {
            Mixed const & e = (*this->_pArr)[i];
            if ( e.isCollection() )
            {
                e._pColl->traverse( [&coll] ( MixedMixedPair const & pr ) {
                    coll.setPair( pr.first, pr.second );
                    return true;
                } );
            }
            else
            {
                coll.setPair( i, e );
            }
        }
        break;
    case MT_COLLECTION:
        coll = *this->_pColl;
        break;
    default:
        // empty collection.
        break;
    }
    return coll;
}

// 比较操作符 ----------------------------------------------------------------------------------
bool Mixed::operator == ( Mixed const & other ) const
{
    if ( this->_type == other._type )
    {
        switch ( this->_type )
        {
        case MT_NULL:
            return true;
            break;
        case MT_BOOLEAN:
            return this->_boolVal == other._boolVal;
            break;
        case MT_CHAR:
            return this->_chVal == other._chVal;
            break;
        case MT_BYTE:
            return this->_btVal == other._btVal;
            break;
        case MT_SHORT:
            return this->_shVal == other._shVal;
            break;
        case MT_USHORT:
            return this->_ushVal == other._ushVal;
            break;
        case MT_INT:
            return this->_iVal == other._iVal;
            break;
        case MT_UINT:
            return this->_uiVal == other._uiVal;
            break;
        case MT_LONG:
            return this->_lVal == other._lVal;
            break;
        case MT_ULONG:
            return this->_ulVal == other._ulVal;
            break;
        case MT_INT64:
            return this->_i64Val == other._i64Val;
            break;
        case MT_UINT64:
            return this->_ui64Val == other._ui64Val;
            break;
        case MT_FLOAT:
            return this->_fltVal == other._fltVal;
            break;
        case MT_DOUBLE:
            return this->_dblVal == other._dblVal;
            break;
        case MT_ANSI:
            return *this->_pStr == *other._pStr;
            break;
        case MT_UNICODE:
            return *this->_pWStr == *other._pWStr;
            break;
        case MT_BINARY:
            return BufferCompare( *this->_pBuf, *other._pBuf ) == 0;
            break;
        case MT_ARRAY:
            return ArrayEqual( *this->_pArr, *other._pArr );
            break;
        case MT_COLLECTION:
            return CollectionEqual( *this->_pColl, *other._pColl );
            break;
        }
    }
    else if ( this->_type > other._type )
    {
        switch ( this->_type )
        {
        case MT_NULL:
            return other._type == MT_NULL;
            break;
        case MT_BOOLEAN:
            return this->_boolVal == (bool)other;
            break;
        case MT_CHAR:
            return this->_chVal == other.toChar();
            break;
        case MT_BYTE:
            return this->_btVal == (byte)other;
            break;
        case MT_SHORT:
            return this->_shVal == (short)other;
            break;
        case MT_USHORT:
            return this->_ushVal == (ushort)other;
            break;
        case MT_INT:
            return this->_iVal == (int)other;
            break;
        case MT_UINT:
            return this->_uiVal == (uint)other;
            break;
        case MT_LONG:
            return this->_lVal == (long)other;
            break;
        case MT_ULONG:
            return this->_ulVal == (ulong)other;
            break;
        case MT_INT64:
            return this->_i64Val == (int64)other;
            break;
        case MT_UINT64:
            return this->_ui64Val == (uint64)other;
            break;
        case MT_FLOAT:
            return this->_fltVal == (float)other;
            break;
        case MT_DOUBLE:
            return this->_dblVal == (double)other;
            break;
        case MT_ANSI:
            return *this->_pStr == other.operator AnsiString();
            break;
        case MT_UNICODE:
            return *this->_pWStr == other.operator UnicodeString();
            break;
        case MT_BINARY:
            return BufferCompare( *this->_pBuf, other.operator Buffer() ) == 0;
            break;
        case MT_ARRAY:
            return ArrayEqual( *this->_pArr, other.operator MixedArray() );
            break;
        case MT_COLLECTION:
            return CollectionEqual( *this->_pColl, other.operator Collection() );
            break;
        }
    }
    else // this->_type < other._type
    {
        switch ( other._type )
        {
        case MT_NULL:
            return this->_type == MT_NULL;
            break;
        case MT_BOOLEAN:
            return (bool)*this == other._boolVal;
            break;
        case MT_CHAR:
            return this->toChar() == other._chVal;
            break;
        case MT_BYTE:
            return (byte)*this == other._btVal;
            break;
        case MT_SHORT:
            return (short)*this == other._shVal;
            break;
        case MT_USHORT:
            return (ushort)*this == other._ushVal;
            break;
        case MT_INT:
            return (int)*this == other._iVal;
            break;
        case MT_UINT:
            return (uint)*this == other._uiVal;
            break;
        case MT_LONG:
            return (long)*this == other._lVal;
            break;
        case MT_ULONG:
            return (ulong)*this == other._ulVal;
            break;
        case MT_INT64:
            return (int64)*this == other._i64Val;
            break;
        case MT_UINT64:
            return (uint64)*this == other._ui64Val;
            break;
        case MT_FLOAT:
            return (float)*this == other._fltVal;
            break;
        case MT_DOUBLE:
            return (double)*this == other._dblVal;
            break;
        case MT_ANSI:
            return this->operator AnsiString() == *other._pStr;
            break;
        case MT_UNICODE:
            return this->operator UnicodeString() == *other._pWStr;
            break;
        case MT_BINARY:
            return BufferCompare( this->operator Buffer(), *other._pBuf ) == 0;
            break;
        case MT_ARRAY:
            return ArrayEqual( this->operator MixedArray(), *other._pArr );
            break;
        case MT_COLLECTION:
            return CollectionEqual( this->operator Collection(), *other._pColl );
            break;
        }
    }

    return false;
}

bool Mixed::operator < ( Mixed const & other ) const
{
    if ( this->_type == other._type )
    {
        switch ( this->_type )
        {
        case MT_NULL:
            return false;
            break;
        case MT_BOOLEAN:
            return this->_boolVal < other._boolVal;
            break;
        case MT_CHAR:
            return this->_chVal < other._chVal;
            break;
        case MT_BYTE:
            return this->_btVal < other._btVal;
            break;
        case MT_SHORT:
            return this->_shVal < other._shVal;
            break;
        case MT_USHORT:
            return this->_ushVal < other._ushVal;
            break;
        case MT_INT:
            return this->_iVal < other._iVal;
            break;
        case MT_UINT:
            return this->_uiVal < other._uiVal;
            break;
        case MT_LONG:
            return this->_lVal < other._lVal;
            break;
        case MT_ULONG:
            return this->_ulVal < other._ulVal;
            break;
        case MT_INT64:
            return this->_i64Val < other._i64Val;
            break;
        case MT_UINT64:
            return this->_ui64Val < other._ui64Val;
            break;
        case MT_FLOAT:
            return this->_fltVal < other._fltVal;
            break;
        case MT_DOUBLE:
            return this->_dblVal < other._dblVal;
            break;
        case MT_ANSI:
            return *this->_pStr < *other._pStr;
            break;
        case MT_UNICODE:
            return *this->_pWStr < *other._pWStr;
            break;
        case MT_BINARY:
            return BufferCompare( *this->_pBuf, *other._pBuf ) < 0;
            break;
        case MT_ARRAY:
            return ArrayLess( *this->_pArr, *other._pArr );
            break;
        case MT_COLLECTION:
            return CollectionLess( *this->_pColl, *other._pColl );
            break;
        }
    }
    else if ( this->_type > other._type )
    {
        switch ( this->_type )
        {
        case MT_NULL:
            return false;
            break;
        case MT_BOOLEAN:
            return this->_boolVal < (bool)other;
            break;
        case MT_CHAR:
            return this->_chVal < other.toChar();
            break;
        case MT_BYTE:
            return this->_btVal < (byte)other;
            break;
        case MT_SHORT:
            return this->_shVal < (short)other;
            break;
        case MT_USHORT:
            return this->_ushVal < (ushort)other;
            break;
        case MT_INT:
            return this->_iVal < (int)other;
            break;
        case MT_UINT:
            return this->_uiVal < (uint)other;
            break;
        case MT_LONG:
            return this->_lVal < (long)other;
            break;
        case MT_ULONG:
            return this->_ulVal < (ulong)other;
            break;
        case MT_INT64:
            return this->_i64Val < (int64)other;
            break;
        case MT_UINT64:
            return this->_ui64Val < (uint64)other;
            break;
        case MT_FLOAT:
            return this->_fltVal < (float)other;
            break;
        case MT_DOUBLE:
            return this->_dblVal < (double)other;
            break;
        case MT_ANSI:
            return *this->_pStr < other.operator AnsiString();
            break;
        case MT_UNICODE:
            return *this->_pWStr < other.operator UnicodeString();
            break;
        case MT_BINARY:
            return BufferCompare( *this->_pBuf, other.operator Buffer() ) < 0;
            break;
        case MT_ARRAY:
            return ArrayLess( *this->_pArr, other.operator MixedArray() );
            break;
        case MT_COLLECTION:
            return CollectionLess( *this->_pColl, other.operator Collection() );
            break;
        }
    }
    else // this->_type < other._type
    {
        switch ( other._type )
        {
        case MT_NULL:
            return false;
            break;
        case MT_BOOLEAN:
            return (bool)*this < other._boolVal;
            break;
        case MT_CHAR:
            return this->toChar() < other._chVal;
            break;
        case MT_BYTE:
            return (byte)*this < other._btVal;
            break;
        case MT_SHORT:
            return (short)*this < other._shVal;
            break;
        case MT_USHORT:
            return (ushort)*this < other._ushVal;
            break;
        case MT_INT:
            return (int)*this < other._iVal;
            break;
        case MT_UINT:
            return (uint)*this < other._uiVal;
            break;
        case MT_LONG:
            return (long)*this < other._lVal;
            break;
        case MT_ULONG:
            return (ulong)*this < other._ulVal;
            break;
        case MT_INT64:
            return (int64)*this < other._i64Val;
            break;
        case MT_UINT64:
            return (uint64)*this < other._ui64Val;
            break;
        case MT_FLOAT:
            return (float)*this < other._fltVal;
            break;
        case MT_DOUBLE:
            return (double)*this < other._dblVal;
            break;
        case MT_ANSI:
            return this->operator AnsiString() < *other._pStr;
            break;
        case MT_UNICODE:
            return this->operator UnicodeString() < *other._pWStr;
            break;
        case MT_BINARY:
            return BufferCompare( this->operator Buffer(), *other._pBuf ) < 0;
            break;
        case MT_ARRAY:
            return ArrayLess( this->operator MixedArray(), *other._pArr );
            break;
        case MT_COLLECTION:
            return CollectionLess( this->operator Collection(), *other._pColl );
            break;
        }
    }

    return false;
}

bool Mixed::operator > ( Mixed const & other ) const
{
    if ( this->_type == other._type )
    {
        switch ( this->_type )
        {
        case MT_NULL:
            return false;
            break;
        case MT_BOOLEAN:
            return this->_boolVal > other._boolVal;
            break;
        case MT_CHAR:
            return this->_chVal > other._chVal;
            break;
        case MT_BYTE:
            return this->_btVal > other._btVal;
            break;
        case MT_SHORT:
            return this->_shVal > other._shVal;
            break;
        case MT_USHORT:
            return this->_ushVal > other._ushVal;
            break;
        case MT_INT:
            return this->_iVal > other._iVal;
            break;
        case MT_UINT:
            return this->_uiVal > other._uiVal;
            break;
        case MT_LONG:
            return this->_lVal > other._lVal;
            break;
        case MT_ULONG:
            return this->_ulVal > other._ulVal;
            break;
        case MT_INT64:
            return this->_i64Val > other._i64Val;
            break;
        case MT_UINT64:
            return this->_ui64Val > other._ui64Val;
            break;
        case MT_FLOAT:
            return this->_fltVal > other._fltVal;
            break;
        case MT_DOUBLE:
            return this->_dblVal > other._dblVal;
            break;
        case MT_ANSI:
            return *this->_pStr > *other._pStr;
            break;
        case MT_UNICODE:
            return *this->_pWStr > *other._pWStr;
            break;
        case MT_BINARY:
            return BufferCompare( *this->_pBuf, *other._pBuf ) > 0;
            break;
        case MT_ARRAY:
            return ArrayGreater( *this->_pArr, *other._pArr );
            break;
        case MT_COLLECTION:
            return CollectionGreater( *this->_pColl, *other._pColl );
            break;
        }
    }
    else if ( this->_type > other._type )
    {
        switch ( this->_type )
        {
        case MT_NULL:
            return false;
            break;
        case MT_BOOLEAN:
            return this->_boolVal > (bool)other;
            break;
        case MT_CHAR:
            return this->_chVal > other.toChar();
            break;
        case MT_BYTE:
            return this->_btVal > (byte)other;
            break;
        case MT_SHORT:
            return this->_shVal > (short)other;
            break;
        case MT_USHORT:
            return this->_ushVal > (ushort)other;
            break;
        case MT_INT:
            return this->_iVal > (int)other;
            break;
        case MT_UINT:
            return this->_uiVal > (uint)other;
            break;
        case MT_LONG:
            return this->_lVal > (long)other;
            break;
        case MT_ULONG:
            return this->_ulVal > (ulong)other;
            break;
        case MT_INT64:
            return this->_i64Val > (int64)other;
            break;
        case MT_UINT64:
            return this->_ui64Val > (uint64)other;
            break;
        case MT_FLOAT:
            return this->_fltVal > (float)other;
            break;
        case MT_DOUBLE:
            return this->_dblVal > (double)other;
            break;
        case MT_ANSI:
            return *this->_pStr > other.operator AnsiString();
            break;
        case MT_UNICODE:
            return *this->_pWStr > other.operator UnicodeString();
            break;
        case MT_BINARY:
            return BufferCompare( *this->_pBuf, other.operator Buffer() ) > 0;
            break;
        case MT_ARRAY:
            return ArrayGreater( *this->_pArr, other.operator MixedArray() );
            break;
        case MT_COLLECTION:
            return CollectionGreater( *this->_pColl, other.operator Collection() );
            break;
        }
    }
    else // this->_type < other._type
    {
        switch ( other._type )
        {
        case MT_NULL:
            return false;
            break;
        case MT_BOOLEAN:
            return (bool)*this > other._boolVal;
            break;
        case MT_CHAR:
            return this->toChar() > other._chVal;
            break;
        case MT_BYTE:
            return (byte)*this > other._btVal;
            break;
        case MT_SHORT:
            return (short)*this > other._shVal;
            break;
        case MT_USHORT:
            return (ushort)*this > other._ushVal;
            break;
        case MT_INT:
            return (int)*this > other._iVal;
            break;
        case MT_UINT:
            return (uint)*this > other._uiVal;
            break;
        case MT_LONG:
            return (long)*this > other._lVal;
            break;
        case MT_ULONG:
            return (ulong)*this > other._ulVal;
            break;
        case MT_INT64:
            return (int64)*this > other._i64Val;
            break;
        case MT_UINT64:
            return (uint64)*this > other._ui64Val;
            break;
        case MT_FLOAT:
            return (float)*this > other._fltVal;
            break;
        case MT_DOUBLE:
            return (double)*this > other._dblVal;
            break;
        case MT_ANSI:
            return this->operator AnsiString() > *other._pStr;
            break;
        case MT_UNICODE:
            return this->operator UnicodeString() > *other._pWStr;
            break;
        case MT_BINARY:
            return BufferCompare( this->operator Buffer(), *other._pBuf ) > 0;
            break;
        case MT_ARRAY:
            return ArrayGreater( this->operator MixedArray(), *other._pArr );
            break;
        case MT_COLLECTION:
            return CollectionGreater( this->operator Collection(), *other._pColl );
            break;
        }
    }

    return false;
}

// create functions ---------------------------------------------------------------------------
Mixed & Mixed::createAnsi( AnsiString const & str )
{
    this->assign(str);
    return *this;
}

Mixed & Mixed::createAnsi( AnsiString && str )
{
    this->assign( std::move(str) );
    return *this;
}

Mixed & Mixed::createUnicode( UnicodeString const & str )
{
    this->assign(str);
    return *this;
}

Mixed & Mixed::createUnicode( UnicodeString && str )
{
    this->assign( std::move(str) );
    return *this;
}

Mixed & Mixed::createBuffer( size_t size )
{
    if ( this->_type == MT_BINARY )
    {
        this->_pBuf->alloc(size);
    }
    else
    {
        this->free();
        this->_type = MT_BINARY;
        this->_pBuf = new Buffer();
        if ( size > 0 ) this->_pBuf->alloc(size);
    }
    return *this;
}

Mixed & Mixed::createArray( size_t count )
{
    if ( this->_type == MT_ARRAY )
    {
        this->_pArr->resize(count);
    }
    else
    {
        this->free();
        this->_type = MT_ARRAY;
        if ( count > 0 )
        {
            this->_pArr = new MixedArray(count);
        }
        else
        {
            this->_pArr = new MixedArray();
        }
    }
    return  *this;
}

Mixed & Mixed::createCollection( bool caseInsensitive )
{
    if ( this->_type == MT_COLLECTION )
    {
        if ( this->_pColl->getCaseInsensitive() == caseInsensitive )
        {
            this->_pColl->clear();
        }
        else
        {
            this->_pColl->create(caseInsensitive);
        }
    }
    else
    {
        this->free();
        this->_type = MT_COLLECTION;
        this->_pColl = new Collection(caseInsensitive);
    }
    return *this;
}

// Buffer有关操作 ------------------------------------------------------------------------------
void Mixed::alloc( size_t size, bool setDataSize )
{
    if ( this->_type == MT_BINARY )
    {
        this->_pBuf->alloc( size, setDataSize );
    }
    else
    {
        this->free();
        this->_type = MT_BINARY;
        this->_pBuf = new Buffer();
        this->_pBuf->alloc( size, setDataSize );
    }
}

bool Mixed::peekCopy( bool copyCapacity )
{
    if ( this->_type == MT_BINARY && this->_pBuf )
    {
        return this->_pBuf->peekCopy(copyCapacity);
    }
    return false;
}

void * Mixed::getBuf() const
{
    if ( this->_type == MT_BINARY && this->_pBuf )
    {
        return this->_pBuf->getBuf();
    }
    return NULL;
}

// Array/Collection有关的操作 ------------------------------------------------------------------
Mixed & Mixed::operator [] ( Mixed const & k )
{
    switch ( this->_type )
    {
    case MT_ARRAY:
        {
            size_t i = k;
            if ( i >= this->_pArr->size() ) throw MixedError( MixedError::meOutOfArrayRange, FormatA( "Array out of bound: index:%d, size:%d", i, this->_pArr->size() ) );
            return this->_pArr->operator [] (i);
        }
        break;
    case MT_COLLECTION:
        {
            return this->_pColl->operator [] (k);
        }
        break;
    default:
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "(" + k.toAnsi() + ")" );
        break;
    }
}

Mixed const & Mixed::operator [] ( Mixed const & k ) const
{
    switch ( this->_type )
    {
    case MT_ARRAY:
        {
            size_t i = k;
            if ( i >= this->_pArr->size() ) throw MixedError( MixedError::meOutOfArrayRange, FormatA( "Array out of bound: index:%d, size:%d", i, this->_pArr->size() ) );
            return static_cast<MixedArray const *>(this->_pArr)->operator [] (i);
        }
        break;
    case MT_COLLECTION:
        {
            if ( !this->_pColl->has(k) ) throw MixedError( MixedError::meKeyNoExist, "Collection is not exist key:`" + k.toAnsi() + "`" );
            return static_cast<Collection const *>(this->_pColl)->at(k);
        }
        break;
    default:
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ +  + "(" + k.toAnsi() + ") const" );
        break;
    }
}

Mixed const & Mixed::get( Mixed const & k, Mixed const & defval ) const
{
    switch ( this->_type )
    {
    case MT_ARRAY:
        {
            size_t i = k;
            if ( i < this->_pArr->size() ) return static_cast<MixedArray const *>(this->_pArr)->operator [] (i);
        }
        break;
    case MT_COLLECTION:
        {
            if ( this->_pColl->has(k) ) return static_cast<Collection const *>(this->_pColl)->at(k);
        }
        break;
    }
    return defval;
}

MixedMixedPair & Mixed::getFirstPair()
{
    if ( this->isCollection() )
    {
        return this->_pColl->getFirstPair();
    }
    else
    {
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "()" );
    }
}

MixedMixedPair const & Mixed::getFirstPair() const
{
    if ( this->isCollection() )
    {
        return this->_pColl->getFirstPair();
    }
    else
    {
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "() const" );
    }
}

Mixed & Mixed::setNewFirstPair( Mixed const & k, Mixed const & v )
{
    if ( this->isCollection() )
    {
        this->_pColl->setNewFirstPair( k, v );
    }
    else
    {
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "()" );
    }
    return *this;
}

Mixed & Mixed::setNewPair( Mixed const & oldK, Mixed const & k, Mixed const & v )
{
    if ( this->isCollection() )
    {
        this->_pColl->setNewPair( oldK, k, v );
    }
    else
    {
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "()" );
    }
    return *this;
}

Mixed & Mixed::setNewPair( Mixed const & oldK, Mixed const & k, Mixed && v )
{
    if ( this->isCollection() )
    {
        this->_pColl->setNewPair( oldK, k, std::move(v) );
    }
    else
    {
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "()" );
    }
    return *this;
}

Mixed & Mixed::setPair( Mixed const & k, Mixed const & v )
{
    if ( this->isCollection() )
    {
        this->_pColl->setPair( k, v );
    }
    else
    {
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "()" );
    }
    return *this;
}

Mixed & Mixed::setPair( Mixed const & k, Mixed && v )
{
    if ( this->isCollection() )
    {
        this->_pColl->setPair( k, std::move(v) );
    }
    else
    {
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "()" );
    }
    return *this;
}

template < typename _Ty >
inline static Mixed::MixedArrayElement __MixedAdd( Mixed * thisMx, _Ty && v )
{
    size_t i = thisMx->_pArr->size();
    thisMx->_pArr->push_back( std::forward<_Ty>(v) );
    return Mixed::MixedArrayElement( i, thisMx->_pArr->at(i) );
}

Mixed::MixedArrayElement Mixed::add( Mixed const & v )
{
    if ( this->isArray() )
    {
        return __MixedAdd( this, v );
    }
    else
    {
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "()" );
    }
}

Mixed::MixedArrayElement Mixed::add( Mixed && v )
{
    if ( this->isArray() )
    {
        return __MixedAdd( this, std::move(v) );
    }
    else
    {
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "()" );
    }
}

Mixed::MixedArrayElement Mixed::addUnique( Mixed const & v )
{
    if ( this->isArray() )
    {
        size_t i;
        for ( i = 0; i < this->_pArr->size(); ++i )
        {
            if ( this->_pArr->at(i) == v )
            {
                return Mixed::MixedArrayElement( i, this->_pArr->at(i) );
            }
        }
        return __MixedAdd( this, v );
    }
    else
    {
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "()" );
    }
}

Mixed::MixedArrayElement Mixed::addUnique( Mixed && v )
{
    if ( this->isArray() )
    {
        size_t i;
        for ( i = 0; i < this->_pArr->size(); ++i )
        {
            if ( this->_pArr->at(i) == v )
            {
                return Mixed::MixedArrayElement( i, this->_pArr->at(i) );
            }
        }
        return __MixedAdd( this, std::move(v) );
    }
    else
    {
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "()" );
    }
}

bool Mixed::del( Mixed const & k )
{
    switch ( this->_type )
    {
    case MT_ARRAY:
        {
            size_t i = k;
            if ( i < this->_pArr->size() )
                this->_pArr->erase( this->_pArr->begin() + i );
            return true;
        }
        break;
    case MT_COLLECTION:
        return this->_pColl->del(k);
        break;
    }
    return false;
}

bool Mixed::has( Mixed const & ek ) const
{
    switch ( this->_type )
    {
    case MT_ARRAY:
        return std::find( this->_pArr->begin(), this->_pArr->end(), ek ) != this->_pArr->end();
        break;
    case MT_COLLECTION:
        return this->_pColl->has(ek);
        break;
    default:
        return false;
        break;
    }
}

Mixed & Mixed::merge( Mixed const & v )
{
    switch ( this->_type )
    {
    case MT_ARRAY:
        switch ( v._type )
        {
        case MT_ARRAY:
            for ( auto itVal = v._pArr->begin(); itVal != v._pArr->end(); ++itVal )
            {
                this->_pArr->push_back(*itVal);
            }
            break;
        case MT_COLLECTION:
            v._pColl->traverse( [this, &v] ( MixedMixedPair const & kv ) {
                Mixed pr;
                pr.setPair( v._pColl->getCaseInsensitive() )( kv.first, kv.second );
                this->_pArr->push_back( std::move(pr) );
                return true;
            } );
            break;
        default:
            this->_pArr->push_back(v);
            break;
        }
        break;
    case MT_COLLECTION:
        switch ( v._type )
        {
        case MT_ARRAY:
            for ( auto itVal = v._pArr->begin(); itVal != v._pArr->end(); ++itVal )
            {
                size_t inx = itVal - v._pArr->begin();
                this->_pColl->operator [] (inx) = *itVal;
            }
            break;
        case MT_COLLECTION:
            v._pColl->traverse( [this] ( MixedMixedPair const & pr ) {
                if ( this->_pColl->has(pr.first) ) // 如果存在此Key
                {
                    Mixed & thisMx = this->_pColl->operator [] (pr.first);
                    if ( thisMx.isContainer() ) // 如果是个容器，则继续调用merge()
                    {
                        thisMx.merge(pr.second);
                    }
                    else // 不是容器，则替换掉
                    {
                        thisMx = pr.second;
                    }
                }
                else // 不存在此Key
                {
                    this->_pColl->operator [] (pr.first) = pr.second;
                }
                return true;
            } );
            break;
        default:
            {
                size_t i = 0;
                while ( this->_pColl->has(i) ) i++;
                this->_pColl->operator [] (i) = v;
            }
            break;
        }
        break;
    default:
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "()" );
        break;
    }

    return *this;
}

Mixed & Mixed::reverse()
{
    switch ( this->_type )
    {
    case MT_NULL:
        break;
    case MT_BOOLEAN:
        break;
    case MT_CHAR:
        break;
    case MT_BYTE:
        break;
    case MT_SHORT:
        InvertByteOrder( &this->_shVal, sizeof(this->_shVal) );
        break;
    case MT_USHORT:
        InvertByteOrder( &this->_ushVal, sizeof(this->_ushVal) );
        break;
    case MT_INT:
        InvertByteOrder( &this->_iVal, sizeof(this->_iVal) );
        break;
    case MT_UINT:
        InvertByteOrder( &this->_uiVal, sizeof(this->_uiVal) );
        break;
    case MT_LONG:
        InvertByteOrder( &this->_lVal, sizeof(this->_lVal) );
        break;
    case MT_ULONG:
        InvertByteOrder( &this->_ulVal, sizeof(this->_ulVal) );
        break;
    case MT_INT64:
        InvertByteOrder( &this->_i64Val, sizeof(this->_i64Val) );
        break;
    case MT_UINT64:
        InvertByteOrder( &this->_ui64Val, sizeof(this->_ui64Val) );
        break;
    case MT_FLOAT:
        InvertByteOrder( &this->_fltVal, sizeof(this->_fltVal) );
        break;
    case MT_DOUBLE:
        InvertByteOrder( &this->_dblVal, sizeof(this->_dblVal) );
        break;
    case MT_ANSI:
        {
            ssize_t i = 0;
            ssize_t j = (ssize_t)this->_pStr->length() - 1;
            while ( i < j )
            {
                auto ch = (*this->_pStr)[i];
                (*this->_pStr)[i] = (*this->_pStr)[j];
                (*this->_pStr)[j] = ch;

                i++;
                j--;
            }
        }
        break;
    case MT_UNICODE:
        {
            ssize_t i = 0;
            ssize_t j = (ssize_t)this->_pWStr->length() - 1;
            while ( i < j )
            {
                auto ch = (*this->_pWStr)[i];
                (*this->_pWStr)[i] = (*this->_pWStr)[j];
                (*this->_pWStr)[j] = ch;

                i++;
                j--;
            }
        }
        break;
    case MT_BINARY:
        InvertByteOrder( this->_pBuf->getBuf(), this->_pBuf->getSize() );
        break;
    case MT_ARRAY:
        {
            offset_t j = (offset_t)this->_pArr->size() - 1;
            offset_t i = 0;
            while ( i < j )
            {
                Mixed t = std::move( this->_pArr->at(i) );
                this->_pArr->at(i) = std::move( this->_pArr->at(j) );
                this->_pArr->at(j) = std::move(t);

                i++;
                j--;
            }
        }
        break;
    case MT_COLLECTION:
        this->_pColl->reverse();
        break;
    default:
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "()" );
        break;
    }

    return *this;
}

bool Mixed::traverse( TraverseFn cb, bool inverted )
{
    if ( this->isArray() )
    {
        if ( inverted )
        {
            for ( auto it = this->_pArr->rbegin(); it != this->_pArr->rend(); it++ )
            {
                if ( !cb(*it) )
                {
                    return false;
                }
            }
        }
        else
        {
            for ( auto it = this->_pArr->begin(); it != this->_pArr->end(); it++ )
            {
                if ( !cb(*it) )
                {
                    return false;
                }
            }
        }
        return true;
    }
    else
    {
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "() for Array" );
    }
}

bool Mixed::traverse( ConstTraverseFn cb, bool inverted ) const
{
    if ( this->isArray() )
    {
        if ( inverted )
        {
            for ( auto it = this->_pArr->rbegin(); it != this->_pArr->rend(); it++ )
            {
                if ( !cb(*it) )
                {
                    return false;
                }
            }
        }
        else
        {
            for ( auto it = this->_pArr->begin(); it != this->_pArr->end(); it++ )
            {
                if ( !cb(*it) )
                {
                    return false;
                }
            }
        }
        return true;
    }
    else
    {
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "() const for Array" );
    }
}

bool Mixed::traverse( Collection::TraverseFn cb, Collection::TraverseOrder order, bool inverted )
{
    if ( this->isCollection() )
    {
        return this->_pColl->traverse( cb, order, inverted );
    }
    else
    {
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "() for Collection" );
    }
}

bool Mixed::traverse( Collection::ConstTraverseFn cb, Collection::TraverseOrder order, bool inverted ) const
{
    if ( this->isCollection() )
    {
        return static_cast<Collection const *>(this->_pColl)->traverse( cb, order, inverted );
    }
    else
    {
        throw MixedError( MixedError::meUnexpectedType, TypeStringA(*this) + " can't support " + __FUNCTION__ + "() const for Collection" );
    }
}

// Assignments --------------------------------------------------------------------------------
void Mixed::assign( bool boolVal )
{
    if ( this->_type == MT_BOOLEAN )
    {
        this->_boolVal = boolVal;
    }
    else
    {
        this->free();
        this->_type = MT_BOOLEAN;
        this->_boolVal = boolVal;
    }
}

void Mixed::assign( char chVal )
{
    if ( this->_type == MT_CHAR )
    {
        this->_chVal = chVal;
    }
    else
    {
        this->free();
        this->_type = MT_CHAR;
        this->_chVal = chVal;
    }
}

void Mixed::assign( byte btVal )
{
    if ( this->_type == MT_BYTE )
    {
        this->_btVal = btVal;
    }
    else
    {
        this->free();
        this->_type = MT_BYTE;
        this->_btVal = btVal;
    }
}

void Mixed::assign( short shVal )
{
    if ( this->_type == MT_SHORT )
    {
        this->_shVal = shVal;
    }
    else
    {
        this->free();
        this->_type = MT_SHORT;
        this->_shVal = shVal;
    }
}

void Mixed::assign( ushort ushVal )
{
    if ( this->_type == MT_USHORT )
    {
        this->_ushVal = ushVal;
    }
    else
    {
        this->free();
        this->_type = MT_USHORT;
        this->_ushVal = ushVal;
    }
}

void Mixed::assign( int iVal )
{
    if ( this->_type == MT_INT )
    {
        this->_iVal = iVal;
    }
    else
    {
        this->free();
        this->_type = MT_INT;
        this->_iVal = iVal;
    }
}

void Mixed::assign( uint uiVal )
{
    if ( this->_type == MT_UINT )
    {
        this->_uiVal = uiVal;
    }
    else
    {
        this->free();
        this->_type = MT_UINT;
        this->_uiVal = uiVal;
    }
}

void Mixed::assign( long lVal )
{
    if ( this->_type == MT_LONG )
    {
        this->_lVal = lVal;
    }
    else
    {
        this->free();
        this->_type = MT_LONG;
        this->_lVal = lVal;
    }
}

void Mixed::assign( ulong ulVal )
{
    if ( this->_type == MT_ULONG )
    {
        this->_ulVal = ulVal;
    }
    else
    {
        this->free();
        this->_type = MT_ULONG;
        this->_ulVal = ulVal;
    }
}

void Mixed::assign( float fltVal )
{
    if ( this->_type == MT_FLOAT )
    {
        this->_fltVal = fltVal;
    }
    else
    {
        this->free();
        this->_type = MT_FLOAT;
        this->_fltVal = fltVal;
    }
}

void Mixed::assign( int64 i64Val )
{
    if ( this->_type == MT_INT64 )
    {
        this->_i64Val = i64Val;
    }
    else
    {
        this->free();
        this->_type = MT_INT64;
        this->_i64Val = i64Val;
    }
}

void Mixed::assign( uint64 ui64Val )
{
    if ( this->_type == MT_UINT64 )
    {
        this->_ui64Val = ui64Val;
    }
    else
    {
        this->free();
        this->_type = MT_UINT64;
        this->_ui64Val = ui64Val;
    }
}

void Mixed::assign( double dblVal )
{
    if ( this->_type == MT_DOUBLE )
    {
        this->_dblVal = dblVal;
    }
    else
    {
        this->free();
        this->_type = MT_DOUBLE;
        this->_dblVal = dblVal;
    }
}

// 字符串赋值函数 -------------------------------------------------------------------------------
void Mixed::assign( AnsiString const & str )
{
    if ( this->_type == MT_ANSI )
    {
        *this->_pStr = str;
    }
    else
    {
        this->free();
        this->_type = MT_ANSI;
        this->_pStr = new AnsiString(str);
    }
}

void Mixed::assign( UnicodeString const & str )
{
    if ( this->_type == MT_UNICODE )
    {
        *this->_pWStr = str;
    }
    else
    {
        this->free();
        this->_type = MT_UNICODE;
        this->_pWStr = new UnicodeString(str);
    }
}

void Mixed::assign( AnsiString && str )
{
    if ( this->_type == MT_ANSI )
    {
        *this->_pStr = std::move(str);
    }
    else
    {
        this->free();
        this->_type = MT_ANSI;
        this->_pStr = new AnsiString( std::move(str) );
    }
}

void Mixed::assign( UnicodeString && str )
{
    if ( this->_type == MT_UNICODE )
    {
        *this->_pWStr = std::move(str);
    }
    else
    {
        this->free();
        this->_type = MT_UNICODE;
        this->_pWStr = new UnicodeString( std::move(str) );
    }
}

void Mixed::assign( char const * str, size_t len )
{
    if ( this->_type == MT_ANSI )
    {
        str = str ? str : "";
        if ( (ssize_t)len < 0 )
        {
            this->_pStr->assign(str);
        }
        else
        {
            this->_pStr->assign( str, len );
        }
    }
    else
    {
        this->free();
        this->_type = MT_ANSI; // set _type as AnsiString
        str = str ? str : "";
        if ( (ssize_t)len < 0 )
        {
            this->_pStr = new AnsiString(str);
        }
        else
        {
            this->_pStr = new AnsiString( str, len );
        }
    }
}

void Mixed::assign( wchar const * str, size_t len )
{
    if ( this->_type == MT_UNICODE )
    {
        str = str ? str : L"";
        if ( (ssize_t)len < 0 )
        {
            this->_pWStr->assign(str);
        }
        else
        {
            this->_pWStr->assign( str, len );
        }
    }
    else
    {
        this->free();
        this->_type = MT_UNICODE; // set _type as UnicodeString
        str = str ? str : L"";
        if ( (ssize_t)len < 0 )
        {
            this->_pWStr = new UnicodeString(str);
        }
        else
        {
            this->_pWStr = new UnicodeString( str, len );
        }
    }
}

// Buffer赋值函数 ------------------------------------------------------------------------------
void Mixed::assign( Buffer const & buf )
{
    if ( this->_type == MT_BINARY )
    {
        *this->_pBuf = buf;
    }
    else
    {
        this->free();
        this->_type = MT_BINARY;
        this->_pBuf = new Buffer(buf);
    }
}

void Mixed::assign( void const * binaryData, size_t size, bool isPeek )
{
    if ( this->_type == MT_BINARY )
    {
        this->_pBuf->setBuf( binaryData, size, isPeek );
    }
    else
    {
        this->free();
        this->_type = MT_BINARY;
        this->_pBuf = new Buffer( binaryData, size, isPeek );
    }
}

void Mixed::assign( Buffer && buf )
{
    if ( this->_type == MT_BINARY )
    {
        *this->_pBuf = std::move(buf);
    }
    else
    {
        this->free();
        this->_type = MT_BINARY;
        this->_pBuf = new Buffer( std::move(buf) );
    }
}

void Mixed::assign( GrowBuffer && buf )
{
    if ( this->_type == MT_BINARY )
    {
        *this->_pBuf = std::move(buf);
    }
    else
    {
        this->free();
        this->_type = MT_BINARY;
        this->_pBuf = new Buffer( std::move(buf) );
    }
}

// Array赋值函数 -------------------------------------------------------------------------------
void Mixed::assign( Mixed * arr, size_t count )
{
    if ( this->_type == MT_ARRAY )
    {
        this->_pArr->assign( arr, arr + count );
    }
    else
    {
        this->free();
        this->_type = MT_ARRAY;
        this->_pArr = new MixedArray( arr, arr + count );
    }
}

void Mixed::assign( std::initializer_list<Mixed> && list )
{
    if ( this->_type == MT_ARRAY )
    {
        this->_pArr->assign( std::move(list) );
    }
    else
    {
        this->free();
        this->_type = MT_ARRAY;
        this->_pArr = new MixedArray( std::move(list) );
    }
}

void Mixed::assign( $a && arr )
{
    if ( this->_type == MT_ARRAY )
    {
        this->_pArr->assign( std::move(arr._list) );
    }
    else
    {
        this->free();
        this->_type = MT_ARRAY;
        this->_pArr = new MixedArray( std::move(arr._list) );
    }
}

// Collection赋值函数 --------------------------------------------------------------------------
void Mixed::assign( $c && coll, bool caseInsensitive )
{
    if ( this->_type == MT_COLLECTION )
    {
        this->_pColl->assign( std::move(coll), caseInsensitive );
    }
    else
    {
        this->free();
        this->_type = MT_COLLECTION;
        this->_pColl = new Collection( std::move(coll), caseInsensitive );
    }
}

inline static void __AssignDifferentColl( Collection * pColl, bool isClear, Collection const & coll )
{
    if ( pColl->getCaseInsensitive() == coll.getCaseInsensitive() )
    {
        pColl->refPairList() = coll.refPairList();
        if ( pColl->getCaseInsensitive() )
        {
            pColl->refMapI() = coll.refMapI();
        }
        else
        {
            pColl->refMap() = coll.refMap();
        }
    }
    else
    {
        if ( isClear ) pColl->clear();
        coll.traverse( [pColl] ( MixedMixedPair const & pr ) {
            (*pColl)[pr.first] = pr.second;
            return true;
        } );
    }
}

void Mixed::assign( Collection const & coll, bool caseInsensitive )
{
    if ( this->_type == MT_COLLECTION && this->_pColl->_caseInsensitive == caseInsensitive )
    {
        __AssignDifferentColl( this->_pColl, true, coll );
    }
    else
    {
        this->free();
        this->_type = MT_COLLECTION;
        this->_pColl = new Collection(caseInsensitive);

        __AssignDifferentColl( this->_pColl, false, coll );
    }
}

// JSON ---------------------------------------------------------------------------------------
String Mixed::myJson( bool autoKeyQuotes, String const & spacer, String const & newline ) const
{
    return MixedToJsonEx( *this, autoKeyQuotes, spacer, newline );
}

String Mixed::json() const
{
    return MixedToJson( *this, false );
}

Mixed & Mixed::json( String const & jsonStr )
{
    ParseJson( jsonStr, this );
    return *this;
}

void Mixed::_copyConstruct( Mixed const & other )
{
    switch ( other._type )
    {
    case MT_ANSI:
        this->_type = other._type;
        this->_pStr = new AnsiString(*other._pStr);
        break;
    case MT_UNICODE:
        this->_type = other._type;
        this->_pWStr = new UnicodeString(*other._pWStr);
        break;
    case MT_BINARY:
        this->_type = other._type;
        this->_pBuf = new Buffer(*other._pBuf);
        break;
    case MT_ARRAY:
        this->_type = other._type;
        this->_pArr = new MixedArray(*other._pArr);
        break;
    case MT_COLLECTION:
        this->_type = other._type;
        this->_pColl = new Collection(*other._pColl);
        break;
    default: // 是其他的话，直接copy就好
        memcpy( this, &other, sizeof(Mixed) );
        break;
    }
}

void Mixed::_copyAssignment( Mixed const & other )
{
    if ( this->_type < MT_ANSI ) // this is POD type
    {
        if ( other._type < MT_ANSI ) // other is POD type
        {
            memcpy( this, &other, sizeof(Mixed) );
        }
        else // other isn't POD type
        {
            this->_copyConstruct(other);
        }
    }
    else // this isn't POD type
    {
        if ( other._type < MT_ANSI ) // other is POD type
        {
            this->free();
            memcpy( this, &other, sizeof(Mixed) );
        }
        else // other isn't POD type
        {
            if ( this->_type == other._type )
            {
                switch ( other._type )
                {
                case MT_ANSI:
                    *this->_pStr = *other._pStr;
                    break;
                case MT_UNICODE:
                    *this->_pWStr = *other._pWStr;
                    break;
                case MT_BINARY:
                    *this->_pBuf = *other._pBuf;
                    break;
                case MT_ARRAY:
                    *this->_pArr = *other._pArr;
                    break;
                case MT_COLLECTION:
                    *this->_pColl = *other._pColl;
                    break;
                }
            }
            else
            {
                this->free();
                this->_copyConstruct(other);
            }
        }
    }
}

// 预定义的Mixed常量 ----------------------------------------------------------------------------
Mixed const mxNull;

// ostream 相关
WINUX_FUNC_IMPL(std::ostream &) operator << ( std::ostream & o, Mixed const & m )
{
    switch ( m._type )
    {
    case Mixed::MT_BOOLEAN:
        o << m._boolVal;
        break;
    case Mixed::MT_CHAR:
        o << m._chVal;
        break;
    case Mixed::MT_BYTE:
        o << m._btVal;
        break;
    case Mixed::MT_SHORT:
        o << m._shVal;
        break;
    case Mixed::MT_USHORT:
        o << m._ushVal;
        break;
    case Mixed::MT_INT:
        o << m._iVal;
        break;
    case Mixed::MT_UINT:
        o << m._uiVal;
        break;
    case Mixed::MT_LONG:
        o << m._lVal;
        break;
    case Mixed::MT_ULONG:
        o << m._ulVal;
        break;
    case Mixed::MT_INT64:
        o << m._i64Val;
        break;
    case Mixed::MT_UINT64:
        o << m._ui64Val;
        break;
    case Mixed::MT_FLOAT:
        o << m._fltVal;
        break;
    case Mixed::MT_DOUBLE:
        o << m._dblVal;
        break;
    case Mixed::MT_ANSI:
        o << *m._pStr;
        break;
    case Mixed::MT_UNICODE:
        o << m.toAnsi();
        break;
    case Mixed::MT_BINARY:
        o << m._pBuf->toAnsi();
        break;
    case Mixed::MT_ARRAY:
        o << MixedToJsonA( m, false );
        break;
    case Mixed::MT_COLLECTION:
        o << MixedToJsonA( m, false );
        break;
    }
    return o;
}

WINUX_FUNC_IMPL(std::wostream &) operator << ( std::wostream & o, Mixed const & m )
{
    switch ( m._type )
    {
    case Mixed::MT_BOOLEAN:
        o << m._boolVal;
        break;
    case Mixed::MT_CHAR:
        o << m._chVal;
        break;
    case Mixed::MT_BYTE:
        o << m._btVal;
        break;
    case Mixed::MT_SHORT:
        o << m._shVal;
        break;
    case Mixed::MT_USHORT:
        o << m._ushVal;
        break;
    case Mixed::MT_INT:
        o << m._iVal;
        break;
    case Mixed::MT_UINT:
        o << m._uiVal;
        break;
    case Mixed::MT_LONG:
        o << m._lVal;
        break;
    case Mixed::MT_ULONG:
        o << m._ulVal;
        break;
    case Mixed::MT_INT64:
        o << m._i64Val;
        break;
    case Mixed::MT_UINT64:
        o << m._ui64Val;
        break;
    case Mixed::MT_FLOAT:
        o << m._fltVal;
        break;
    case Mixed::MT_DOUBLE:
        o << m._dblVal;
        break;
    case Mixed::MT_ANSI:
        o << m.toUnicode();
        break;
    case Mixed::MT_UNICODE:
        o << *m._pWStr;
        break;
    case Mixed::MT_BINARY:
        o << m._pBuf->toUnicode();
        break;
    case Mixed::MT_ARRAY:
        o << MixedToJsonW( m, false );
        break;
    case Mixed::MT_COLLECTION:
        o << MixedToJsonW( m, false );
        break;
    }
    return o;
}


} // namespace winux
