//
// winux的归档文件
//
#include "utilities.hpp"
#include "smartptr.hpp"
#include "strings.hpp"
#include "filesys.hpp"
#include "archives.hpp"

#include "eienexpr.hpp"
#include "eienexpr_errstrs.inl"

#if defined(OS_WIN) // IS_WINDOWS
    #include <mbstring.h>
    #include <tchar.h>
#else
    #include <wchar.h>
    #ifdef UNICODE
    #define _vsntprintf vswprintf
    #define _tcsstr wcsstr
    #define _tcstol wcstol
    #define _tcstoul wcstoul
    #else
    #define _vsntprintf vsnprintf
    #define _tcsstr strstr
    #define _tcstol strtol
    #define _tcstoul strtoul
    #endif
#endif

namespace winux
{
using namespace eienexpr;

#include "is_x_funcs.inl"

// 判断字符串是否为数字串。数字串只能包含数字、小数点、负号、加号、e
inline static bool IsNumberString( String const & str, bool * isPoint, bool * isExp, bool * isHex )
{
    if ( str.empty() ) return false;
    // 是否有小数点
    bool havePoint = false;
    bool haveExp = false;
    bool haveHex = false;
    // 数字位数
    int n = 0;
    for ( size_t i = 0; i < str.length(); ++i )
    {
        auto ch = str[i];
        if ( IsDigit(ch) || haveHex && IsHex(ch) )
        {
            n++;
        }
        else if ( ch == '-' || ch == '+' )
        {
            // 符号不出现在开始或者e后面，则不是数字
            if ( !( i == 0 || ( i > 0 && ( str[i - 1] | 0x20 ) == 'e' ) ) )
            {
                return false;
            }
        }
        else if ( ( ch | 0x20 ) == 'e' )
        {
            // e不能出现在非数字之后或者字符串末尾
            if ( n == 0 || i == str.length() - 1 ) return false;
            if ( haveExp ) return false;
            haveExp = true;
        }
        else if ( ch == '.' )
        {
            if ( havePoint ) return false;
            havePoint = true;
        }
        else if ( ( ch | 0x20 ) == 'x' )
        {
            // x必须在第二个位置且以0开头，否则不是数字
            if ( !( i == 1 && str[0] == '0' ) ) return false;
            // 若已标记为16进制串，则返回false。不可能有2个以上x
            if ( haveHex ) return false;
            haveHex = true;
        }
        else
        {
            return false;
        }
    }
    ASSIGN_PTR(isPoint) = havePoint;
    ASSIGN_PTR(isExp) = haveExp;
    ASSIGN_PTR(isHex) = haveHex;
    return n > 0 && n < 21; // 数字位大于0且小于21位才算数字串
}

// 判断是否为合法标识符
inline static bool IsIdentifierString( String const & idstr )
{
    if ( idstr.empty() ) return false;
    for ( auto ch : idstr )
    {
        if ( !( IsDigit(ch) || IsWord(ch) ) )
        {
            return false;
        }
    }
    return true;
}

// 判断一个值是否需要用引号包裹
template < typename _ChTy >
inline static bool IsNeedQuote( XString<_ChTy> const & valstr )
{
    // 值中不需要引号包裹可使用的标点符号
    thread_local tchar const dontNeedQuoteChars[] = $T("!#%&*+,-./:<=>?@\\^`|~");
    thread_local tchar const * dontNeedQuoteCharsBegin = dontNeedQuoteChars;
    thread_local tchar const * dontNeedQuoteCharsEnd = dontNeedQuoteChars + countof(dontNeedQuoteChars);

    if ( valstr.empty() ) return true;
    for ( auto ch : valstr )
    {
        if ( !( IsDigit(ch) || IsWord(ch) || std::find( dontNeedQuoteCharsBegin, dontNeedQuoteCharsEnd, (tchar)ch ) != dontNeedQuoteCharsEnd ) )
        {
            return true;
        }
    }
    return false;
}

// 将数字串转换为字符
inline static tchar NumberStringToChar( String const & number, int base )
{
    tchar * endptr;
    return (tchar)_tcstol( number.c_str(), &endptr, base );
}

// 将数字串转换为无符号长整数
inline static winux::ulong NumberStringToNumber( String const & number, int base )
{
    tchar * endptr;
    return (winux::ulong)_tcstoul( number.c_str(), &endptr, base );
}

// class Configure ----------------------------------------------------------------------------
String const Configure::ConfigVarsSlashChars = $T("\n\r\t\v\a\\\'\"$()");

Configure::Configure()
{

}

Configure::Configure( String const & configFile )
{
    this->load(configFile);
}

int Configure::_FindConfigRef( String const & str, int offset, int * length, String * name )
{
    String ldelim = $T("$(");
    String rdelim = $T(")");
    *length = 0;
    int pos1 = (int)str.find( ldelim, offset );
    if ( pos1 == -1 ) return -1;
    int pos2 = (int)str.find( rdelim, pos1 + ldelim.length() );
    if ( pos2 == -1 ) return -1;
    *length = pos2 + (int)rdelim.length() - pos1;
    *name = str.substr( pos1 + ldelim.length(), pos2 - pos1 - ldelim.length() );
    return pos1;
}

String Configure::_expandVarNoStripSlashes( String const & name, StringArray * chains ) const
{
    if ( !this->has(name) ) return $T("");
    chains->push_back(name);
    String res = _expandValNoStripSlashes( _rawParams.at(name), chains );
    chains->pop_back();
    return res;
}

String Configure::_expandValNoStripSlashes( String const & val, StringArray * chains ) const
{
    if ( val.empty() ) return $T("");
    String res;
    int len;
    String varName;
    int offset = 0;
    int pos;
    while ( ( pos = _FindConfigRef( val, offset, &len, &varName ) ) != -1 )
    {
        res += val.substr( offset, pos - offset );
        offset = pos + len;
        res += !ValueIsInArray( *chains, varName ) ? _expandVarNoStripSlashes( varName, chains ) : $T("");
    }
    res += val.substr(offset);
    return res;
}

inline static bool __GetLine( IFile * f, String * line )
{
    String tmpLine = f->getLine();
    *line = tmpLine.empty() ? Literal<String::value_type>::emptyStr : ( tmpLine[tmpLine.length() - 1] == Literal<String::value_type>::lfChar ? tmpLine.substr( 0, tmpLine.length() - 1 ) : tmpLine );
    return !tmpLine.empty();
}

int Configure::_load( String const & configFile, StringStringMap * rawParams, StringArray * loadFileChains )
{
    loadFileChains->push_back( RealPath(configFile) );
    int lineCount = 0;
    int varsCount = 0;
    try
    {
        File fin( configFile, $T("r") );
        if ( !fin ) return varsCount;
        String line;
        while ( __GetLine( &fin, &line ) )
        {
            lineCount++;
            String tmp = StrTrim(line);

            if ( tmp.empty() || tmp[0] == '#' ) // '#'行开头表示注释,只有在行开头才表示注释
                continue;

            if ( tmp[0] == '@' ) // '@'开头表示配置命令,目前仅支持 @import other-config-file
            {
                int pos = (int)tmp.find(' ');
                String commandName, commandParam;
                if ( pos == -1 )
                {
                    commandName = tmp.substr(1); // 偏移1是为了 skip '@'
                    commandParam = $T("");
                }
                else
                {
                    commandName = tmp.substr( 1, pos - 1 );
                    commandParam = tmp.substr( pos + 1 );
                }
                if ( commandName == $T("import") ) // 导入外部配置
                {
                    String dirPath = FilePath(configFile);
                    StringArray callChains;
                    // 展开值中引用的变量值
                    String confPath = StripSlashes( _expandValNoStripSlashes( commandParam, &callChains ), ConfigVarsSlashChars );
                    confPath = IsAbsPath(confPath) ? confPath : CombinePath( dirPath, confPath );
                    if ( !ValueIsInArray( *loadFileChains, RealPath(confPath), true ) )
                    {
                        varsCount += _load( confPath, rawParams, loadFileChains );
                    }
                }
                continue;
            }

            int delimPos = (int)line.find('=');
            if ( delimPos == -1 ) // 找不到'='分隔符,就把整行当成参数名
                (*rawParams)[line] = $T("");
            else
                (*rawParams)[ line.substr( 0, delimPos ) ] = line.substr( delimPos + 1 );

            varsCount++;
        }
    }
    catch ( std::exception & )
    {
    }
    //loadFileChains->pop_back(); //注释掉这句，则每一个文件只载入一次
    return varsCount;
}

int Configure::load( String const & configFile )
{
    _configFile = configFile;
    StringArray loadFileChains;
    return _load( configFile, &_rawParams, &loadFileChains );
}

String Configure::get( String const & name, bool stripslashes, bool expand ) const
{
    String value;

    if ( expand )
    {
        StringArray callChains; // 递归调用链，防止无穷递归
        value = _expandVarNoStripSlashes( name, &callChains );
    }
    else
    {
        value = this->has(name) ? _rawParams.at(name) : $T("");
    }

    if ( stripslashes )
        value = StripSlashes( value, ConfigVarsSlashChars );

    return value;
}

String Configure::operator [] ( String const & name ) const
{
    return this->has(name) ? StripSlashes( _rawParams.at(name), ConfigVarsSlashChars ) : $T("");
}

String Configure::operator () ( String const & name ) const
{
    StringArray callChains; // 递归调用链，防止无穷递归
    return StripSlashes( _expandVarNoStripSlashes( name, &callChains ), ConfigVarsSlashChars );
}

void Configure::setRaw( String const & name, String const & value )
{
    _rawParams[name] = value;
}

void Configure::set( String const & name, String const & value )
{
    _rawParams[name] = AddSlashes( value, ConfigVarsSlashChars );
}

bool Configure::del( String const & name )
{
    if ( this->has(name) )
    {
        _rawParams.erase(name);
        return true;
    }
    return false;
}

void Configure::clear()
{
    _rawParams.clear();
}

// additional operators -----------------------------------------------------------------------
// operator @ ---------------------------------------------------------------------------------
static bool __OprFile( Expression const * e, ExprOperand * arOperands[], short n, SimplePointer<ExprOperand> * outRetValue, void * data )
{
    outRetValue->attachNew(NULL);
    if ( n == 1 )
    {
        String settingsDir = data ? reinterpret_cast<EvalSettings *>(data)->getSettingsDir() : Literal<String::value_type>::emptyStr;
        String path, nl;
        size_t i = 0;
        // 只读取第一行作路径
        StrGetLine( &path, arOperands[0]->val().to<String>(), &i, &nl );
        // 基于配置文件目录计算路径
        path = IsAbsPath(path) ? path : CombinePath( settingsDir, path );

        ExprLiteral * ret = new ExprLiteral();
        ret->getValue().createString( FileGetString( path, feMultiByte ) );
        outRetValue->attachNew(ret);
        return true;
    }
    return false;
}

// additional functions -----------------------------------------------------------------------
// file( path[, textmode = true] ) path:文件路径，textmode:文本模式 默认true即默认文本模式
static bool __FuncFile( Expression * e, std::vector<Expression *> const & params, SimplePointer<ExprOperand> * outRetValue, void * data )
{
    outRetValue->attachNew(NULL);
    if ( params.size() > 0 )
    {
        String settingsDir = data ? reinterpret_cast<EvalSettings *>(data)->getSettingsDir() : Literal<String::value_type>::emptyStr;
        String path, nl;
        size_t i = 0;
        // 只读取第一行作路径
        StrGetLine( &path, params[0]->val().to<String>(), &i, &nl );
        // 基于配置文件目录计算路径
        path = IsAbsPath(path) ? path : CombinePath( settingsDir, path );
        // 文本模式
        bool textmode = params.size() > 1 ? params[1]->val().to<bool>() : true;

        ExprLiteral * ret = new ExprLiteral();
        if ( textmode )
            ret->getValue().createString( FileGetString( path, feMultiByte ) );
        else
            ret->getValue().assign( FileGetContentsEx( path, false ) );
        outRetValue->attachNew(ret);
        return true;
    }
    else
    {
        throw ExprError( ExprError::eeFuncParamCountError, EXPRERRSTR_NOT_ENOUGH_PARAMETERS("file") );
    }
    return false;
}

// struct EvalSettings_Data -------------------------------------------------------------------
struct EvalSettings_Data
{
    ExprPackage package; // 表达式语言包，包含自定义函数和算符
    VarContext rootCtx; // 根变量场景

    EvalSettings_Data()
    {
        // 注册自定义函数和算符
        this->package.setFunc( $T("file"), __FuncFile );
        this->package.addOpr( $T("@"), true, true, 1000, __OprFile );
    }

    void update( Expression * parent, Mixed & collVal, Mixed & collExpr, StringArray const & names, String const & updateExprStr, Mixed * * v )
    {
        if ( collVal.isCollection() ) // 集合，应该创建表达式变量场景对象
        {
            if ( names.size() > 0 )
            {
                VarContext ctx(&collVal);
                Expression exprObj( &this->package, &ctx, parent, ( parent ? parent->getDataPtr() : nullptr ) );
                StringArray newNames( names.begin() + 1, names.end() );
                this->update( &exprObj, collVal[ names[0] ], collExpr[ names[0] ], newNames, updateExprStr, v );
            }
            else
            {
                *v = &collVal;
            }
        }
        else if ( collVal.isArray() ) // 数组
        {
            if ( names.size() > 0 )
            {
                StringArray newNames( names.begin() + 1, names.end() );
                this->update( parent, collVal[ names[0] ], collExpr[ names[0] ], newNames, updateExprStr, v );
            }
            else
            {
                *v = &collVal;
            }
        }
        else // 非容器，应该通过表达式计算值
        {
            Expression expr( &this->package, nullptr, parent, ( parent ? parent->getDataPtr() : nullptr ) );
            if ( !updateExprStr.empty() )
            {
                collExpr = updateExprStr;
            }
            ExprParser().parse( &expr, collExpr );
            collVal = expr.val();
            *v = &collVal;
        }
    }
};

// --------------------------------------------------------------------------------------------
// 配置解析场景标记flag
enum EvalSettings_ParseContext
{
    cspcName,
    cspcValue,
    cspcString,
    cspcStrAntiSlashes,
    cspcExpr,
    cspcRootCollection,
    cspcCollection,
    cspcArray,
    cspcBinary,
};

static void EvalSettings_ParseCollection( std::vector<EvalSettings_ParseContext> & cspc, String const & str, int & i, Expression * exprObj, Mixed * collVal,  Mixed * collExpr );

// 解析反斜杠转义字符
static void EvalSettings_ParseStrAntiSlashes( std::vector<EvalSettings_ParseContext> & cspc, String const & str, int & i, String * v )
{
    String & result = *v;
    i++; // skip '\\'
    while ( i < (int)str.length() )
    {
        auto ch = str[i];
        if ( ch == 'a' )
        {
            result += '\a';
            i++;
            break;
        }
        else if ( ch == 'b' )
        {
            result += '\b';
            i++;
            break;
        }
        else if ( ch == 't' )
        {
            result += '\t';
            i++;
            break;
        }
        else if ( ch == 'n' )
        {
            result += '\n';
            i++;
            break;
        }
        else if ( ch == 'v' )
        {
            result += '\v';
            i++;
            break;
        }
        else if ( ch == 'f' )
        {
            result += '\f';
            i++;
            break;
        }
        else if ( ch == 'r' )
        {
            result += '\r';
            i++;
            break;
        }
        else if ( IsOct(ch) )
        {
            String octStr;
            for ( ; i < (int)str.length(); i++ )
            {
                ch = str[i];
                if ( IsOct(ch) && octStr.length() < 3 )
                {
                    octStr += ch;
                }
                else
                {
                    break;
                }
            }
            result += NumberStringToChar( octStr, 8 );
            break;
        }
        else if ( ( ch | 0x20 ) == 'x' ) // 解析\xHH
        {
            i++; // skip 'x'
            String hexStr;
            for ( ; i < (int)str.length(); i++ )
            {
                ch = str[i];
                if ( IsHex(ch) && hexStr.length() < 2 )
                {
                    hexStr += ch;
                }
                else
                {
                    break;
                }
            }
            result += NumberStringToChar( hexStr, 16 );
            break;
        }
        else // 其余加\的字符都照原样输出
        {
            result += ch;
            i++;
            break;
        }
    }
}

// 解析一个字符串'...' or "..."
static void EvalSettings_ParseString( std::vector<EvalSettings_ParseContext> & cspc, String const & str, int & i, String * v, String * vExpr )
{
    auto quote = str[i];
    v->clear();
    i++; // skip left quote
    while ( i < (int)str.length() )
    {
        auto ch = str[i];
        if ( ch == quote )
        {
            i++; // skip right quote
            break;
        }
        else if ( ch == '\\' )
        {
            cspc.push_back(cspcStrAntiSlashes);
            EvalSettings_ParseStrAntiSlashes( cspc, str, i, v );
            cspc.pop_back();
        }
        else
        {
            *v += ch;
            i++;
        }
    }
    IF_PTR(vExpr)->assign( quote + AddCSlashes(*v) + quote );
}

// 解析一个Key名称
static void EvalSettings_ParseName( std::vector<EvalSettings_ParseContext> & cspc, String const & str, int & i, String * name )
{
    int start = i;
    name->clear();
    while ( i < (int)str.length() )
    {
        auto ch = str[i];
        if ( IsSpace(ch) || ch == ';' || ch == '{' || ch == '}' || ch == '[' || ch == ']' )
        {
            break;
        }
        else if ( i == start && ( ch == '\'' || ch == '\"' ) )
        {
            cspc.push_back(cspcString);
            EvalSettings_ParseString( cspc, str, i, name, nullptr );
            cspc.pop_back();
            break;
        }
        else
        {
            *name += ch;
            i++;
        }
    }
}

// 解析一个表达式(...)
static void EvalSettings_ParseExpr( std::vector<EvalSettings_ParseContext> & cspc, String const & str, int & i, Expression * exprObj, Mixed * v, Mixed * vExpr )
{
    String vStr;
    i++; // skip '('
    int brackets = 1; // 括号配对。0时表示一个表达式结束
    while ( i < (int)str.length() )
    {
        auto ch = str[i];
        if ( ch == ';' )
        {
            break;
        }
        else if ( ch == '\'' || ch == '\"' )
        {
            String tmp;
            cspc.push_back(cspcString);
            EvalSettings_ParseString( cspc, str, i, &tmp, nullptr );
            cspc.pop_back();

            vStr += ch + AddCSlashes(tmp) + ch;
        }
        else if ( ch == '(' )
        {
            ++brackets;
            vStr += ch;
            i++;
        }
        else if ( ch == ')' )
        {
            if ( --brackets < 1 )
            {
                i++; // skip ')'
                break;
            }
            else
            {
                vStr += ch;
                i++;
            }
        }
        else
        {
            vStr += ch;
            i++;
        }
    }

    // 执行表达式
    try
    {
        exprObj->clear();
        ExprParser().parse( exprObj, vStr );
        *v = exprObj->val();
    }
    catch ( ExprError const & )
    {
    }

    vExpr->assign( $T("(") + exprObj->toString() + $T(")") );
}

// 解析二进制数据`...`
static void EvalSettings_ParseBinary( std::vector<EvalSettings_ParseContext> & cspc, String const & str, int & i, Mixed * v, Mixed * vExpr )
{
    thread_local String const prefix = String(Literal<String::value_type>::base64Str) + Literal<String::value_type>::colonStr;

    bool isBase64 = false;
    auto quote = str[i];

    i++; // skip left `

    if ( i + prefix.length() <= str.length() )
    {
        isBase64 = str.compare( i, prefix.length(), prefix ) == 0;
    }

    if ( isBase64 )
    {
        String vStr;
        i += (int)prefix.length(); // skip 'base64:'

        while ( i < (int)str.length() )
        {
            auto ch = str[i];
            if ( ch == quote )
            {
                i++; // skip right `
                break;
            }
            else
            {
                vStr += ch;
                i++;
            }
        }

        v->assign( Base64DecodeBuffer(vStr) );
        vExpr->assign( $T("`") + prefix + vStr + $T("`") );
    }
    else
    {
        String vch;
        GrowBuffer buf;

        while ( i < (int)str.length() )
        {
            auto ch = str[i];
            if ( ch == quote )
            {
                i++; // skip right `
                break;
            }
            else if ( IsSpace(ch) )
            {
                if ( !vch.empty() )
                {
                    buf.appendType<byte>( (byte)NumberStringToChar( vch, 16 ) );
                    vch.clear();
                }
                i++;
            }
            else
            {
                if ( vch.length() < 2 )
                {
                    vch += ch;
                    i++;
                }
                else
                {
                    buf.appendType<byte>( (byte)NumberStringToChar( vch, 16 ) );
                    vch.clear();
                }
            }
        }

        if ( !vch.empty() )
        {
            buf.appendType<byte>( (byte)NumberStringToChar( vch, 16 ) );
            vch.clear();
        }

        v->assign( std::move(buf) );
        vExpr->assign( $T("`") + BufferToHex( v->refBuffer() ) + $T("`") );
    }
}

// 存储一个Auto值
static void _StoreAutoValue( String const & oneValueStr, Mixed * arr, Mixed * arrExpr )
{
    if ( oneValueStr.empty() )
    {
        arr->add(mxNull);
        arrExpr->add( $T("null") );
    }
    else
    {
        bool isNumPoint = false, isNumExp = false, isNumHex = false;
        if ( IsNumberString( oneValueStr, &isNumPoint, &isNumExp, &isNumHex ) ) // 数字
        {
            Mixed & v = arr->add(mxNull).e;
            arrExpr->add(oneValueStr);

            if ( isNumPoint || isNumExp ) // 浮点数
            {
                double dbl = 0.0;
                ParseDouble( oneValueStr, &dbl );
                v.assign(dbl);
            }
            else // 整数
            {
                if ( isNumHex ? oneValueStr.length() < 11 : oneValueStr.length() < 10 ) // int32
                {
                    winux::uint ui = 0;
                    ParseUInt( oneValueStr, &ui );
                    if ( isNumHex )
                        v.assign(ui);
                    else
                        v.assign( (int)ui );
                }
                else // int64
                {
                    winux::uint64 ui64 = 0;
                    ParseUInt64( oneValueStr, &ui64 );
                    if ( isNumHex )
                        v.assign(ui64);
                    else
                        v.assign( (winux::int64)ui64 );
                }
            }
        }
        else if ( IsIdentifierString(oneValueStr) ) // 标识符
        {
            thread_local std::map< String, Mixed > constMap{
                { $T("false"), false },
                { $T("true"), true },
                { $T("null"), winux::mxNull },
            };
            if ( winux::IsSet( constMap, oneValueStr ) )
            {
                arr->add(constMap[oneValueStr]);
                arrExpr->add(oneValueStr);
            }
            else
            {
                arr->add(oneValueStr);
                arrExpr->add( IsNeedQuote(oneValueStr) ? $T("\"") + AddCSlashes(oneValueStr) + $T("\"") : oneValueStr );
            }
        }
        else // 非引号字符串
        {
            arr->add(oneValueStr);
            arrExpr->add( IsNeedQuote(oneValueStr) ? $T("\"") + AddCSlashes(oneValueStr) + $T("\"") : oneValueStr );
        }
    }
}

// 解析值并返回解析到的值个数
static size_t EvalSettings_ParseValue( std::vector<EvalSettings_ParseContext> & cspc, String const & str, int & i, Expression * exprObj, Mixed * value, Mixed * valExpr )
{
    Mixed arr; // 值数组，可能含有多个
    Mixed arrExpr; // 表达式数组，可能含有多个
    arr.createArray();
    arrExpr.createArray();

    String oneValueStr; // 单个值串
    bool hasOneValue = false;

    int start = i;

    while ( i < (int)str.length() )
    {
        auto ch = str[i];

        if ( ch == ';' ) // 遇到分号，说明值结束
        {
            if ( hasOneValue || !oneValueStr.empty() )
            {
                _StoreAutoValue( oneValueStr, &arr, &arrExpr );
                oneValueStr.clear();
                hasOneValue = false;
            }
            else if ( ( !hasOneValue || oneValueStr.empty() ) && arr._pArr->size() == 0 ) // 遇到了结束分号却依旧没有读取到值，就加个空Mixed作值
            {
                _StoreAutoValue( oneValueStr, &arr, &arrExpr );
                oneValueStr.clear();
                hasOneValue = false;
            }

            i++; // skip ';'
            break;
        }
        else if ( IsSpace(ch) ) // 遇到空白字符
        {
            if ( hasOneValue || !oneValueStr.empty() )
            {
                _StoreAutoValue( oneValueStr, &arr, &arrExpr );
                oneValueStr.clear();
                hasOneValue = false;
            }

            i++; // skip space-char
            start = i;
        }
        else if ( ch == '#' ) // 行注释
        {
            i++; // skip '#'
            while ( i < (int)str.length() && str[i] != '\n' ) i++; // 一直读取，直到'\n'
            if ( i < (int)str.length() ) i++; // skip '\n' if not end
        }
        else if ( i == start && ( ch == '\'' || ch == '\"' ) ) // 字符串
        {
            Mixed & tValue = arr.add(mxNull).e.createString<String::value_type>();        // 存值
            Mixed & tValExpr = arrExpr.add(mxNull).e.createString<String::value_type>();  // 存值表达式串

            cspc.push_back(cspcString);
            EvalSettings_ParseString( cspc, str, i, &tValue.ref<String>(), &tValExpr.ref<String>() );
            cspc.pop_back();

            oneValueStr.clear();
            hasOneValue = false;

            start = i;
        }
        else if ( i == start && ch == '(' ) // 表达式
        {
            Mixed & tValue = arr.add(mxNull).e;        // 存值
            Mixed & tValExpr = arrExpr.add(mxNull).e;  // 存值表达式串

            cspc.push_back(cspcExpr);
            EvalSettings_ParseExpr( cspc, str, i, exprObj, &tValue, &tValExpr );
            cspc.pop_back();

            oneValueStr.clear();
            hasOneValue = false;

            start = i;
        }
        else if ( i == start && ch == '`' ) // 二进制
        {
            Mixed & tValue = arr.add(mxNull).e;        // 存值
            Mixed & tValExpr = arrExpr.add(mxNull).e;  // 存值表达式串

            cspc.push_back(cspcBinary);
            EvalSettings_ParseBinary( cspc, str, i, &tValue, &tValExpr );
            cspc.pop_back();

            oneValueStr.clear();
            hasOneValue = false;

            start = i;
        }
        else if ( ch == '{' ) // 又进入一个Collection
        {
            i++; // skip '{'

            Mixed & tValue = arr.add(mxNull).e.createCollection();        // 存值
            Mixed & tValExpr = arrExpr.add(mxNull).e.createCollection();  // 存值表达式串

            VarContext varCtx(&tValue);
            Expression exprSubScope( exprObj->getPackage(), &varCtx, exprObj, nullptr );

            cspc.push_back(cspcCollection);
            EvalSettings_ParseCollection( cspc, str, i, &exprSubScope, &tValue, &tValExpr );
            cspc.pop_back();

            oneValueStr.clear();
            hasOneValue = false;

            // 如果是"}\n"，也说明值结束
            if ( i < (int)str.length() )
            {
                ch = str[i];
                if ( ch == '\n' || ch == '\r' )
                {
                    i++; // skip '\n' or '\r'
                    break;
                }
            }
        }
        else if ( ch == '}' )
        {
            break;
        }
        else if ( ch == '[' )
        {
            i++; // skip '['

            Mixed & tValue = arr.add(mxNull).e;        // 存值
            Mixed & tValExpr = arrExpr.add(mxNull).e;  // 存值表达式串
            Mixed tValue1;
            Mixed tValExpr1;

            cspc.push_back(cspcArray);
            size_t n = EvalSettings_ParseValue( cspc, str, i, exprObj, &tValue1, &tValExpr1 );
            cspc.pop_back();

            if ( tValExpr1.isArray() )
            {
                tValue = std::move(tValue1);
            }
            else
            {
                tValue.createArray();
                if ( n ) tValue.add( std::move(tValue1) );
            }

            if ( tValExpr1.isArray() )
            {
                tValExpr = std::move(tValExpr1);
            }
            else
            {
                tValExpr.createArray();
                if ( n ) tValExpr.add( std::move(tValExpr1) );
            }

            oneValueStr.clear();
            hasOneValue = false;
        }
        else if ( ch == ']' )
        {
            i++; // skip ']'
            break;
        }
        else
        {
            oneValueStr += ch;
            hasOneValue = true;
            i++;
        }
    }

    if ( hasOneValue || !oneValueStr.empty() )
    {
        _StoreAutoValue( oneValueStr, &arr, &arrExpr );
        oneValueStr.clear();
        hasOneValue = false;
    }

    size_t n = arr.getCount();

    // 值
    switch ( n )
    {
    case 0:
        //value->assign( $T("") );
        break;
    case 1:
        *value = std::move(arr[0]);
        break;
    default:
        *value = std::move(arr);
        break;
    }

    // 表达式
    switch ( arrExpr.getCount() )
    {
    case 0:
        valExpr->assign( $T("") );
        break;
    case 1:
        *valExpr = std::move(arrExpr[0]);
        break;
    default:
        *valExpr = std::move(arrExpr);
        break;
    }

    return n;
}

static void EvalSettings_ParseCollection( std::vector<EvalSettings_ParseContext> & cspc, String const & str, int & i, Expression * exprObj, Mixed * collVal,  Mixed * collExpr )
{
    String name;       // 存名字
    Mixed value;       // 存值
    Mixed valExpr;     // 存值表达式串
    bool currIsName = true;   // 当前是否解析名字
    bool hasName = false;     // 是否有名字
    bool hasValue = false;    // 是否有值

    while ( i < (int)str.length() )
    {
        auto ch = str[i];
        if ( IsSpace(ch) )
        {
            i++;
        }
        else if ( ch == '#' ) // 行注释
        {
            i++; // skip '#'
            while ( i < (int)str.length() && str[i] != '\n' ) i++; // 一直读取，直到'\n'
            if ( i < (int)str.length() ) i++; // skip '\n' if not end
        }
        else if ( ch == '}' )
        {
            i++; // skip '}'
            break;
        }
        else
        {
            if ( currIsName )
            {
                cspc.push_back(cspcName);
                EvalSettings_ParseName( cspc, str, i, &name );
                cspc.pop_back();

                hasName = true;
                currIsName = false;
            }
            else
            {
                cspc.push_back(cspcValue);
                EvalSettings_ParseValue( cspc, str, i, exprObj, &value, &valExpr );
                cspc.pop_back();

                hasValue = true;
                currIsName = true;

                if ( hasName && hasValue )
                {
                    exprObj->getVarContext()->set(name) = std::move(value);
                    (*collExpr)[name] = std::move(valExpr);

                    name.clear();
                    value.free();
                    valExpr.free();
                    currIsName = true;
                    hasName = hasValue = false;
                }
                else if ( hasName )
                {
                    exprObj->getVarContext()->set(name) = winux::mxNull;
                    (*collExpr)[name].assign( $T("null") );

                    name.clear();
                    value.free();
                    valExpr.free();
                    currIsName = true;
                    hasName = hasValue = false;
                }
            }
        }
    }

    if ( hasName && hasValue )
    {
        exprObj->getVarContext()->set(name) = std::move(value);
        (*collExpr)[name] = std::move(valExpr);

    }
    else if ( hasName )
    {
        exprObj->getVarContext()->set(name) = winux::mxNull;
        (*collExpr)[name].assign( $T("null") );
    }
}

// --------------------------------------------------------------------------------------------

template < typename _ChTy >
inline static XString<_ChTy> Impl_RecursiveEvalSettingsToStringEx(
    int level,
    Mixed const & parentCollVal,
    Mixed const & parentCollExpr,
    Mixed const & collVal,
    Mixed const & collExpr,
    XString<_ChTy> const & spacer,
    XString<_ChTy> const & newline
)
{
    XString<_ChTy> s;
    switch ( collVal._type )
    {
    case Mixed::MT_NULL:
        {
            auto & expr = collExpr.refString<_ChTy>();
            switch ( expr[0] )
            {
            case Literal<_ChTy>::quoteChar:
            case Literal<_ChTy>::aposChar:
            case Literal<_ChTy>::lBrkChar:
                s += expr;
                break;
            default:
                s += Literal<_ChTy>::nullValueStr;
                break;
            }
        }
        break;
    case Mixed::MT_BOOLEAN:
        {
            auto & expr = collExpr.refString<_ChTy>();
            switch ( expr[0] )
            {
            case Literal<_ChTy>::quoteChar:
            case Literal<_ChTy>::aposChar:
            case Literal<_ChTy>::lBrkChar:
                s += expr;
                break;
            default:
                s += collVal._boolVal ? Literal<_ChTy>::boolTrueStr : Literal<_ChTy>::boolFalseStr;
                break;
            }
        }
        break;
    case Mixed::MT_CHAR:
        {
            auto & expr = collExpr.refString<_ChTy>();
            switch ( expr[0] )
            {
            case Literal<_ChTy>::quoteChar:
            case Literal<_ChTy>::aposChar:
            case Literal<_ChTy>::lBrkChar:
                s += expr;
                break;
            default:
                s += Literal<_ChTy>::aposStr + XString<_ChTy>( 1, (_ChTy)collVal._chVal ) + Literal<_ChTy>::aposStr;
                break;
            }
        }
        break;
    case Mixed::MT_ANSI:
    case Mixed::MT_UNICODE:
        {
            auto & expr = collExpr.refString<_ChTy>();
            switch ( expr[0] )
            {
            case Literal<_ChTy>::quoteChar:
            case Literal<_ChTy>::aposChar:
            case Literal<_ChTy>::lBrkChar:
                s += expr;
                break;
            default:
                {
                    auto t = collVal.toString<_ChTy>();
                    s += IsNeedQuote(t) ? Literal<_ChTy>::quoteStr + AddCSlashes<_ChTy>(t) + Literal<_ChTy>::quoteStr : t;
                }
                break;
            }
        }
        break;
    case Mixed::MT_BINARY:
        {
            auto & expr = collExpr.refString<_ChTy>();
            switch ( expr[0] )
            {
            case Literal<_ChTy>::graveChar:
            case Literal<_ChTy>::quoteChar:
            case Literal<_ChTy>::aposChar:
            case Literal<_ChTy>::lBrkChar:
                s += expr;
                break;
            default:
                s += Literal<_ChTy>::graveStr + BufferToHex<_ChTy>(*collVal._pBuf) + Literal<_ChTy>::graveStr;
                break;
            }
        }
        break;
    case Mixed::MT_ARRAY:
        {
            if ( collExpr.isArray() )
            {
                if ( parentCollExpr.isArray() ) s += Literal<_ChTy>::lSBrkStr;
                for ( size_t i = 0; i < collExpr._pArr->size(); i++ )
                {
                    if ( i != 0 ) s += Literal<_ChTy>::spaceStr;
                    s += Impl_RecursiveEvalSettingsToStringEx( level + 1, collVal, collExpr, collVal[i], collExpr[i], spacer, newline );
                }
                if ( parentCollExpr.isArray() ) s += Literal<_ChTy>::rSBrkStr;
            }
            else
            {
                s += collExpr.refString<_ChTy>();
            }
        }
        break;
    case Mixed::MT_COLLECTION:
        {
            if ( collExpr.isCollection() )
            {
                if ( level || parentCollVal.isContainer() ) s += Literal<_ChTy>::lCBrkStr + newline;

                for ( auto & k : collExpr._pColl->refKeysArray() )
                {
                    // 如果不换行，就不用级别留白
                    s += newline.find('\n') == npos ? Literal<_ChTy>::spaceStr : StrMultiple( spacer, level );
                    // 键
                    auto tk = k.toString<_ChTy>();
                    s += IsNeedQuote(tk) ? Literal<_ChTy>::quoteStr + AddCSlashes<_ChTy>(tk) + Literal<_ChTy>::quoteStr : tk;
                    s += Literal<_ChTy>::spaceStr;
                    // 值的表达式
                    auto & expr = collExpr[k];
                    s += Impl_RecursiveEvalSettingsToStringEx( level + 1, collVal, collExpr, collVal[k], expr, spacer, newline );
                    // 如果值表达式是集合或是数组且最后一个元素是集合，且换行，那么不加分号
                    if ( ( expr.isCollection() || ( expr.getCount() > 0 && expr[expr.getCount() - 1].isCollection() ) ) && newline.find('\n') != npos )
                    {
                        s += newline;
                    }
                    else
                    {
                        s += Literal<_ChTy>::semicolonStr + newline;
                    }
                }

                if ( level || parentCollVal.isContainer() ) s += ( newline.find('\n') == npos ? Literal<_ChTy>::spaceStr : StrMultiple( spacer, level ? level - 1 : 0 ) ) + Literal<_ChTy>::rCBrkStr;
            }
            else
            {
                s += collExpr.refString<_ChTy>();
            }
        }
        break;
    default: // 其他整数类型
        {
            auto & expr = collExpr.refString<_ChTy>();
            switch ( expr[0] )
            {
            case Literal<_ChTy>::quoteChar:
            case Literal<_ChTy>::aposChar:
            case Literal<_ChTy>::lBrkChar:
                s += expr;
                break;
            default:
                s += collExpr.toString<_ChTy>();
                break;
            }
        }
        break;
    }
    return s;
}

template < typename _ChTy >
inline static XString<_ChTy> Impl_RecursiveEvalSettingsValueToStringEx(
    int level,
    Mixed const & parentVal,
    Mixed const & curVal,
    XString<_ChTy> const & spacer,
    XString<_ChTy> const & newline
)
{
    XString<_ChTy> s;
    switch ( curVal._type )
    {
    case Mixed::MT_NULL:
        s += Literal<_ChTy>::nullValueStr;
        break;
    case Mixed::MT_BOOLEAN:
        s += curVal._boolVal ? Literal<_ChTy>::boolTrueStr : Literal<_ChTy>::boolFalseStr;
        break;
    case Mixed::MT_CHAR:
        s += Literal<_ChTy>::aposStr + XString<_ChTy>( 1, (_ChTy)curVal._chVal ) + Literal<_ChTy>::aposStr;
        break;
    case Mixed::MT_ANSI:
    case Mixed::MT_UNICODE:
        {
            auto t = curVal.toString<_ChTy>();
            s += Literal<_ChTy>::quoteStr + AddCSlashes<_ChTy>(t) + Literal<_ChTy>::quoteStr;
        }
        break;
    case Mixed::MT_BINARY:
        s += Literal<_ChTy>::graveStr + BufferToHex<_ChTy>(*curVal._pBuf) + Literal<_ChTy>::graveStr;
        break;
    case Mixed::MT_ARRAY:
        {
            if ( parentVal.isArray() ) s += Literal<_ChTy>::lSBrkStr;
            for ( size_t i = 0; i < curVal._pArr->size(); i++ )
            {
                if ( i != 0 ) s += Literal<_ChTy>::spaceStr;
                s += Impl_RecursiveEvalSettingsValueToStringEx( level + 1, curVal, curVal[i], spacer, newline );
            }
            if ( parentVal.isArray() ) s += Literal<_ChTy>::rSBrkStr;
        }
        break;
    case Mixed::MT_COLLECTION:
        {
            if ( level || parentVal.isContainer() ) s += Literal<_ChTy>::lCBrkStr + newline;

            for ( auto & k : curVal._pColl->refKeysArray() )
            {
                // 如果不换行，就不用级别留白
                s += newline.find('\n') == npos ? Literal<_ChTy>::spaceStr : StrMultiple( spacer, level );
                // 键
                auto tk = k.toString<_ChTy>();
                s += IsNeedQuote(tk) ? Literal<_ChTy>::quoteStr + AddCSlashes<_ChTy>(tk) + Literal<_ChTy>::quoteStr : tk;
                s += Literal<_ChTy>::spaceStr;
                // 值
                auto & val = curVal[k];
                s += Impl_RecursiveEvalSettingsValueToStringEx( level + 1, curVal, val, spacer, newline );
                // 如果值是集合或是数组且最后一个元素是集合，且换行，那么不加分号
                if ( ( val.isCollection() || ( val.getCount() > 0 && val[val.getCount() - 1].isCollection() ) ) && newline.find('\n') != npos )
                {
                    s += newline;
                }
                else
                {
                    s += Literal<_ChTy>::semicolonStr + newline;
                }
            }

            if ( level || parentVal.isContainer() ) s += ( newline.find('\n') == npos ? Literal<_ChTy>::spaceStr : StrMultiple( spacer, level ? level - 1 : 0 ) ) + Literal<_ChTy>::rCBrkStr;
        }
        break;
    default: // 其他整数类型
        s += curVal.toString<_ChTy>();
        break;
    }
    return s;
}

// class EvalSettings -------------------------------------------------------------------------
String EvalSettings::ValToString( Mixed const & v, String const & spacer, String const & newline )
{
    return Impl_RecursiveEvalSettingsValueToStringEx<String::value_type>(
        0,
        mxNull,
        v,
        spacer,
        newline
    );
}

Mixed EvalSettings::Parse( String const & valStr )
{
    Mixed v, vExpr;
    std::vector<EvalSettings_ParseContext> cspc;
    int i = 0;
    EvalSettings cs;
    VarContext varCtx(&cs._collectionVal);
    Expression exprObj( &cs._self->package, &varCtx, nullptr, &cs );
    EvalSettings_ParseValue( cspc, valStr, i, &exprObj, &v, &vExpr );
    return v;
}

// Constructors
EvalSettings::EvalSettings( String const & settingsFile )
{
    this->_collectionVal.createCollection();
    this->_collectionExpr.createCollection();
    _self->rootCtx.setMixedCollection(&this->_collectionVal);

    if ( !settingsFile.empty() )
    {
        this->load(settingsFile);
    }
}

EvalSettings::EvalSettings( EvalSettings const & other )
{
    _self = other._self;
}

EvalSettings & EvalSettings::operator = ( EvalSettings const & other )
{
    if ( this != &other )
    {
        _self = other._self;
    }
    return *this;
}

EvalSettings::EvalSettings( EvalSettings && other ) noexcept : _self( std::move(other._self) )
{
}

EvalSettings & EvalSettings::operator = ( EvalSettings && other ) noexcept
{
    if ( this != &other )
    {
        _self = std::move(other._self);
    }
    return *this;
}

EvalSettings::~EvalSettings()
{
}

size_t EvalSettings::_load( String const & settingsFile, Mixed * collVal, Mixed * collExpr, StringArray * loadFileChains )
{
    String settingsFileRealPath = RealPath(settingsFile);
    loadFileChains->push_back(settingsFileRealPath);

    String settingsContent = FileGetString( settingsFileRealPath, feMultiByte );
    {
        std::vector<EvalSettings_ParseContext> cspc;

        if ( !collVal->isCollection() ) collVal->createCollection();
        if ( !collExpr->isCollection() ) collExpr->createCollection();

        VarContext varCtx(collVal);
        Expression exprObj( &_self->package, &varCtx, nullptr, this );

        int i = 0;
        cspc.push_back(cspcRootCollection);
        EvalSettings_ParseCollection( cspc, settingsContent, i, &exprObj, collVal, collExpr );
        cspc.pop_back();
    }

    //loadFileChains->pop_back(); // 注释掉这句，则每一个文件只载入一次

    _self->rootCtx.setMixedCollection(&this->_collectionVal); // 每次载入完设置后重新设置Root变量场景

    return this->_collectionVal.getCount();
}

size_t EvalSettings::load( String const & settingsFile )
{
    this->_settingsFile = settingsFile;
    StringArray loadFileChains;
    return this->_load( settingsFile, &this->_collectionVal, &this->_collectionExpr, &loadFileChains );
}

Mixed & EvalSettings::update( String const & multiname, String const & updateExprStr )
{
    StringArray names;

    // 这里并不是计算，而是解析层次，记下每层的名字
    Expression te( &_self->package, nullptr, nullptr, nullptr );
    ExprParser().parse( &te, multiname );

    for ( ExprAtom * atom : te._suffixAtoms )
    {
        if ( atom->getAtomType() == ExprAtom::eatOperand )
        {
            ExprOperand * opd = (ExprOperand *)atom;

            if ( opd->getOperandType() == ExprOperand::eotLiteral )
            {
                names.push_back( ((ExprLiteral *)opd)->getValue() );
            }
            else
            {
                names.push_back( atom->toString() );
            }
        }
    }

    Mixed * v = nullptr;
    _self->update( nullptr, this->_collectionVal, this->_collectionExpr, names, updateExprStr, &v );
    return *v;
}

Mixed & EvalSettings::execRef( String const & exprStr ) const
{
    thread_local Mixed inner;
    try
    {
        Expression expr( const_cast<ExprPackage *>(&_self->package), const_cast<VarContext *>(&_self->rootCtx), nullptr, nullptr );
        ExprParser().parse( &expr, exprStr );
        Mixed * v = nullptr;
        if ( expr.evaluateMixedPtr(&v) )
        {
            return *v;
        }
    }
    catch ( ExprError const & /*e*/ )
    {
    }
    inner.free();
    return inner;
}

Mixed EvalSettings::execVal( String const & exprStr, Mixed const & defval ) const
{
    try
    {
        Expression expr( const_cast<ExprPackage *>(&_self->package), const_cast<VarContext *>(&_self->rootCtx), nullptr, nullptr );
        ExprParser().parse( &expr, exprStr );
        return expr.val();
    }
    catch ( ExprError const & /*e*/ )
    {
    }
    return defval;
}

Mixed const & EvalSettings::operator [] ( String const & name ) const
{
    return this->get(name);
}

Mixed & EvalSettings::operator [] ( String const & name )
{
    return this->_collectionVal[name];
}

bool EvalSettings::has( String const & name ) const
{
    return this->_collectionVal.has(name);
}

Mixed const & EvalSettings::get( String const & name ) const
{
    thread_local Mixed const inner;
    return this->_collectionVal.has(name) ? this->_collectionVal[name] : inner;
}

EvalSettings & EvalSettings::set( String const & name, Mixed const & value )
{
    this->_collectionVal[name] = value;
    return *this;
}

Mixed const & EvalSettings::val() const
{
    return this->_collectionVal;
}

Mixed & EvalSettings::val()
{
    return this->_collectionVal;
}

Mixed const & EvalSettings::expr() const
{
    return this->_collectionExpr;
}

Mixed & EvalSettings::expr()
{
    return this->_collectionExpr;
}

String EvalSettings::getSettingsDir() const
{
    return FilePath(this->_settingsFile);
}

String EvalSettings::toString( String const & spacer, String const & newline ) const
{
    return Impl_RecursiveEvalSettingsToStringEx<String::value_type>(
        0,
        mxNull,
        mxNull,
        this->_collectionVal,
        this->_collectionExpr,
        spacer,
        newline
    );
}

String EvalSettings::valToString( String const & spacer, String const & newline ) const
{
    return Impl_RecursiveEvalSettingsValueToStringEx<String::value_type>(
        0,
        mxNull,
        this->_collectionVal,
        spacer,
        newline
    );
}


// class CsvWriter ----------------------------------------------------------------------------
inline static String __JudgeAddQuotes( String const & valStr )
{
    String::const_iterator it = valStr.begin();
    for ( ; it != valStr.end(); ++it )
    {
        if ( *it == ',' || *it == '\"' ||  *it == '\'' || *it == '\n' )
        {
            break;
        }
    }
    if ( it != valStr.end() )
    {
        return $T("\"") + AddQuotes( valStr, Literal<String::value_type>::quoteChar ) + $T("\"");
    }
    else
    {
        return valStr;
    }
}

CsvWriter::CsvWriter( IFile * outputFile ) : _outputFile(outputFile)
{
}

void CsvWriter::write( Mixed const & records, Mixed const & columnHeaders )
{
    if ( columnHeaders.isArray() )
    {
        this->writeRecord(columnHeaders);
    }
    if ( records.isArray() ) // 多条记录
    {
        size_t i, n = records.getCount();
        for ( i = 0; i < n; i++ )
        {
            this->writeRecord(records[i]);
        }
    }
    else // 只有一条记录
    {
        this->writeRecord(records);
    }
}

void CsvWriter::writeRecord( Mixed const & record )
{
    if ( record.isArray() ) // 多个列
    {
        size_t i, n = record.getCount();
        String strRecord;
        for ( i = 0; i < n; i++ )
        {
            if ( i != 0 ) strRecord += $T(",");
            strRecord += __JudgeAddQuotes(record[i]);
        }
        _outputFile->puts( strRecord + $T("\n") );
    }
    else if ( record.isCollection() )
    {
        size_t i, n = record.getCount();
        String strRecord;
        for ( i = 0; i < n; i++ )
        {
            if ( i != 0 ) strRecord += $T(",");
            strRecord += __JudgeAddQuotes( record.getPair(i).second );
        }
        _outputFile->puts( strRecord + $T("\n") );
    }
    else // 只有1列
    {
        _outputFile->puts( __JudgeAddQuotes(record) + $T("\n") );
    }
}

// class CsvReader ----------------------------------------------------------------------------
CsvReader::CsvReader( IFile * inputFile, bool hasColumnHeaders )
{
    if ( inputFile ) this->read( inputFile->entire(), hasColumnHeaders );
}

CsvReader::CsvReader( String const & content, bool hasColumnHeaders )
{
    if ( !content.empty() ) this->read( content, hasColumnHeaders );
}

inline static void _ReadString( String const & str, size_t * pI, String * valStr )
{
    size_t & i = *pI;
    ++i; // skip '\"'
    while ( i < str.length() )
    {
        String::value_type ch = str[i];
        if ( ch == '\"' )
        {
            if ( i + 1 < str.length() && str[i + 1] == '\"' ) // double '\"' 解析成一个'\"'
            {
                *valStr += '\"';
                i++; // skip "
                i++; // skip "
            }
            else
            {
                i++; // skip 作为字符串结束的尾"
                break;
            }
        }
        else
        {
            *valStr += ch;
            i++;
        }
    }
}

inline static void _ReadRecord( String const & str, size_t * pI, Mixed * record )
{
    size_t & i = *pI;
    record->createArray();
    String valStr;
    while ( i < str.length() )
    {
        String::value_type ch = str[i];
        if ( ch == '\n' ) // 结束一条记录
        {
            record->add(valStr);
            i++; // skip '\n'
            break;
        }
        else if ( ch == ',' ) // 结束一个值
        {
            record->add(valStr);
            valStr.clear();
            i++; // skip ','
        }
        else if ( ch == '\"' )
        {
            // 去除之前可能的空白
            if ( StrTrim(valStr).empty() )
            {
                valStr.clear(); // 去除之前可能获得的空白字符
                _ReadString( str, &i, &valStr );
                //record.add(valStr);
                //valStr.clear();
                // 跳过','以避免再次加入这个值,或者跳过换行符
                //while ( i < str.length() && str[i] != ',' && str[i] != '\n' ) i++;
            }
            else
            {
                valStr += ch;
                i++;
            }
        }
        else
        {
            valStr += ch;
            i++;
        }
    }

    if ( str.length() > 0 && '\n' != str[i - 1] ) // 最后一个字符不是换行符
    {
        record->add(valStr);
    }
}

void CsvReader::read( String const & content, bool hasColumnHeaders )
{
    size_t i = 0;
    if ( hasColumnHeaders )
    {
        Mixed hdrs;
        _ReadRecord( content, &i, &hdrs );
        _columns.createCollection();
        for ( size_t i = 0, n = hdrs.getCount(); i < n; ++i )
        {
            _columns[ hdrs[i] ] = i;
        }
    }

    _records.createArray();
    while ( i < content.length() )
    {
        Mixed & record = _records.add(mxNull).e;
        _ReadRecord( content, &i, &record );
    }
}


// class TextArchive --------------------------------------------------------------------------
void TextArchive::saveEx( Buffer const & content, AnsiString const & encoding, GrowBuffer * output, FileEncoding fileEncoding )
{
    switch ( fileEncoding )
    {
    case feMultiByte:
        {
            Conv conv( encoding, this->_mbsEncoding );
            char * buf;
            size_t n = conv.convert( content.getBuf<char>(), content.getSize(), &buf );
            output->appendString( NewlineToFile( buf, n, false ) );
            Buffer::Free(buf);
        }
        break;
    case feUtf8:
        {
            Conv conv( encoding, "UTF-8" );
            char * buf;
            size_t n = conv.convert( content.getBuf<char>(), content.getSize(), &buf );
            output->appendString( NewlineToFile( buf, n, false ) );
            Buffer::Free(buf);
        }
        break;
    case feUtf8Bom:
        {
            Conv conv( encoding, "UTF-8" );
            char * buf;
            size_t n = conv.convert( content.getBuf<char>(), content.getSize(), &buf );
            // write BOM
            output->appendType( { '\xef', '\xbb', '\xbf' } );
            output->appendString( NewlineToFile( buf, n, false ) );
            Buffer::Free(buf);
        }
        break;
    case feUtf16Le:
        {
            Conv conv( encoding, "UTF-16LE" );
            char * buf;
            size_t n = conv.convert( content.getBuf<char>(), content.getSize(), &buf );
            // write BOM
            output->appendType( { '\xff', '\xfe' } );
            Utf16String res2 = NewlineToFile( (Utf16String::value_type *)buf, n / sizeof(Utf16String::value_type), IsBigEndian() );
            Buffer::Free(buf);
            output->appendString(res2);
        }
        break;
    case feUtf16Be:
        {
            Conv conv( encoding, "UTF-16BE" );
            char * buf;
            size_t n = conv.convert( content.getBuf<char>(), content.getSize(), &buf );
            // write BOM
            output->appendType( { '\xfe', '\xff' } );
            Utf16String res2 = NewlineToFile( (Utf16String::value_type *)buf, n / sizeof(Utf16String::value_type), IsLittleEndian() );
            Buffer::Free(buf);
            output->appendString(res2);
        }
        break;
    case feUtf32Le:
        {
            Conv conv( encoding, "UTF-32LE" );
            char * buf;
            size_t n = conv.convert( content.getBuf<char>(), content.getSize(), &buf );
            // write BOM
            output->appendType( { '\xff', '\xfe', '\0', '\0' } );
            Utf32String res2 = NewlineToFile( (Utf32String::value_type *)buf, n / sizeof(Utf32String::value_type), IsBigEndian() );
            Buffer::Free(buf);
            output->appendString(res2);
        }
        break;
    case feUtf32Be:
        {
            Conv conv( encoding, "UTF-32BE" );
            char * buf;
            size_t n = conv.convert( content.getBuf<char>(), content.getSize(), &buf );
            // write BOM
            output->appendType( { '\0', '\0', '\xfe', '\xff' } );
            Utf32String res2 = NewlineToFile( (Utf32String::value_type *)buf, n / sizeof(Utf32String::value_type), IsLittleEndian() );
            Buffer::Free(buf);
            output->appendString(res2);
        }
        break;
    }
}

void TextArchive::_processContent( Buffer const & content, bool isConvert, AnsiString const & encoding )
{
    switch ( _fileEncoding )
    {
    case feMultiByte:
        {
            XString<char> strContent = NewlineFromFile( content.get<char>(), content.size(), false );
            if ( isConvert )
            {
                Conv conv( this->_mbsEncoding, encoding );
                char * buf;
                size_t n = conv.convert( (char const *)strContent.c_str(), strContent.length() * sizeof(char), &buf );
                this->_pureContent.setBuf( buf, n, false );
                Buffer::Free(buf);
                this->setContentEncoding(encoding);
            }
            else
            {
                this->_pureContent.setBuf( (void *)strContent.c_str(), strContent.length() * sizeof(char), false );
                this->setContentEncoding(this->_mbsEncoding);
            }
        }
        break;
    case feUtf8:
        {
            XString<char> strContent = NewlineFromFile( content.get<char>(), content.size(), false );
            if ( isConvert )
            {
                Conv conv( "UTF-8", encoding );
                char * buf;
                size_t n = conv.convert( (char const *)strContent.c_str(), strContent.length() * sizeof(char), &buf );
                this->_pureContent.setBuf( buf, n, false );
                Buffer::Free(buf);
                this->setContentEncoding(encoding);
            }
            else
            {
                this->_pureContent.setBuf( (void *)strContent.c_str(), strContent.length() * sizeof(char), false );
                this->setContentEncoding("UTF-8");
            }
        }
        break;
    case feUtf8Bom:
        {
            XString<char> strContent = NewlineFromFile( content.get<char>(), content.size(), false );
            if ( isConvert )
            {
                Conv conv( "UTF-8", encoding );
                char * buf;
                size_t n = conv.convert( (char const *)strContent.c_str(), strContent.length() * sizeof(char), &buf );
                this->_pureContent.setBuf( buf, n, false );
                Buffer::Free(buf);
                this->setContentEncoding(encoding);
            }
            else
            {
                this->_pureContent.setBuf( (void *)strContent.c_str(), strContent.length() * sizeof(char), false );
                this->setContentEncoding("UTF-8");
            }
        }
        break;
    case feUtf16Le:
        {
            XString<char16> strContent = NewlineFromFile( content.get<char16>(), content.size() / sizeof(char16), IsBigEndian() );
            if ( isConvert )
            {
                Conv conv( "UTF-16LE", encoding );
                char * buf;
                size_t n = conv.convert( (char const *)strContent.c_str(), strContent.length() * sizeof(char16), &buf );
                this->_pureContent.setBuf( buf, n, false );
                Buffer::Free(buf);
                this->setContentEncoding(encoding);
            }
            else
            {
                this->_pureContent.setBuf( (void *)strContent.c_str(), strContent.length() * sizeof(char16), false );
                this->setContentEncoding("UTF-16LE");
            }
        }
        break;
    case feUtf16Be:
        {
            XString<char16> strContent = NewlineFromFile( content.get<char16>(), content.size() / sizeof(char16), IsLittleEndian() );
            if ( isConvert )
            {
                Conv conv( "UTF-16BE", encoding );
                char * buf;
                size_t n = conv.convert( (char const *)strContent.c_str(), strContent.length() * sizeof(char16), &buf );
                this->_pureContent.setBuf( buf, n, false );
                Buffer::Free(buf);
                this->setContentEncoding(encoding);
            }
            else
            {
                this->_pureContent.setBuf( (void *)strContent.c_str(), strContent.length() * sizeof(char16), false );
                this->setContentEncoding("UTF-16BE");
            }
        }
        break;
    case feUtf32Le:
        {
            XString<char32> strContent = NewlineFromFile( content.get<char32>(), content.size() / sizeof(char32), IsBigEndian() );
            if ( isConvert )
            {
                Conv conv( "UTF-32LE", encoding );
                char * buf;
                size_t n = conv.convert( (char const *)strContent.c_str(), strContent.length() * sizeof(char32), &buf );
                this->_pureContent.setBuf( buf, n, false );
                Buffer::Free(buf);
                this->setContentEncoding(encoding);
            }
            else
            {
                this->_pureContent.setBuf( (void *)strContent.c_str(), strContent.length() * sizeof(char32), false );
                this->setContentEncoding("UTF-32LE");
            }
        }
        break;
    case feUtf32Be:
        {
            XString<char32> strContent = NewlineFromFile( content.get<char32>(), content.size() / sizeof(char32), IsLittleEndian() );
            if ( isConvert )
            {
                Conv conv( "UTF-32BE", encoding );
                char * buf;
                size_t n = conv.convert( (char const *)strContent.c_str(), strContent.length() * sizeof(char32), &buf );
                this->_pureContent.setBuf( buf, n, false );
                Buffer::Free(buf);
                this->setContentEncoding(encoding);
            }
            else
            {
                this->_pureContent.setBuf( (void *)strContent.c_str(), strContent.length() * sizeof(char32), false );
                this->setContentEncoding("UTF-32BE");
            }
        }
        break;
    }
}

void TextArchive::_recognizeEncode( Buffer const & content, size_t * pI )
{
    this->_fileEncoding = RecognizeFileEncoding( content, pI, 1024 );
}


} // namespace winux
