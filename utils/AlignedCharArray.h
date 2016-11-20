/*=====================================================================
AlignedCharArray.h
------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "../maths/SSE.h"


/*======================================================================================
AlignedCharArray
----------------
Adapted from LLVM's AlignOf.h.
All of this gumpf is required since MSVC can't handle
GLARE_ALIGN(GLARE_ALIGNMENT(T)) in template parameters.
======================================================================================*/

template <typename T>
struct AlignmentCalcImpl {
  char x;
  T t;
private:
  AlignmentCalcImpl() {} // Never instantiate.
};

/// AlignOf - A templated class that contains an enum value representing
///  the alignment of the template argument.  For example,
///  AlignOf<int>::Alignment represents the alignment of type "int".  The
///  alignment calculated is the minimum alignment, and not necessarily
///  the "desired" alignment returned by GCC's __alignof__ (for example).  Note
///  that because the alignment is an enum value, it can be used as a
///  compile-time constant (e.g., for template instantiation).
template <typename T>
struct AlignOf {
  enum { Alignment = static_cast<unsigned int>(sizeof(AlignmentCalcImpl<T>) - sizeof(T)) };
};



template <int Alignment, size_t Size> struct AlignedCharArray;

// Specialisations:
template <size_t Size> struct AlignedCharArray<1,   Size> { GLARE_ALIGN(1)   char buf[Size]; };
template <size_t Size> struct AlignedCharArray<2,   Size> { GLARE_ALIGN(2)   char buf[Size]; };
template <size_t Size> struct AlignedCharArray<4,   Size> { GLARE_ALIGN(4)   char buf[Size]; };
template <size_t Size> struct AlignedCharArray<8,   Size> { GLARE_ALIGN(8)   char buf[Size]; };
template <size_t Size> struct AlignedCharArray<16,  Size> { GLARE_ALIGN(16)  char buf[Size]; };
template <size_t Size> struct AlignedCharArray<32,  Size> { GLARE_ALIGN(32)  char buf[Size]; };
template <size_t Size> struct AlignedCharArray<64,  Size> { GLARE_ALIGN(64)  char buf[Size]; };
template <size_t Size> struct AlignedCharArray<128, Size> { GLARE_ALIGN(128) char buf[Size]; };
