// Copyright Glare Technologies Limited 2009 - 
#ifndef PLATFORM_H_GT
#define PLATFORM_H_GT


#if defined(_WIN32) && !defined(__MINGW32__)
#define INDIGO_STRONG_INLINE __forceinline
#else
#define INDIGO_STRONG_INLINE inline
#endif


// To disallow copy-construction and assignment operators, put this in the private part of a class:
#define INDIGO_DISABLE_COPY(TypeName) \
	TypeName(const TypeName &); \
	TypeName &operator=(const TypeName &);


//Compiler Definitiosn
//#define COMPILER_GCC
//#define COMPILER_MSVC 1
//#define COMPILER_MSVC_6
//#define COMPILER_MSVC_2003 1

#ifdef _MSC_VER
#define COMPILER_MSVC 1
#endif


// Workaround for MSVC not including stdint.h for fixed-width int types
#ifdef _MSC_VER
typedef signed __int8		int8_t;
typedef signed __int16		int16_t;
typedef signed __int32		int32_t;
typedef signed __int64		int64_t;
typedef unsigned __int8		uint8_t;
typedef unsigned __int16	uint16_t;
typedef unsigned __int32	uint32_t;
typedef unsigned __int64	uint64_t;
#else
#include <stdint.h>
#endif

typedef uint8_t uint8;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;


#endif //PLATFORM_H_GT
