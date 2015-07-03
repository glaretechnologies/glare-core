/*=====================================================================
UTF8Utils.h
-----------
Copyright Glare Technologies Limited 2015 -
Generated at 2015-07-01 22:26:50 +0100
=====================================================================*/
#pragma once


#include "Platform.h"
#include <string>


/*=====================================================================
UTF8Utils
---------

=====================================================================*/
namespace UTF8Utils
{
	// Number of bytes a Unicode code point will take in UTF-8.
	// Codepoint must be <= 0x10FFFF
	size_t codePointEncodedLength(uint32 codepoint);

	// Convert a single code point to a UTF-8 string.
	// Codepoint must be <= 0x10FFFF
	const std::string encodeCodePoint(uint32 codepoint);

	size_t numCodePointsInString(const std::string& s);

	size_t numCodePointsInString(const char* data, size_t num_bytes);


	size_t numBytesForChar(uint8 first_byte);

	// Returns the byte index of the given character
	// Throws an Indigo::Exception if char_index is out of bounds.
	size_t byteIndex(const uint8* data, size_t num_bytes, size_t char_index);

	// Returns the character, still in UTF-8 encoding, in a uint32.
	// Throws an Indigo::Exception if char_index is out of bounds.
	uint32 charAt(const uint8* data, size_t num_bytes, size_t char_index);


	void test();
} // end namespace UTF8Utils
