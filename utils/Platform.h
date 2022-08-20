/*=====================================================================
Platform.h
----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#if defined(_WIN32)
#define GLARE_STRONG_INLINE __forceinline
#else
#define GLARE_STRONG_INLINE inline
#endif


#if defined(_WIN32)
#define GLARE_NO_INLINE __declspec(noinline)
#else
#define GLARE_NO_INLINE __attribute__ ((noinline))
#endif


// To disallow copy-construction and assignment operators, put this in the private part of a class:
#define GLARE_DISABLE_COPY(TypeName) \
	TypeName(const TypeName &); \
	TypeName &operator=(const TypeName &);



#if defined(_MSC_VER) && !defined(__clang__)
#define COMPILER_MSVC 1
#endif


// Declare a variable as used.  This can be used to avoid the Visual Studio 'local variable is initialized but not referenced' warning.
#ifdef COMPILER_MSVC
#define GLARE_DECLARE_USED(x) (x)
#else
#define GLARE_DECLARE_USED(x) (void)(x);
#endif

// This is like the usual assert() macro, except in NDEBUG mode ('release' mode) it marks the variable as used by writing the expression as a statement.
// NOTE: Use with care as the expression may have side effects.
#ifdef NDEBUG
	#ifdef COMPILER_MSVC
	#define assertOrDeclareUsed(expr) (expr)
	#else
	#define assertOrDeclareUsed(expr) (void)(expr)
	#endif
#else
#define assertOrDeclareUsed(expr) assert(expr)
#endif


#define staticArrayNumElems(a) sizeof((a)) / sizeof((a[0]))


#ifdef _WIN32
#define GLARE_THREAD_LOCAL __declspec(thread)
#else
#define GLARE_THREAD_LOCAL __thread
#endif





#ifdef COMPILER_MSVC
#define SSE_CLASS_ALIGN _MM_ALIGN16 class
#define SSE_ALIGN _MM_ALIGN16
#else
#define SSE_CLASS_ALIGN class __attribute__ ((aligned (16)))
#define SSE_ALIGN __attribute__ ((aligned (16)))
#endif

#ifdef COMPILER_MSVC
#define AVX_CLASS_ALIGN _CRT_ALIGN(32) class
#define AVX_ALIGN _CRT_ALIGN(32)
#else
#define AVX_CLASS_ALIGN class __attribute__ ((aligned (32)))
#define AVX_ALIGN __attribute__ ((aligned (32)))
#endif


#ifdef COMPILER_MSVC
#define GLARE_ALIGN(x) _CRT_ALIGN(x)
#else
#define GLARE_ALIGN(x) __attribute__ ((aligned (x)))
#endif


#include <stdint.h>

typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
