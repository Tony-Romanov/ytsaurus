//
// ByteOrder.h
//
// Library: Foundation
// Package: Core
// Module:  ByteOrder
//
// Copyright (c) 2004-2014, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef DB_Foundation_ByteOrder_INCLUDED
#define DB_Foundation_ByteOrder_INCLUDED


#include "DBPoco/Foundation.h"
#include "DBPoco/Types.h"


namespace DBPoco
{


class Foundation_API ByteOrder
/// This class contains a number of static methods
/// to convert between big-endian and little-endian
/// integers of various sizes.
{
public:
    static Int16 flipBytes(Int16 value);
    static UInt16 flipBytes(UInt16 value);
    static Int32 flipBytes(Int32 value);
    static UInt32 flipBytes(UInt32 value);
    static Int64 flipBytes(Int64 value);
    static UInt64 flipBytes(UInt64 value);

    static Int16 toBigEndian(Int16 value);
    static UInt16 toBigEndian(UInt16 value);
    static Int32 toBigEndian(Int32 value);
    static UInt32 toBigEndian(UInt32 value);
    static Int64 toBigEndian(Int64 value);
    static UInt64 toBigEndian(UInt64 value);

    static Int16 fromBigEndian(Int16 value);
    static UInt16 fromBigEndian(UInt16 value);
    static Int32 fromBigEndian(Int32 value);
    static UInt32 fromBigEndian(UInt32 value);
    static Int64 fromBigEndian(Int64 value);
    static UInt64 fromBigEndian(UInt64 value);

    static Int16 toLittleEndian(Int16 value);
    static UInt16 toLittleEndian(UInt16 value);
    static Int32 toLittleEndian(Int32 value);
    static UInt32 toLittleEndian(UInt32 value);
    static Int64 toLittleEndian(Int64 value);
    static UInt64 toLittleEndian(UInt64 value);

    static Int16 fromLittleEndian(Int16 value);
    static UInt16 fromLittleEndian(UInt16 value);
    static Int32 fromLittleEndian(Int32 value);
    static UInt32 fromLittleEndian(UInt32 value);
    static Int64 fromLittleEndian(Int64 value);
    static UInt64 fromLittleEndian(UInt64 value);

    static Int16 toNetwork(Int16 value);
    static UInt16 toNetwork(UInt16 value);
    static Int32 toNetwork(Int32 value);
    static UInt32 toNetwork(UInt32 value);
    static Int64 toNetwork(Int64 value);
    static UInt64 toNetwork(UInt64 value);

    static Int16 fromNetwork(Int16 value);
    static UInt16 fromNetwork(UInt16 value);
    static Int32 fromNetwork(Int32 value);
    static UInt32 fromNetwork(UInt32 value);
    static Int64 fromNetwork(Int64 value);
    static UInt64 fromNetwork(UInt64 value);
};


#if !defined(POCO_NO_BYTESWAP_BUILTINS)
#    if __has_builtin(__builtin_bswap32)
#        define DB_POCO_HAVE_GCC_BYTESWAP 1
#    endif
#endif


//
// inlines
//
inline UInt16 ByteOrder::flipBytes(UInt16 value)
{
#if defined(POCO_HAVE_MSC_BYTESWAP)
    return _byteswap_ushort(value);
#else
    return ((value >> 8) & 0x00FF) | ((value << 8) & 0xFF00);
#endif
}


inline Int16 ByteOrder::flipBytes(Int16 value)
{
    return Int16(flipBytes(UInt16(value)));
}


inline UInt32 ByteOrder::flipBytes(UInt32 value)
{
#if defined(POCO_HAVE_MSC_BYTESWAP)
    return _byteswap_ulong(value);
#elif defined(DB_POCO_HAVE_GCC_BYTESWAP)
    return __builtin_bswap32(value);
#else
    return ((value >> 24) & 0x000000FF) | ((value >> 8) & 0x0000FF00) | ((value << 8) & 0x00FF0000) | ((value << 24) & 0xFF000000);
#endif
}


inline Int32 ByteOrder::flipBytes(Int32 value)
{
    return Int32(flipBytes(UInt32(value)));
}


inline UInt64 ByteOrder::flipBytes(UInt64 value)
{
#    if defined(POCO_HAVE_MSC_BYTESWAP)
    return _byteswap_uint64(value);
#    elif defined(DB_POCO_HAVE_GCC_BYTESWAP)
    return __builtin_bswap64(value);
#    else
    UInt32 hi = UInt32(value >> 32);
    UInt32 lo = UInt32(value & 0xFFFFFFFF);
    return UInt64(flipBytes(hi)) | (UInt64(flipBytes(lo)) << 32);
#    endif
}


inline Int64 ByteOrder::flipBytes(Int64 value)
{
    return Int64(flipBytes(UInt64(value)));
}


//
// some macro trickery to automate the method implementation
//
#define DB_POCO_IMPLEMENT_BYTEORDER_NOOP_(op, type) \
    inline type ByteOrder::op(type value) \
    { \
        return value; \
    }
#define DB_POCO_IMPLEMENT_BYTEORDER_FLIP_(op, type) \
    inline type ByteOrder::op(type value) \
    { \
        return flipBytes(value); \
    }


#    define DB_POCO_IMPLEMENT_BYTEORDER_NOOP(op) \
        DB_POCO_IMPLEMENT_BYTEORDER_NOOP_(op, Int16) \
        DB_POCO_IMPLEMENT_BYTEORDER_NOOP_(op, UInt16) \
        DB_POCO_IMPLEMENT_BYTEORDER_NOOP_(op, Int32) \
        DB_POCO_IMPLEMENT_BYTEORDER_NOOP_(op, UInt32) \
        DB_POCO_IMPLEMENT_BYTEORDER_NOOP_(op, Int64) \
        DB_POCO_IMPLEMENT_BYTEORDER_NOOP_(op, UInt64)
#    define DB_POCO_IMPLEMENT_BYTEORDER_FLIP(op) \
        DB_POCO_IMPLEMENT_BYTEORDER_FLIP_(op, Int16) \
        DB_POCO_IMPLEMENT_BYTEORDER_FLIP_(op, UInt16) \
        DB_POCO_IMPLEMENT_BYTEORDER_FLIP_(op, Int32) \
        DB_POCO_IMPLEMENT_BYTEORDER_FLIP_(op, UInt32) \
        DB_POCO_IMPLEMENT_BYTEORDER_FLIP_(op, Int64) \
        DB_POCO_IMPLEMENT_BYTEORDER_FLIP_(op, UInt64)


#if defined(DB_POCO_ARCH_BIG_ENDIAN)
#    define DB_POCO_IMPLEMENT_BYTEORDER_BIG DB_POCO_IMPLEMENT_BYTEORDER_NOOP
#    define DB_POCO_IMPLEMENT_BYTEORDER_LIT DB_POCO_IMPLEMENT_BYTEORDER_FLIP
#else
#    define DB_POCO_IMPLEMENT_BYTEORDER_BIG DB_POCO_IMPLEMENT_BYTEORDER_FLIP
#    define DB_POCO_IMPLEMENT_BYTEORDER_LIT DB_POCO_IMPLEMENT_BYTEORDER_NOOP
#endif


DB_POCO_IMPLEMENT_BYTEORDER_BIG(toBigEndian)
DB_POCO_IMPLEMENT_BYTEORDER_BIG(fromBigEndian)
DB_POCO_IMPLEMENT_BYTEORDER_BIG(toNetwork)
DB_POCO_IMPLEMENT_BYTEORDER_BIG(fromNetwork)
DB_POCO_IMPLEMENT_BYTEORDER_LIT(toLittleEndian)
DB_POCO_IMPLEMENT_BYTEORDER_LIT(fromLittleEndian)


} // namespace DBPoco


#endif // DB_Foundation_ByteOrder_INCLUDED
