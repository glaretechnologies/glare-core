/*=====================================================================
UTF8Utils.cpp
-------------
Copyright Glare Technologies Limited 2021 -
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


// Convert a single unicode character, encoded in UTF-8 and store in a uint32, to the Unicode code point value.
uint32 codePointForUTF8Char(uint32 utf8_char)
{
	if((utf8_char & 0x80) == 0) // If left bit of byte 0 is 0:
	{
		// 1 byte UTF8-rep:
		return utf8_char;
	}
	else if((utf8_char & 0xE0) == 0xC0) // If left 3 bits of byte 0 are 110:
	{
		return	((utf8_char & 0x1F) << 6) |  // right five bits of byte 0
				((utf8_char & 0x3F00) >> 8); // right 6 bits of byte 1
	}
	else if((utf8_char & 0xF0) == 0xE0) // If left 4 bits of byte 0 are 1110:
	{
		return	((utf8_char & 0x0F) << 12) |  // right four bits of byte 0
				((utf8_char & 0x3F00) >> 2) | // right 6 bits of byte 1
				((utf8_char & 0x3F0000) >> 16);  // right 6 bits of byte 2
	}
	else
	{
		assert((utf8_char & 0xF8) == 0xF0); // left 5 bits of byte 0 should be 11110.
		return	((utf8_char & 0x07) << 18) |  // right 3 bits of byte 0
				((utf8_char & 0x3F00) << 4) | // right 6 bits of byte 1
				((utf8_char & 0x3F0000) >> 10) |  // right 6 bits of byte 2
				((utf8_char & 0x3F000000) >> 24);  // right 6 bits of byte 3
	}
}


// Convert a single unicode character, encoded in UTF-8 and store in a std::string, to the Unicode code point value.
uint32 codePointForUTF8CharString(string_view s)
{
	if(s.empty())
		throw glare::Exception("Invalid Unicode character");
	const uint8* const data = (const uint8*)s.data();

	if(data[0] <= 0x7F) // (data[0] & 0x80) == 0) // If left bit of byte 0 is 0:
	{
		// 1 byte UTF8-rep:
		return data[0];
	}
	else if(data[0] < 0xE0) // (data[0] & 0xE0) == 0xC0) // If left 3 bits of byte 0 are 110:
	{
		if(s.size() < 2)
			throw glare::Exception("Invalid Unicode character");

		//return	((data[0] & 0x1F) << 6) |  // right five bits of byte 0
		//		((data[1] & 0x3F)); // right 6 bits of byte 1
		return ((uint32)data[0] << 6) + (uint32)data[1] - ((0xC0 << 6) | 0x80);
	}
	else if(data[0] < 0xF0) // (data[0] & 0xF0) == 0xE0) // If left 4 bits of byte 0 are 1110:
	{
		if(s.size() < 3)
			throw glare::Exception("Invalid Unicode character");

		//return	((data[0] & 0x0F) << 12) |  // right four bits of byte 0
		//		((data[1] & 0x3F) << 6) | // right 6 bits of byte 1
		//		((data[2] & 0x3F));  // right 6 bits of byte 2
		return ((uint32)data[0] << 12) + ((uint32)data[1] << 6) + (uint32)data[2] - ((0xE0 << 12) | (0x80 << 6) | 0x80);
	}
	else
	{
		assert((data[0] & 0xF8) == 0xF0); // left 5 bits of byte 0 should be 11110.
		if(s.size() < 4)
			throw glare::Exception("Invalid Unicode character");
		
		//return	((data[0] & 0x07) << 18) |  // right 3 bits of byte 0
		//		((data[1] & 0x3F) << 12) | // right 6 bits of byte 1
		//		((data[2] & 0x3F) << 6) |  // right 6 bits of byte 2
		//		((data[3] & 0x3F));  // right 6 bits of byte 3
		return ((uint32)data[0] << 18) + ((uint32)data[1] << 12) + ((uint32)data[2] << 6) + (uint32)data[3] - ((0xF0 << 18) | (0x80 << 12) | (0x80 << 6) | 0x80);
	}
}


static inline bool isContinuationByte(uint8 b)
{
	return (b & 0xC0) == 0x80; // Left two bits should be 10, see https://en.wikipedia.org/wiki/UTF-8
}


size_t numCodePointsInString(const std::string& s)
{
	const uint8* data = (const uint8*)s.c_str();

	size_t num_chars = 0;
	// "Every byte that does not start 10xxxxxx is the start of a UCS character sequence." - http://www.cl.cam.ac.uk/~mgk25/ucs/utf-8-history.txt
	for(size_t i=0; i<s.size(); ++i)
		if(!isContinuationByte(data[i])) // If leftmost two bits are != 10:
			num_chars++;
	return num_chars;
}


size_t numCodePointsInString(const char* data_, size_t num_bytes)
{
	const uint8* data = (const uint8*)data_;

	size_t num_chars = 0;
	// "Every byte that does not start 10xxxxxx is the start of a UCS character sequence." - http://www.cl.cam.ac.uk/~mgk25/ucs/utf-8-history.txt
	for(size_t i=0; i<num_bytes; ++i)
		if(!isContinuationByte(data[i])) // If leftmost two bits are != 10:
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


const std::string charString(uint32 utf8_char)
{
	const size_t num_bytes = numBytesForChar((uint8)(utf8_char & 0xFF));
	std::string s;
	s.resize(num_bytes);
	std::memcpy(&s[0], &utf8_char, num_bytes);
	return s;
}


// Returns the byte index of the given character
// Throws an glare::Exception if char_index is out of bounds.
size_t byteIndex(const uint8* data, size_t num_bytes, size_t char_index)
{
	size_t byte_i = 0;
	for(size_t i=0; i<char_index; ++i)
	{
		if(byte_i >= num_bytes)
			throw glare::Exception("out of bounds");
		const size_t bytes_for_char = numBytesForChar(data[byte_i]);
		byte_i += bytes_for_char;
	}
	if(byte_i >= num_bytes)
		throw glare::Exception("out of bounds");
	return byte_i;
}


// Returns the character, still in UTF-8 encoding, in a uint32.
// Throws an glare::Exception if char_index is out of bounds.
uint32 charAt(const uint8* data, size_t num_bytes, size_t char_index)
{
	size_t byte_i = byteIndex(data, num_bytes, char_index);

	const size_t bytes_for_char = numBytesForChar(data[byte_i]);
	if(byte_i + bytes_for_char > num_bytes)
		throw glare::Exception("out of bounds");

	// Copy character bytes to uint32 to return
	uint32 res = 0;
	std::memcpy(&res, data + byte_i, bytes_for_char);
	return res;
}


bool isValidUTF8String(string_view s)
{
	const uint8* const str = (const uint8*)s.data();
	const size_t s_size = s.size();
	for(size_t i=0; i<s_size; )
	{
		const uint8 first_byte = str[i++];

		if((first_byte & 0x80) == 0) // If left bit of byte 0 is 0:
		{
			// No continuation bytes
		}
		else if((first_byte & 0xE0) == 0xC0) // If left 3 bits of byte 0 are 110:
		{
			// 1 continuation byte follows
			if(i >= s_size || !isContinuationByte(str[i]))
				return false;
			i++;
		}
		else if((first_byte & 0xF0) == 0xE0) // If left 4 bits of byte 0 are 1110:
		{
			// 2 continuation bytes follow
			for(int z=0; z<2; ++z)
			{
				if(i >= s_size || !isContinuationByte(str[i]))
					return false;
				i++;
			}
		}
		else if((first_byte & 0xF8) == 0xF0) // If left 5 bits of byte 0 are 11110:
		{
			// 3 continuation bytes follow
			for(int z=0; z<3; ++z)
			{
				if(i >= s_size || !isContinuationByte(str[i]))
					return false;
				i++;
			}
		}
		else
			return false;
	}
	return true;
}


} // end namespace UTF8Utils



#if BUILD_TESTS


#include "TestUtils.h"
#include "Timer.h"
#include "ConPrint.h"


namespace UTF8Utils
{


void testByteIndexThrowsExcep(const uint8* data, size_t num_bytes, size_t char_index)
{
	try
	{
		byteIndex(data, num_bytes, char_index);
		failTest("Expected excep.");
	}
	catch(glare::Exception&)
	{}
}


void testCharAtThrowsExcep(const uint8* data, size_t num_bytes, size_t char_index)
{
	try
	{
		charAt(data, num_bytes, char_index);
		failTest("Expected excep.");
	}
	catch(glare::Exception&)
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

	//========================= encodeCodePoint =============================
	testAssert(encodeCodePoint('a') == "a");
	testAssert(encodeCodePoint(0x393) == gamma);
	testAssert(encodeCodePoint(0x20AC) == euro);
	testAssert(encodeCodePoint(0x2005A) == cui);

	//========================= codePointForUTF8Char =============================
	testAssert(codePointForUTF8Char(0x61) == 0x61); // 'a'
	testAssert(codePointForUTF8Char(0x93CE) == 0x393); // Assuming little-endian, so byte 0 (with value 0xCE) is least-significant byte in the uint32.
	testAssert(codePointForUTF8Char(0xAC82E2) == 0x20AC);
	testAssert(codePointForUTF8Char(0x9A81A0F0) == 0x2005A);

	//========================= codePointForUTF8CharString =============================
	testAssert(codePointForUTF8CharString("a") == 0x61); // 'a'
	testAssert(codePointForUTF8CharString(gamma) == 0x393); // Assuming little-endian, so byte 0 (with value 0xCE) is least-significant byte in the uint32.
	testAssert(codePointForUTF8CharString(euro) == 0x20AC);
	testAssert(codePointForUTF8CharString(cui) == 0x2005A);

	// Check that the encodeCodePoint -> codePointForUTF8CharString round trip is correct.
	for(uint32 code_point=0; code_point<0x10FFFF; ++code_point)
	{
		const std::string encoded = encodeCodePoint(code_point);
		testAssert(codePointEncodedLength(code_point) == encoded.length()); // Check codePointEncodedLength() is correct.

		// Convert encoded string into encoded uint32.
		uint32 x = 0;
		std::memcpy((char*)&x, &encoded[0], encoded.length());

		testAssert(charString(x) == encoded);

		testAssert(codePointForUTF8Char(x) == code_point);

		testAssert(codePointForUTF8CharString(encoded) == code_point);

		testAssert(numBytesForChar(encoded[0]) == encoded.length());
	}

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

	//========================= numBytesForChar ==============================
	testAssert(numBytesForChar((uint8)'a') == 1);
	testAssert(numBytesForChar((uint8)'\xCE') == 2);
	testAssert(numBytesForChar((uint8)'\xE2') == 3);
	testAssert(numBytesForChar((uint8)'\xF0') == 4);

	//========================= charString ==============================
	testAssert(charString('a') == "a");
	testAssert(charString(0x93CE) == gamma);
	testAssert(charString(0xAC82E2) == euro);
	testAssert(charString(0x9A81A0F0) == cui);

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

	//========================= charAt ==============================
	{
		std::string s = "";
		testCharAtThrowsExcep(NULL, s.size(), 0);
	}

	{
		std::string s = euro;
		testAssert(charString(charAt((const uint8*)s.data(), s.size(), 0)) == euro);
		testCharAtThrowsExcep((const uint8*)s.data(), s.size(), 1);
	}
	{
		std::string s = euro + "a" + euro + "b";
		testAssert(charString(charAt((const uint8*)s.data(), s.size(), 0)) == euro);
		testAssert(charString(charAt((const uint8*)s.data(), s.size(), 1)) == "a");
		testAssert(charString(charAt((const uint8*)s.data(), s.size(), 2)) == euro);
		testAssert(charString(charAt((const uint8*)s.data(), s.size(), 3)) == "b");
		testCharAtThrowsExcep((const uint8*)s.data(), s.size(), 4);
	}
	{
		std::string s = gamma + "a" + gamma + "b";
		testAssert(charString(charAt((const uint8*)s.data(), s.size(), 0)) == gamma);
		testAssert(charString(charAt((const uint8*)s.data(), s.size(), 1)) == "a");
		testAssert(charString(charAt((const uint8*)s.data(), s.size(), 2)) == gamma);
		testAssert(charString(charAt((const uint8*)s.data(), s.size(), 3)) == "b");
		testCharAtThrowsExcep((const uint8*)s.data(), s.size(), 4);
	}

	{
		std::string s = cui + "a" + cui + "b";
		testAssert(charString(charAt((const uint8*)s.data(), s.size(), 0)) == cui);
		testAssert(charString(charAt((const uint8*)s.data(), s.size(), 1)) == "a");
		testAssert(charString(charAt((const uint8*)s.data(), s.size(), 2)) == cui);
		testAssert(charString(charAt((const uint8*)s.data(), s.size(), 3)) == "b");
		testCharAtThrowsExcep((const uint8*)s.data(), s.size(), 4);
	}

	//============================== isValidUTF8String ==============================
	testAssert(isValidUTF8String(""));
	testAssert(isValidUTF8String("a"));
	testAssert(isValidUTF8String(gamma));
	testAssert(isValidUTF8String(euro));
	testAssert(isValidUTF8String(cui));
	testAssert(isValidUTF8String("a" + gamma + euro + cui));


	testAssert(!isValidUTF8String("\x80")); // 0b1000000: invalid first byte

	testAssert(!isValidUTF8String("\xC0z")); // 2-byte sequence with invalid continuation byte
	testAssert(!isValidUTF8String("\xC0\xC0")); // 2-byte sequence with invalid continuation byte
	testAssert(!isValidUTF8String("\xC0\xFF")); // 2-byte sequence with invalid continuation byte
	testAssert(!isValidUTF8String("\xC0")); // 2-byte sequence truncated

	testAssert(!isValidUTF8String("\xE0\x80\xFF")); // 3-byte sequence with invalid continuation byte (0xFF)
	testAssert(!isValidUTF8String("\xE0\xFF\x80")); // 3-byte sequence with invalid continuation byte
	testAssert(!isValidUTF8String("\xE0\xFF")); // 3-byte sequence truncated
	testAssert(!isValidUTF8String("\xE0")); // 3-byte sequence truncated

	testAssert(!isValidUTF8String("\xF0\x80\x80\xFF")); // 4-byte sequence with invalid continuation byte (0xFF)
	testAssert(!isValidUTF8String("\xF0\x80\xFF\x80")); // 4-byte sequence with invalid continuation byte
	testAssert(!isValidUTF8String("\xF0\xFF\x80\x80")); // 4-byte sequence with invalid continuation byte
	testAssert(!isValidUTF8String("\xF0\xFF\x80")); // 4-byte sequence truncated
	testAssert(!isValidUTF8String("\xF0\xFF")); // 4-byte sequence truncated
	testAssert(!isValidUTF8String("\xF0")); // 4-byte sequence truncated

}


}


#endif
