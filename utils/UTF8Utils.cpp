/*=====================================================================
UTF8Utils.cpp
-------------
Copyright Glare Technologies Limited 2015 -
Generated at 2015-07-01 22:26:50 +0100
=====================================================================*/
#include "UTF8Utils.h"


#include "Exception.h"
#include "ConPrint.h"
#include "StringUtils.h"
#include <assert.h>
#include <cstring>


namespace UTF8Utils
{


size_t codePointEncodedLength(uint32 codepoint)
{
	assert(codepoint <= 0x10FFFF);
	if(codepoint <= 0x007F)
		return 1;
	if(codepoint <= 0x07FF)
		return 2;
	if(codepoint <= 0xFFFF)
		return 3;
	return 4;
}


const std::string encodeCodePoint(uint32 codepoint)
{
	assert(codepoint <= 0x10FFFF);
	std::string s;
	if(codepoint <= 0x007F)
	{
		s.resize(1);
		s[0] = (char)codepoint;
	}
	else if(codepoint <= 0x07FF)
	{
		s.resize(2);
		s[0] = ((codepoint >> 6) & 0x1F) | 0xC0; // Get bits [6, 11) of codepoint, set left bits to 110.
		s[1] = (codepoint        & 0x3F) | 0x80; // Get lowest 6 bits of codepoint, set left bits to 10.
	}
	else if(codepoint <= 0xFFFF)
	{
		s.resize(3);
		s[0] = ((codepoint >> 12) & 0xF)  | 0xE0; // Get bits [12, 16) of codepoint, set left bits to 1110.
		s[1] = ((codepoint >> 6)  & 0x3F) | 0x80; // Get bits [6, 12) of codepoint, set left bits to 10.
		s[2] = (codepoint         & 0x3F) | 0x80; // Get lowest 6 bits of codepoint, set left bits to 10.
	}
	else
	{
		s.resize(4);
		s[0] = ((codepoint >> 18) & 0x7)  | 0xF0; // Get bits [18, 21) of codepoint, set left bits to 11110.
		s[1] = ((codepoint >> 12) & 0x3F) | 0x80; // Get bits [12, 18) of codepoint, set left bits to 10.
		s[2] = ((codepoint >> 6)  & 0x3F) | 0x80; // Get bits [6, 12) of codepoint, set left bits to 10.
		s[3] = (codepoint         & 0x3F) | 0x80; // Get lowest 6 bits of codepoint, set left bits to 10.
	}
	return s;
}


size_t numCodePointsInString(const std::string& s)
{
	const uint8* data = (const uint8*)s.c_str();

	size_t num_chars = 0;
	// "Every byte that does not start 10xxxxxx is the start of a UCS character sequence." - http://www.cl.cam.ac.uk/~mgk25/ucs/utf-8-history.txt
	for(size_t i=0; i<s.size(); ++i)
		if((data[i] & 0xC0) != 0x80) // If leftmost two bits are != 10:
			num_chars++;
	return num_chars;
}


size_t numCodePointsInString(const char* data_, size_t num_bytes)
{
	const uint8* data = (const uint8*)data_;

	size_t num_chars = 0;
	// "Every byte that does not start 10xxxxxx is the start of a UCS character sequence." - http://www.cl.cam.ac.uk/~mgk25/ucs/utf-8-history.txt
	for(size_t i=0; i<num_bytes; ++i)
		if((data[i] & 0xC0) != 0x80) // If leftmost two bits are != 10:
			num_chars++;
	return num_chars;
}


size_t numBytesForChar(uint8 first_byte)
{
	if((first_byte & 0x80) == 0) // If left bit of byte 0 is 0:
		return 1;
	else if((first_byte & 0xE0) == 0xC0) // If left 3 bits of byte 0 are 110:
		return 2;
	else if((first_byte & 0xF0) == 0xE0) // If left 4 bits of byte 0 are 1110:
		return 3;
	else
	{
		assert((first_byte & 0xF8) == 0xF0); // left 5 bits of byte 0 should be 11110.
		return 4;
	}
}


// Returns the byte index of the given character
// Throws an Indigo::Exception if char_index is out of bounds.
size_t byteIndex(const uint8* data, size_t num_bytes, size_t char_index)
{
	size_t byte_i = 0;
	for(size_t i=0; i<char_index; ++i)
	{
		if(byte_i >= num_bytes)
			throw Indigo::Exception("out of bounds");
		const size_t bytes_for_char = numBytesForChar(data[byte_i]);
		byte_i += bytes_for_char;
	}
	if(byte_i >= num_bytes)
		throw Indigo::Exception("out of bounds");
	return byte_i;
}


// Returns the character, still in UTF-8 encoding, in a uint32.
// Throws an Indigo::Exception if char_index is out of bounds.
uint32 charAt(const uint8* data, size_t num_bytes, size_t char_index)
{
	size_t byte_i = byteIndex(data, num_bytes, char_index);

	const size_t bytes_for_char = numBytesForChar(data[byte_i]);
	if(byte_i + bytes_for_char > num_bytes)
		throw Indigo::Exception("out of bounds");

	// Copy character bytes to uint32 to return
	uint32 res = 0;
	std::memcpy(&res, data, bytes_for_char);
	return res;
}


} // end namespace UTF8Utils



#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"


namespace UTF8Utils
{


void testByteIndexThrowsExcep(const uint8* data, size_t num_bytes, size_t char_index)
{
	try
	{
		byteIndex(data, num_bytes, char_index);
		failTest("Expected excep.");
	}
	catch(Indigo::Exception&)
	{}
}


void test()
{
	conPrint("UTF8Utils::test()");

	const std::string gamma = "\xCE\x93"; // Greek capital letter gamma, U+393, http://unicodelookup.com/#gamma/1, 

	// This is the Euro sign, encoded in UTF-8: see http://en.wikipedia.org/wiki/UTF-8
	const std::string euro = "\xE2\x82\xAC";

	// A vietnamese character: U+2005A  http://www.isthisthingon.org/unicode/index.php?page=20&subpage=0&glyph=2005A
	const std::string cui = "\xF0\xA0\x81\x9A";

	//========================= codePointEncodedLength =============================
	testAssert(codePointEncodedLength('a') == 1);
	testAssert(codePointEncodedLength(0x393) == 2);
	testAssert(codePointEncodedLength(0x20AC) == 3);
	testAssert(codePointEncodedLength(0x2005A) == 4);

	//========================= encode =============================
	testAssert(encodeCodePoint('a') == "a");
	testAssert(encodeCodePoint(0x393) == gamma);
	testAssert(encodeCodePoint(0x20AC) == euro);
	testAssert(encodeCodePoint(0x2005A) == cui);

	//========================= numCodePointsInString ==============================
	testAssert(numCodePointsInString("") == 0);
	testAssert(numCodePointsInString(gamma) == 1);
	testAssert(numCodePointsInString(euro) == 1);
	testAssert(numCodePointsInString(cui) == 1);
	testAssert(numCodePointsInString(gamma + gamma) == 2);
	testAssert(numCodePointsInString(euro + euro) == 2);
	testAssert(numCodePointsInString(cui + cui) == 2);
	testAssert(numCodePointsInString(" " + euro + euro) == 3);
	testAssert(numCodePointsInString("a" + euro + "b" + euro) == 4);

	//========================= numCodePointsInString ==============================
	testAssert(numBytesForChar((uint8)'a') == 1);
	testAssert(numBytesForChar((uint8)'\xCE') == 2);
	testAssert(numBytesForChar((uint8)'\xE2') == 3);
	testAssert(numBytesForChar((uint8)'\xF0') == 4);

	//========================= byteIndex ==============================
	{
		std::string s = "";
		testByteIndexThrowsExcep(NULL, s.size(), 0);
	}

	{
		std::string s = euro;
		testAssert(byteIndex((const uint8*)s.data(), s.size(), 0) == 0);
		testByteIndexThrowsExcep((const uint8*)s.data(), s.size(), 1);
	}
	{
		std::string s = euro + "a" + euro + "b";
		testAssert(byteIndex((const uint8*)s.data(), s.size(), 0) == 0);
		testAssert(byteIndex((const uint8*)s.data(), s.size(), 1) == 3);
		testAssert(byteIndex((const uint8*)s.data(), s.size(), 2) == 4);
		testAssert(byteIndex((const uint8*)s.data(), s.size(), 3) == 7);
		testByteIndexThrowsExcep((const uint8*)s.data(), s.size(), 4);
	}
	{
		std::string s = gamma + "a" + gamma + "b";
		testAssert(byteIndex((const uint8*)s.data(), s.size(), 0) == 0);
		testAssert(byteIndex((const uint8*)s.data(), s.size(), 1) == 2);
		testAssert(byteIndex((const uint8*)s.data(), s.size(), 2) == 3);
		testAssert(byteIndex((const uint8*)s.data(), s.size(), 3) == 5);
		testByteIndexThrowsExcep((const uint8*)s.data(), s.size(), 4);
	}

	{
		std::string s = cui + "a" + cui + "b";
		testAssert(byteIndex((const uint8*)s.data(), s.size(), 0) == 0);
		testAssert(byteIndex((const uint8*)s.data(), s.size(), 1) == 4);
		testAssert(byteIndex((const uint8*)s.data(), s.size(), 2) == 5);
		testAssert(byteIndex((const uint8*)s.data(), s.size(), 3) == 9);
		testByteIndexThrowsExcep((const uint8*)s.data(), s.size(), 4);
	}

}


}


#endif
