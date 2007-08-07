#ifndef PLATFORM_H_666
#define PLATFORM_H_666


#if defined(WIN32) || defined(WIN64)
#define _FORCEINLINE __forceinline
#else
#define _FORCEINLINE __inline
#endif

/*#ifdef DEBUG
//don't try and force inline in debug mode
#define FORCEINLINE inline
#else
#define FORCEINLINE _FORCEINLINE //winNT.h defines this already?
#endif
*/

#ifdef DEBUG
//don't try and force inline in debug mode
#define DO_FORCEINLINE inline
#else
#define DO_FORCEINLINE _FORCEINLINE //winNT.h defines this already?
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


typedef unsigned int uint32;
typedef unsigned long long uint64;


#endif //PLATFORM_H_666
