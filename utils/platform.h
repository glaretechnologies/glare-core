// Copyright Glare Technologies Limited 2009 - 
#ifndef PLATFORM_H_666
#define PLATFORM_H_666


#if defined(WIN32) || defined(WIN64)
#define INDIGO_STRONG_INLINE __forceinline
#else
#define INDIGO_STRONG_INLINE inline
#endif


//Compiler Definitions
//#define COMPILER_GCC
//#define COMPILER_MSVC 1
//#define COMPILER_MSVC_6
//#define COMPILER_MSVC_2003 1

#ifdef _MSC_VER
#define COMPILER_MSVC 1
#endif

#ifdef COMPILER_MSVC_6
#pragma warning(disable : 4786)//disable long debug name warning (esp. for templates)
#endif


#ifdef COMPILER_MSVC_2003
#pragma warning(disable : 4290)//disable exception specification warning in VS2003
#endif

typedef int int32;
typedef unsigned int uint32;
typedef unsigned long long uint64;


#endif //PLATFORM_H_666
