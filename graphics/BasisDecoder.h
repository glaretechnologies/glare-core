/*=====================================================================
BasisDecoder.h
--------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "../utils/Reference.h"
#include "../utils/ArrayRef.h"
#include <string>
namespace glare { class Allocator; }
class Map2D;


/*=====================================================================
BasisDecoder
------------
https://github.com/BinomialLLC/basis_universal
=====================================================================*/
class BasisDecoder
{
public:
	static void init();

	struct BasisDecoderOptions
	{
		BasisDecoderOptions() : ETC_support(false) {}
		bool ETC_support;
	};

	// throws ImFormatExcep on failure
	static Reference<Map2D> decode(const std::string& path, glare::Allocator* mem_allocator = NULL, const BasisDecoderOptions& options = BasisDecoderOptions());

	static Reference<Map2D> decodeFromBuffer(const void* data, size_t size, glare::Allocator* mem_allocator = NULL, const BasisDecoderOptions& options = BasisDecoderOptions());

	static void test();
};
