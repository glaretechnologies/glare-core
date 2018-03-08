/*=====================================================================
Platform.h
-------------------
Copyright Glare Technologies Limited 2014 -
=====================================================================*/
#pragma once


#if defined(_WIN32) && !defined(__MINGW32__)
#define INDIGO_STRONG_INLINE __forceinline
#else
#define INDIGO_STRONG_INLINE inline
#endif


// To disallow copy-construction and assignment operators, put this in the private part of a class:
#define INDIGO_DISABLE_COPY(TypeName) \
	TypeName(const TypeName &); \
	TypeName &operator=(const TypeName &);


// Declare a variable as used.  This can be used to avoid the Visual Studio 'local variable is initialized but not referenced' warning.
#ifdef _MSC_VER
#define GLARE_DECLARE_USED(x) (x)
#else
#define GLARE_DECLARE_USED(x) (void)(x);
#endif

// This is like the usual assert() macro, except in NDEBUG mode ('release' mode) it marks the variable as used by writing the expression as a statement.
// NOTE: Use with care as the expression may have side effects.
#ifdef NDEBUG
	#ifdef _MSC_VER
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


#if defined(_MSC_VER) && !defined(__clang__)
#define COMPILER_MSVC 1
#endif


#include <stdint.h>

typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
