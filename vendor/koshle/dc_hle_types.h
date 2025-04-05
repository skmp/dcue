#pragma once

#if ( (__LONG_MAX__ *2UL+1UL) == 18446744073709551615ULL) && ((__INT_MAX__  *2U +1U) == 4294967295ULL)
typedef unsigned long uint64;  /**< \brief 64-bit unsigned integer */
typedef unsigned int uint32;       /**< \brief 32-bit unsigned integer */
typedef unsigned short uint16;      /**< \brief 16-bit unsigned integer */
typedef unsigned char uint8;        /**< \brief 8-bit unsigned integer */
typedef long int64;            /**< \brief 64-bit signed integer */
typedef int int32;                 /**< \brief 32-bit signed integer */
typedef short int16;                /**< \brief 16-bit signed integer */
typedef char int8;                  /**< \brief 8-bit signed integer */ 

typedef volatile unsigned long vuint64;  /**< \brief 64-bit unsigned integer */
typedef volatile unsigned int vuint32;       /**< \brief 32-bit unsigned integer */
typedef volatile unsigned short vuint16;      /**< \brief 16-bit unsigned integer */
typedef volatile unsigned char vuint8;        /**< \brief 8-bit unsigned integer */
typedef volatile long vint64;            /**< \brief 64-bit signed integer */
typedef volatile int vint32;                 /**< \brief 32-bit signed integer */
typedef volatile short vint16;                /**< \brief 16-bit signed integer */
typedef volatile char vint8;                  /**< \brief 8-bit signed integer */ 

typedef uint64 ptr_t;
#define INT32_IS_INT
#elif ((__LONG_LONG_MAX__*2ULL+1ULL) == 18446744073709551615ULL) && ((__LONG_MAX__ *2UL+1UL) == 4294967295ULL)
// These are -m32 specific and try to follow KOS rules
typedef unsigned long long uint64;  /**< \brief 64-bit unsigned integer */
typedef unsigned long uint32;       /**< \brief 32-bit unsigned integer */
typedef unsigned short uint16;      /**< \brief 16-bit unsigned integer */
typedef unsigned char uint8;        /**< \brief 8-bit unsigned integer */
typedef long long int64;            /**< \brief 64-bit signed integer */
typedef long int32;                 /**< \brief 32-bit signed integer */
typedef short int16;                /**< \brief 16-bit signed integer */
typedef char int8;                  /**< \brief 8-bit signed integer */ 

typedef volatile unsigned long long vuint64;  /**< \brief 64-bit unsigned integer */
typedef volatile unsigned long vuint32;       /**< \brief 32-bit unsigned integer */
typedef volatile unsigned short vuint16;      /**< \brief 16-bit unsigned integer */
typedef volatile unsigned char vuint8;        /**< \brief 8-bit unsigned integer */
typedef volatile long long vint64;            /**< \brief 64-bit signed integer */
typedef volatile long vint32;                 /**< \brief 32-bit signed integer */
typedef volatile short vint16;                /**< \brief 16-bit signed integer */
typedef volatile char vint8;                  /**< \brief 8-bit signed integer */ 

typedef uint32 ptr_t;
#else
#error "Unable to detect basic types" 
#endif

static_assert(sizeof(uint64) == 8, "uint64 size is not 8 bytes");
static_assert(sizeof(uint32) == 4, "uint32 size is not 4 bytes");
static_assert(sizeof(uint16) == 2, "uint16 size is not 2 bytes");
static_assert(sizeof(uint8) == 1, "uint8 size is not 1 byte");
static_assert(sizeof(int64) == 8, "int64 size is not 8 bytes");
static_assert(sizeof(int32) == 4, "int32 size is not 4 bytes");
static_assert(sizeof(int16) == 2, "int16 size is not 2 bytes");
static_assert(sizeof(int8) == 1, "int8 size is not 1 byte");
static_assert(sizeof(vuint64) == 8, "vuint64 size is not 8 bytes");
static_assert(sizeof(vuint32) == 4, "vuint32 size is not 4 bytes");
static_assert(sizeof(vuint16) == 2, "vuint16 size is not 2 bytes");
static_assert(sizeof(vuint8) == 1, "vuint8 size is not 1 byte");
static_assert(sizeof(vint64) == 8, "vint64 size is not 8 bytes");
static_assert(sizeof(vint32) == 4, "vint32 size is not 4 bytes");
static_assert(sizeof(vint16) == 2, "vint16 size is not 2 bytes");
static_assert(sizeof(vint8) == 1, "vint8 size is not 1 byte");
static_assert(sizeof(ptr_t) == sizeof(void*), "ptr_t size is not equal to void* size");