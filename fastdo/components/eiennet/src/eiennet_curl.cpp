#include "eiennet_base.hpp"
#include "eiennet_curl.hpp"
#include <curl/curl.h>
#include <iostream>
#include <string.h>

namespace eiennet
{
// class CUrlLib -------------------------------------------------------------------------
char const * CUrlLib::GetVersion()
{
    return curl_version();
}

CUrlLib::CUrlLib()
{
    curl_global_init(CURL_GLOBAL_ALL);
}

CUrlLib::~CUrlLib()
{
    curl_global_cleanup();
}

// class SList ---------------------------------------------------------------------------
SList::SList() : _slist(NULL)
{

}

SList::~SList()
{
    this->free();
}

void SList::free()
{
    if ( _slist )
    {
        curl_slist_free_all(_slist);
        _slist = NULL;
    }
}

void SList::append( char const * s )
{
    _slist = curl_slist_append( _slist, s );
}

void SList::append( winux::AnsiString const & str )
{
    _slist = curl_slist_append( _slist, str.c_str() );
}

// class PostMultipart --------------------------------------------------------------------
PostMultipart::PostMultipart() : _first(NULL), _last(NULL)
{

}

PostMultipart::~PostMultipart()
{
    this->free();
}

void PostMultipart::free()
{
    if ( _first )
    {
        curl_formfree(_first);
        _first = NULL;
        _last = NULL;
    }
}

void PostMultipart::addVar( winux::AnsiString const & name, winux::AnsiString const & value )
{
    curl_formadd( &_first, &_last, CURLFORM_COPYNAME, name.c_str(), CURLFORM_COPYCONTENTS, value.c_str(), CURLFORM_CONTENTSLENGTH, (long)value.length(), CURLFORM_END );
}

void PostMultipart::addFile( winux::AnsiString const & name, winux::AnsiString const & filename, winux::AnsiString const & showfilename )
{
    if ( showfilename.empty() )
    {
        curl_formadd( &_first, &_last, CURLFORM_COPYNAME, name.c_str(), CURLFORM_FILE, filename.c_str(), CURLFORM_END );
    }
    else
    {
        curl_formadd( &_first, &_last, CURLFORM_COPYNAME, name.c_str(), CURLFORM_FILE, filename.c_str(), CURLFORM_FILENAME, showfilename.c_str(), CURLFORM_END );
    }
}

void PostMultipart::addFile( winux::AnsiString const & name, char const * data, long size, winux::AnsiString const & showfilename )
{
    curl_formadd( &_first, &_last, CURLFORM_COPYNAME, name.c_str(), CURLFORM_BUFFER, showfilename.c_str(), CURLFORM_BUFFERPTR, data, CURLFORM_BUFFERLENGTH, size, CURLFORM_END );
}

// class CUrl -----------------------------------------------------------------------------
CUrl::CUrl( bool isInit ) : _curl(NULL), _errNo(0)
{
    if ( isInit ) this->init();
}

CUrl::CUrl( CUrl const & other ) : _curl(NULL), _errNo(0)
{
    this->operator = (other);
}

CUrl::~CUrl()
{
    this->cleanup();
}

CUrl & CUrl::operator = ( CUrl const & other )
{
    if ( this != &other )
    {
        this->cleanup();
        // 复制curl对象
        _curl = curl_easy_duphandle(other._curl);
        // 重新设置错误缓冲区
        _errBuf.resize(CURL_ERROR_SIZE);
        this->setErrorBuffer(&_errBuf[0]);
        // 重新设置'写'响应函数的自定义参数
        this->setWriteHandlerData(this);
        // 重新设置'读'响应函数的自定义参数
        this->setReadHandlerData(this);
        // 重新设置'头部'响应自定义参数
        this->setHeaderHandlerData(this);
        // 重新设置'进度'响应自定义参数
        this->setProgressHandlerData(this);
    }
    return *this;
}

void CUrl::init()
{
    this->cleanup();
    // 初始化新的curl对象
    _curl = curl_easy_init();

    // 设置错误缓冲区
    _errBuf.resize(CURL_ERROR_SIZE);
    this->setErrorBuffer(&_errBuf[0]);
    // 设置'写'响应函数及其自定义参数
    this->setWriteHandler(WriteCallback);
    this->setWriteHandlerData(this);
    // 设置'读'响应函数及其自定义参数
    this->setReadHandler(ReadCallback);
    this->setReadHandlerData(this);
    // 设置'头部'响应以及自定义参数
    this->setHeaderHandler(HeaderCallback);
    this->setHeaderHandlerData(this);
    // 设置'进度'响应以及自定义参数
    this->setProgressHandler(ProgressCallback);
    this->setProgressHandlerData(this);
}

void CUrl::cleanup()
{
    if ( _curl )
    {
        curl_easy_cleanup(_curl);
        _curl = NULL;
    }
    _errNo = 0;
    _errBuf.clear();
}

bool CUrl::perform()
{
    assert( _curl != NULL );
    return CURLE_OK == ( _errNo = curl_easy_perform(_curl) );
}

void CUrl::reset()
{
    assert( _curl != NULL );
    curl_easy_reset(_curl);
}

///////////////////////////////////////////////////////////////////////////////////////////
void CUrl::setHttpGet( bool b /*= true */ )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_HTTPGET, (long)b );
}

void CUrl::setHttpPost( bool b /*= true */ )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_POST, (long)b );
}

void CUrl::setUrl( winux::AnsiString const & url )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_URL, url.c_str() );
}

void CUrl::setTimeout( winux::ulong timeout )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_TIMEOUT, timeout );
}

void CUrl::setErrorBuffer( char * errBuf )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_ERRORBUFFER, errBuf );
}

void CUrl::setNoProgress( bool b )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_NOPROGRESS, (long)b );
}

void CUrl::setUsername( winux::AnsiString const & username )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_USERNAME, username.c_str() );
}

void CUrl::setPassword( winux::AnsiString const & password )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_PASSWORD, password.c_str() );
}

void CUrl::setPostFields( winux::AnsiString const & data )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_POSTFIELDS, data.c_str() );
}

void CUrl::setPostFieldSize( long size )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_POSTFIELDSIZE, size );
}

void CUrl::setPostMultipart( PostMultipart const & data )
{
    assert( _curl != NULL );
    curl_easy_setopt(_curl, CURLOPT_HTTPPOST, (struct curl_httppost *)data );
}

void CUrl::setHttpHeader( SList const & headers )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_HTTPHEADER, (struct curl_slist *)headers );
}

void CUrl::setVerbose( bool b )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_VERBOSE, (long)b );
}

void CUrl::setCookieJar( winux::AnsiString const & filename )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_COOKIEJAR, filename.c_str() );
}

void CUrl::setSslVerifyPeer( bool b )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_SSL_VERIFYPEER, (long)b );
}

void CUrl::setSslVerifyHost( bool b )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_SSL_VERIFYHOST, (long)( b ? 2L : 0L ) );
}

///////////////////////////////////////////////////////////////////////////////////////////
char const * CUrl::errNoStr() const
{
    return curl_easy_strerror( (CURLcode)_errNo );
}

char const * CUrl::error() const
{
    return _errBuf.c_str();
}

///////////////////////////////////////////////////////////////////////////////////////////
void CUrl::setWriteHandler( WriteFunction handler )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_WRITEFUNCTION, handler );
}

void CUrl::setWriteHandlerData( void * data )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_WRITEDATA, data );
}

void CUrl::setReadHandler( ReadFunction handler )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_READFUNCTION, handler );
}

void CUrl::setReadHandlerData( void * data )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_READDATA, data );
}


void CUrl::setHeaderHandler( HeaderFunction handler )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_HEADERFUNCTION, handler );
}

void CUrl::setHeaderHandlerData( void * data )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_HEADERDATA, data );
}

void CUrl::setProgressHandler( ProgressFunction handler )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_PROGRESSFUNCTION, handler );
}

void CUrl::setProgressHandlerData( void * data )
{
    assert( _curl != NULL );
    curl_easy_setopt( _curl, CURLOPT_PROGRESSDATA, data );
}

winux::AnsiString CUrl::getContentType() const
{
    winux::AnsiString r;
    char * pstr = nullptr;
    CURLcode rc = curl_easy_getinfo( _curl, CURLINFO_CONTENT_TYPE, &pstr );
    if ( rc == CURLE_OK && pstr != nullptr )
    {
        r = pstr;
    }
    return r;
}

///////////////////////////////////////////////////////////////////////////////////////////
size_t CUrl::WriteCallback( char * buf, size_t itemSize, size_t count, void * data )
{
    CUrl * target = reinterpret_cast<CUrl*>(data);
    if ( target )
    {
        return target->OnWrite( buf, itemSize, count );
    }
    return itemSize * count;
}

size_t CUrl::OnWrite( char * buf, size_t itemSize, size_t count )
{
    std::cout.write( buf, itemSize * count );
    return itemSize * count;
}

size_t CUrl::ReadCallback( char * buf, size_t itemSize, size_t count, void * data )
{
    CUrl * target = reinterpret_cast<CUrl*>(data);
    if ( target )
    {
        return target->OnRead( buf, itemSize, count );
    }
    return itemSize * count;
}

size_t CUrl::OnRead( char * buf, size_t itemSize, size_t count )
{
    return itemSize * count;
}

size_t CUrl::HeaderCallback( char * buf, size_t itemSize, size_t count, void * data )
{
    CUrl * target = reinterpret_cast<CUrl*>(data);
    if ( target )
    {
        return target->OnHeader( buf, itemSize, count );
    }
    return itemSize * count;
}

size_t CUrl::OnHeader( char * buf, size_t itemSize, size_t count )
{
    return itemSize * count;
}

int CUrl::ProgressCallback( void * data, double dltotal, double dlnow, double ultotal, double ulnow )
{
    CUrl * target = reinterpret_cast<CUrl*>(data);
    if ( target )
    {
        if ( dlnow > 0 )
            return target->OnDownloadProgress( dltotal, dlnow );
        else if ( ulnow > 0 )
            return target->OnUploadProgress( ultotal, ulnow );
    }
    return 0;
}

int CUrl::OnDownloadProgress( double dltotal, double dlnow )
{
    return 0;
}

int CUrl::OnUploadProgress( double ultotal, double ulnow )
{
    return 0;
}


} // namespace eiennet
