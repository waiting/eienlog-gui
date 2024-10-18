#pragma once

#ifdef MIXED_REF_NO_EXCEPTION
#define MIXED_REF_TYPE_METHOD( mt, ty, memb, funcname )\
    ty& ref##funcname() { return memb; }\
    ty const& ref##funcname() const { return memb; }
#define MIXED_REF_TYPE_METHOD_OUTER( mt, ty, memb, funcname )\
    template<> inline ty& Mixed::ref<ty>() { return memb; }\
    template<> inline ty const& Mixed::ref<ty>() const { return memb; }
#else
#define MIXED_REF_TYPE_METHOD( mt, ty, memb, funcname )\
    ty& ref##funcname() { if ( this->_type != mt ) throw MixedError( MixedError::meUnexpectedType, "ref"#funcname"(): " + TypeStringA(*this) + " can not be referenced as a "#mt ); return memb; }\
    ty const& ref##funcname() const { if ( this->_type != mt ) throw MixedError( MixedError::meUnexpectedType, "ref"#funcname"(): " + TypeStringA(*this) + " can not be referenced as a "#mt ); return memb; }
#define MIXED_REF_TYPE_METHOD_OUTER( mt, ty, memb, funcname )\
    template<> inline ty& Mixed::ref<ty>() { if ( this->_type != mt ) throw MixedError( MixedError::meUnexpectedType, "ref<"#ty">(): " + TypeStringA(*this) + " can not be referenced as a "#mt ); return memb; }\
    template<> inline ty const& Mixed::ref<ty>() const { if ( this->_type != mt ) throw MixedError( MixedError::meUnexpectedType, "ref<"#ty">(): " + TypeStringA(*this) + " can not be referenced as a "#mt ); return memb; }
#endif

// Mixed 引用指定的类型列表
#define MIXED_REF_TYPE_LIST(_)\
_(MT_BOOLEAN, bool, _boolVal, Bool)\
_(MT_CHAR, char, _chVal, Char)\
_(MT_BYTE, byte, _btVal, Byte)\
_(MT_SHORT, short, _shVal, Short)\
_(MT_USHORT, ushort, _ushVal, UShort)\
_(MT_INT, int, _iVal, Int)\
_(MT_UINT, uint, _uiVal, UInt)\
_(MT_LONG, long, _lVal, Long)\
_(MT_ULONG, ulong, _ulVal, ULong)\
_(MT_FLOAT, float, _fltVal, Float)\
_(MT_INT64, int64, _i64Val, Int64)\
_(MT_UINT64, uint64, _ui64Val, UInt64)\
_(MT_DOUBLE, double, _dblVal, Double)\
_(MT_ANSI, AnsiString, *_pStr, Ansi)\
_(MT_UNICODE, UnicodeString, *_pWStr, Unicode)\
_(MT_BINARY, Buffer, *_pBuf, Buffer)\
_(MT_ARRAY, MixedArray, *_pArr, Array)\
_(MT_COLLECTION, Collection, *_pColl, Collection)

// 生成 Mixed 引用类型方法
MIXED_REF_TYPE_LIST(MIXED_REF_TYPE_METHOD)
