/*=====================================================================
Base64.cpp
----------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "Base64.h"


#include "Platform.h"
#include "StringUtils.h"
#include "Exception.h"
#include <limits>
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
void encode(const void* data_, size_t datalen, std::string& res_out)
{
	const size_t remainder = datalen % 3;
	const size_t required_size = (remainder == 0) ? (4 * (datalen / 3)) : (4 * (1 + (datalen / 3)));
	res_out.resize(required_size);
	
	const unsigned char* const data = (const unsigned char*)data_;
	char* const res_out_data = (required_size > 0) ? &res_out[0] : NULL;

	size_t j = 0;
	size_t i = 0;
	if(datalen >= 3)
	{
		// Process all input bytes starting up to 3 from the end, so we don't need to bounds check.
		for(; i < datalen - 3; i += 3)
		{
			assert(i + 2 < datalen);
			// Get 3 bytes (24 bits), OR them together into a 32-bit uint.
			uint32 octet_a = data[i + 0];
			uint32 octet_b = data[i + 1];
			uint32 octet_c = data[i + 2];

			uint32 triple = (octet_a << 16) | (octet_b << 8) | octet_c;

			// Break the 24 bits up into 4 sextets.
			assert(j + 3 < res_out.size());
			res_out_data[j++] = base64_encoding_table[(triple >> 18) & 0x3F];
			res_out_data[j++] = base64_encoding_table[(triple >> 12) & 0x3F];
			res_out_data[j++] = base64_encoding_table[(triple >>  6) & 0x3F];
			res_out_data[j++] = base64_encoding_table[(triple >>  0) & 0x3F];
		}
	}

	// Process any remaining input bytes
	for(; i < datalen;)
	{
		// Get 3 bytes (24 bits), OR them together into a 32-bit uint.
		uint32 octet_a = i < datalen ? data[i++] : 0;
		uint32 octet_b = i < datalen ? data[i++] : 0;
		uint32 octet_c = i < datalen ? data[i++] : 0;

		uint32 triple = (octet_a << 16) | (octet_b << 8) | octet_c;

		// Break the 24 bits up into 4 sextets.
		assert(j + 3 < res_out.size());
		res_out_data[j++] = base64_encoding_table[(triple >> 18) & 0x3F];
		res_out_data[j++] = base64_encoding_table[(triple >> 12) & 0x3F];
		res_out_data[j++] = base64_encoding_table[(triple >>  6) & 0x3F];
		res_out_data[j++] = base64_encoding_table[(triple >>  0) & 0x3F];
	}

	if(remainder == 1)
	{
		res_out_data[required_size - 2] = '=';
		res_out_data[required_size - 1] = '=';
	}
	else if(remainder == 2)
	{
		res_out_data[required_size - 1] = '=';
	}

	assert(j == res_out.size());
}


void decode(const string_view s, std::vector<unsigned char>& data_out)
{
	const size_t input_length = s.size();
	if(input_length % 4 != 0)
		throw glare::Exception("Invalid length");
	if(input_length == 0)
	{
		data_out.resize(0);
		return;
	}
	assert((input_length % 4 == 0) && (input_length >= 4));

	const unsigned char* const input = (const unsigned char*)s.data();

	size_t output_length = input_length / 4 * 3;
	if(s[input_length - 1] == '=') output_length--;
	if(s[input_length - 2] == '=') output_length--;

	data_out.resize(output_length);

	// Handle initial blocks, which aren't allowed to have padding characters in them.
	size_t last_input_block_index = input_length - 4;

	for(size_t i = 0, j = 0; i < last_input_block_index; i += 4, j += 3)
	{
		// Get 4 sextets, decode from ASCII to binary
		assert(i + 3 < input_length);
		uint32 sextet_a = base64_decoding_table[input[i + 0]];
		uint32 sextet_b = base64_decoding_table[input[i + 1]];
		uint32 sextet_c = base64_decoding_table[input[i + 2]];
		uint32 sextet_d = base64_decoding_table[input[i + 3]];

		// If any of the sextets are equal to 64, we have an invalid char.  Test this by checking if the relevant bit (6) is set.
		if((sextet_a | sextet_b | sextet_c | sextet_d) & 0x40)
			throw glare::Exception("Invalid character");

		// Combine them together into a 24 bit value
		uint32 triple = (sextet_a << 3 * 6) | (sextet_b << 2 * 6) | (sextet_c << 1 * 6) | (sextet_d << 0 * 6);

		// Break into 3 bytes
		assert(j + 2 < output_length);
		data_out[j + 0] = (triple >> 2 * 8) & 0xFF;
		data_out[j + 1] = (triple >> 1 * 8) & 0xFF;
		data_out[j + 2] = (triple >> 0 * 8) & 0xFF;
	}

	// Handle last block, which may have '=' padding characters in it.
	{
		size_t i = last_input_block_index;
		size_t j = last_input_block_index / 4 * 3;
		assert(i + 3 < input_length);
		uint32 sextet_a = base64_decoding_table[input[i + 0]];
		uint32 sextet_b = base64_decoding_table[input[i + 1]];
		uint32 sextet_c = s[i + 2] == '=' ? 0 : base64_decoding_table[input[i + 2]];
		uint32 sextet_d = s[i + 3] == '=' ? 0 : base64_decoding_table[input[i + 3]];

		if((sextet_a | sextet_b | sextet_c | sextet_d) & 0x40)
			throw glare::Exception("Invalid character");

		uint32 triple = (sextet_a << 3 * 6) | (sextet_b << 2 * 6) | (sextet_c << 1 * 6) | (sextet_d << 0 * 6);

		if(j < output_length) data_out[j++] = (triple >> 2 * 8) & 0xFF;
		if(j < output_length) data_out[j++] = (triple >> 1 * 8) & 0xFF;
		if(j < output_length) data_out[j++] = (triple >> 0 * 8) & 0xFF;
	}
}


#if BUILD_TESTS


} // End namespace Base64


#include "../utils/TestUtils.h"
#include "../maths/PCG32.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"


namespace Base64
{


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


static void doDecodeEncodeTest(const std::string& encoded)
{
	// Decode
	std::vector<unsigned char> decoded;
	Base64::decode(encoded, decoded);

	// Re-encode
	std::string reencoded;
	Base64::encode(&decoded[0], decoded.size(), reencoded);

	// Check reencoded == encoded
	testAssert(encoded.size() == reencoded.size());
	for(size_t i=0; i<encoded.size(); ++i)
		testAssert(encoded[i] == reencoded[i]);
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
	catch(glare::Exception& )
	{
	}
}


void test()
{
	conPrint("Base64::test()");

	static_assert(staticArrayNumElems(base64_encoding_table) == 64, "staticArrayNumElems(base64_encoding_table) == 64");
	static_assert(staticArrayNumElems(base64_decoding_table) == 256, "staticArrayNumElems(base64_decoding_table) == 256");

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


	/*{
		std::string res;
		encode(NULL, 0, res);
		testAssert(res == "");
	}*/

	// Test empty string.  Should encode to empty string.
	{
		const std::string s = "";
		doTest(s, "");
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

	// Invalid length (non multiple of 4)
	testDecodingFailure("a");
	testDecodingFailure("aa");
	testDecodingFailure("aaa");
	testDecodingFailure("aaaaa");

	// Test invalid characters
	testDecodingFailure("!aaa");
	testDecodingFailure("a!aa");
	testDecodingFailure("aa!a");
	testDecodingFailure("aaa!");

	// Test all padding chars
	testDecodingFailure("====");

	// Test padding char in index 1.
	testDecodingFailure("A===");

	// Test negative character values.  Need to test since std::string usually has a signed char type.
	testDecodingFailure("\255aaa");

	// Test decoding strings of all possible characters.
	{
		for(int i = (int)std::numeric_limits<char>::min(); i <= (int)std::numeric_limits<char>::max(); i++)
		{
			const char c = (char)i;
			std::string encoded;
			for(int z=0; z<4; ++z)
				encoded.push_back(c);

			testAssert(encoded.size() == 4);

			if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '+') || (c == '/'))
			{
				// c is a valid char for base 64
				doDecodeEncodeTest(encoded);
			}
			else
			{
				testDecodingFailure(encoded);
			}
		}
	}

	// Perf test
	if(true)
	{
		PCG32 rng(1);
		std::vector<unsigned char> plaintext((int)1.0e6);
		for(size_t i = 0; i < plaintext.size(); i++)
			plaintext[i] = (unsigned char)(rng.unitRandom() * 255.9);

		std::vector<unsigned char> decoded;
		std::string encoded;
		Timer encode_timer, decode_timer;
		encode_timer.pause();
		decode_timer.pause();

		const int iters = 10;
		for(int i=0; i<iters; ++i)
		{
			encode_timer.unpause();
			encode(plaintext.data(), plaintext.size(), encoded);
			encode_timer.pause();

			decode_timer.unpause();
			decode(encoded, decoded);
			decode_timer.pause();

			testAssert(plaintext == decoded);
		}

		const double encode_elapsed = encode_timer.elapsed() / iters;
		const double decode_elapsed = decode_timer.elapsed() / iters;
		const double encode_speed = plaintext.size() / encode_elapsed;
		const double decode_speed = plaintext.size() / decode_elapsed;
		conPrint("Encode elapsed: " + toString(encode_elapsed) + " s (" + toString(encode_speed * 1.0e-6) + " MB/s");
		conPrint("Decode elapsed: " + toString(decode_elapsed) + " s (" + toString(decode_speed * 1.0e-6) + " MB/s");
	}


	// Fuzz test encoder and decoder
#ifdef NDEBUG
	const int fuzz_iters = (int)1.0e4; // Non-debug mode
#else
	const int fuzz_iters = (int)1.0e3; // Debug mode
#endif
	if(true)
	{
		conPrint("Fuzz testing encoder (" + toString(fuzz_iters) + " iters)...");
		PCG32 rng(1);
		size_t sum = 0;
		for(int z=0; z<fuzz_iters; ++z)
		{
			const size_t input_len = (size_t)(rng.unitRandom() * 1000);
			std::vector<unsigned char> plaintext(input_len);
			for(size_t i = 0; i < input_len; i++)
				plaintext[i] = (unsigned char)(rng.unitRandom() * 255.9);

			std::string encoded;
			encode(plaintext.data(), plaintext.size(), encoded);
			sum += encoded.size();
		}
		printVar(sum);
	}
	
	if(true)
	{
		conPrint("Fuzz testing decoder (" + toString(fuzz_iters) + " iters)...");
		PCG32 rng(1);
		size_t sum = 0;
		for(int z=0; z<fuzz_iters; ++z)
		{
			const size_t encoded_len = (size_t)(rng.unitRandom() * 40);

			std::string encoded(encoded_len, 'a');

			if(rng.unitRandom() < 0.75)
			{
				for(size_t i = 0; i < encoded_len; i++)
					encoded[i] = base64_encoding_table[(int)(rng.unitRandom() * 63.99)];

				while(rng.unitRandom() < 0.5)
					encoded.push_back('=');
			}
			else
			{
				for(size_t i = 0; i < encoded_len; i++)
					encoded[i] = (char)(-128 + rng.unitRandom() * 255.9);
			}

			std::vector<unsigned char> decoded;
			try
			{
				decode(encoded, decoded);
			}
			catch(glare::Exception&)
			{}
			sum += decoded.size();
		}
		printVar(sum);
	}

	conPrint("Base64::test() done.");
}


#endif // BUILD_TESTS


} // End namespace Base64
