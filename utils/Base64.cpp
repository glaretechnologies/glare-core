/*=====================================================================
Base64.cpp
-------------------
Copyright Glare Technologies Limited 2014 -
Generated at 2014-03-22 11:44:41 +0000
=====================================================================*/
#include "Base64.h"


#include "platform.h"
#include "stringutils.h"
#include "Exception.h"
#include "../indigo/TestUtils.h"
#include <assert.h>


namespace Base64
{


// Map from binary sextet to base64 encoding character.
static char base64_encoding_table[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/'
};


// Effectively a map from base64 encoding character to original binary sextet.
// NOTE: using 64 as an invalid value marker.
static unsigned char base64_decoding_table[] = {
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 62, 64, 64, 64, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64,
	64, 64, 64, 64, 64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
	18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64, 64, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
};


// Modified from http://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c
void encode(const void* data, size_t datalen, std::string& res_out)
{
	const size_t remainder = datalen % 3;
	const size_t required_size = (remainder == 0) ? (4 * (datalen / 3)) : (4 * (1 + (datalen / 3)));
	res_out.resize(required_size);

	int j = 0;
	for (int i = 0; i < datalen;)
	{
        uint32_t octet_a = i < datalen ? ((unsigned char*)data)[i++] : 0;
        uint32_t octet_b = i < datalen ? ((unsigned char*)data)[i++] : 0;
        uint32_t octet_c = i < datalen ? ((unsigned char*)data)[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        res_out[j++] = base64_encoding_table[(triple >> 3 * 6) & 0x3F];
        res_out[j++] = base64_encoding_table[(triple >> 2 * 6) & 0x3F];
        res_out[j++] = base64_encoding_table[(triple >> 1 * 6) & 0x3F];
        res_out[j++] = base64_encoding_table[(triple >> 0 * 6) & 0x3F];
    }

	if(remainder == 1)
	{
		res_out[required_size - 2] = '=';
		res_out[required_size - 1] = '=';
	}
	else if(remainder == 2)
	{
		res_out[required_size - 1] = '=';
	}

	assert(j == res_out.size());
}


void decode(const std::string& s, std::vector<unsigned char>& data_out)
{
	const size_t input_length = s.size();
	if(input_length % 4 != 0)
		throw Indigo::Exception("Invalid length");

	size_t output_length = input_length / 4 * 3;
    if (s[input_length - 1] == '=') output_length--;
    if (s[input_length - 2] == '=') output_length--;

	data_out.resize(output_length);

    for(int i = 0, j = 0; i < input_length;)
	{
        uint32_t sextet_a = s[i] == '=' ? 0 : base64_decoding_table[s[i]];  i++;
        uint32_t sextet_b = s[i] == '=' ? 0 : base64_decoding_table[s[i]];  i++;
        uint32_t sextet_c = s[i] == '=' ? 0 : base64_decoding_table[s[i]];  i++;
        uint32_t sextet_d = s[i] == '=' ? 0 : base64_decoding_table[s[i]];  i++;

		if(sextet_a == 64)
			throw Indigo::Exception("Invalid character");
		if(sextet_b == 64)
			throw Indigo::Exception("Invalid character");
		if(sextet_c == 64)
			throw Indigo::Exception("Invalid character");
		if(sextet_d == 64)
			throw Indigo::Exception("Invalid character");

        uint32_t triple = (sextet_a << 3 * 6)
        + (sextet_b << 2 * 6)
        + (sextet_c << 1 * 6)
        + (sextet_d << 0 * 6);

        if (j < output_length) data_out[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < output_length) data_out[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < output_length) data_out[j++] = (triple >> 0 * 8) & 0xFF;
    }
}


#if BUILD_TESTS


//#include <iostream>


static void doTest(const std::string& original_text, const std::string& target_encoding)
{
	std::string encoded;
	Base64::encode(original_text.c_str(), original_text.size(), encoded);
	testAssert(encoded == target_encoding);

	// Decode
	std::vector<unsigned char> decoded;
	Base64::decode(encoded, decoded);

	testAssert(original_text.size() == decoded.size());
	for(size_t i=0; i<original_text.size(); ++i)
		testAssert(original_text[i] == decoded[i]);
}


static void doEncodeAndDecodeTest(const std::vector<unsigned char>& plaintext)
{
	std::string encoded;
	Base64::encode(&plaintext[0], plaintext.size(), encoded);

	// Decode
	std::vector<unsigned char> decoded;
	Base64::decode(encoded, decoded);

	testAssert(plaintext.size() == decoded.size());
	for(size_t i=0; i<plaintext.size(); ++i)
		testAssert(plaintext[i] == decoded[i]);
}


static void testDecodingFailure(const std::string& encoded)
{
	try
	{
		// Decode
		std::vector<unsigned char> decoded;
		Base64::decode(encoded, decoded);

		failTest("Expected exception to be thrown.");
	}
	catch(Indigo::Exception& )
	{
	}
}


void test()
{
	testAssert(sizeof(base64_encoding_table) / sizeof(char) == 64);
	testAssert(sizeof(base64_decoding_table) / sizeof(unsigned char) == 256);

	// Make decoding table - effectively a map from base64 char to original sextet
	/*{
		char decoding_table[256];
		for(int i = 0; i < 256; i++)
			decoding_table[i] = 64; // NOTE: using 64 as an invalid value marker.

		for(int i = 0; i < 64; i++)
		{
			decoding_table[(char)base64_encoding_table[i]] = (char)i;

			
		}

		for(int i = 0; i < 256; i++)
			std::cout << int32ToString(decoding_table[i]) << ", ";

		std::cout << std::endl;
	}*/


	{
		std::string res;
		encode(NULL, 0, res);
		testAssert(res == "");
	}

	{
		const std::string s = "Man";
		doTest(s, "TWFu");
	}
	{
		const std::string s = "M";
		doTest(s, "TQ==");
	}
	{
		const std::string s = "Ma";
		doTest(s, "TWE=");
	}

	// Examples from http://en.wikipedia.org/wiki/Base64
	{
		const std::string s = "Man is distinguished, not only by his reason, but by this singular passion from other animals, "
			"which is a lust of the mind, that by a perseverance of delight in the continued and indefatigable generation of knowledge, "
			"exceeds the short vehemence of any carnal pleasure.";
		doTest(s, "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlz"
			"IHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2Yg"
			"dGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGlu"
			"dWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRo"
			"ZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4=");
	}

	// Test truncated quote
	{
		const std::string s = "Man is distinguished, not only by his reason, but by this singular passion from other animals, "
			"which is a lust of the mind, that by a perseverance of delight in the continued and indefatigable generation of knowledge, "
			"exceeds the short vehemence of any carnal pleasure";
		doTest(s, "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlz"
			"IHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2Yg"
			"dGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGlu"
			"dWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRo"
			"ZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZQ==");
	}
	// Test truncated quote
	{
		const std::string s = "Man is distinguished, not only by his reason, but by this singular passion from other animals, "
			"which is a lust of the mind, that by a perseverance of delight in the continued and indefatigable generation of knowledge, "
			"exceeds the short vehemence of any carnal pleasur";
		doTest(s, "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlz"
			"IHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2Yg"
			"dGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGlu"
			"dWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRo"
			"ZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3Vy");
	}
	// Test truncated quote
	{
		const std::string s = "Man is distinguished, not only by his reason, but by this singular passion from other animals, "
			"which is a lust of the mind, that by a perseverance of delight in the continued and indefatigable generation of knowledge, "
			"exceeds the short vehemence of any carnal pleasu";
		doTest(s, "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlz"
			"IHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2Yg"
			"dGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGlu"
			"dWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRo"
			"ZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3U=");
	}
	// Test truncated quote
	{
		const std::string s = "Man is distinguished, not only by his reason, but by this singular passion from other animals, "
			"which is a lust of the mind, that by a perseverance of delight in the continued and indefatigable generation of knowledge, "
			"exceeds the short vehemence of any carnal pleas";
		doTest(s, "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlz"
			"IHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2Yg"
			"dGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGlu"
			"dWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRo"
			"ZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhcw==");
	}

	{
		const std::string s = "pleasure.";
		doTest(s, "cGxlYXN1cmUu");
	}
	{
		const std::string s = "leasure.";
		doTest(s, "bGVhc3VyZS4=");
	}
	{
		const std::string s = "easure.";
		doTest(s, "ZWFzdXJlLg==");
	}
	{
		const std::string s = "asure.";
		doTest(s, "YXN1cmUu");
	}
	{
		const std::string s = "sure.";
		doTest(s, "c3VyZS4=");
	}


	// Test encoding of all possible different byte values
	{
		std::vector<unsigned char> plaintext(256);
		for(int i = 0; i < 256; i++)
			plaintext[i] = (unsigned char)i;

		doEncodeAndDecodeTest(plaintext);
	}


	//==================== Test decoding failure cases ================================

	// Invalid length
	testDecodingFailure("a");
	testDecodingFailure("aa");
	testDecodingFailure("aaa");

	// Test invalid characters
	testDecodingFailure("aaa!");
}


#endif // BUILD_TESTS


} // End namespace Base64
