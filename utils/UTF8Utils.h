/*=====================================================================
UTF8Utils.h
-----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <string>
#include "string_view.h"


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

	// Convert a single unicode character, encoded in UTF-8 and stored in a uint32, to the Unicode code point value.
	uint32 codePointForUTF8Char(uint32 utf8_char);

	// Convert a single unicode character, encoded in UTF-8 and stored in a std::string, to the Unicode code point value.
	uint32 codePointForUTF8CharString(string_view s);


	size_t numCodePointsInString(const std::string& s);

	size_t numCodePointsInString(const char* data, size_t num_bytes);


	size_t numBytesForChar(uint8 first_byte);

	// Convert from a single Unicode char, encoded in UTF-8 and stored in a uint32, to one stored in a std::string.
	const std::string charString(uint32 utf8_char);

	// Returns the byte index of the given character
	// Throws an glare::Exception if char_index is out of bounds.
	size_t byteIndex(const uint8* data, size_t data_num_bytes, size_t char_index);

	// Returns the character, still in UTF-8 encoding, in a uint32.
	// Throws an glare::Exception if char_index is out of bounds.
	uint32 charAt(const uint8* data, size_t data_num_bytes, size_t char_index);


	bool isValidUTF8String(string_view s);

	// Replace any invalid UTF-8 byte sequences with the Unicode replacement character.
	// (Currently will replace other bytes as well if the high bit is set)
	// The resulting string will return true when passed into isValidUTF8String.
	std::string sanitiseUTF8String(const std::string& s);

	void test();
} // end namespace UTF8Utils
