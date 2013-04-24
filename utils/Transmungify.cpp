/*=====================================================================
Transmungify.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Sun May 30 21:03:25 +1200 2010
=====================================================================*/
#include "Transmungify.h"


#include "Exception.h"


// Some magic numbers...
const static int permute[4] = { 3, 1, 0, 2 };
const static uint32 char_offsets[4] = { 177, 230, 229, 160 }; //{ -79, 230, -27, 160 };
const static uint32 magic0 = 0xB77E0246; // random, nothing special
const static uint32 magic1 = 0x2E479BAA; // random, nothing special


Transmungify::Transmungify()
{

}


Transmungify::~Transmungify()
{

}


static inline unsigned char toUChar(uint32 x)
{
	return (unsigned char)(x % 256);
}


bool Transmungify::encrypt(const std::string& src_string, std::vector<uint32>& dst_dwords)
{
	return encrypt(src_string.c_str(), (uint32)src_string.size(), dst_dwords);
}


bool Transmungify::encrypt(const char* src_string, uint32 src_string_size, std::vector<uint32>& dst_dwords)
{
	uint32 i;
	const uint32 string_len = src_string_size; // (uint32)src_string.size();
	const uint32 encrypted_int32s = (string_len + 3) / 4;
	const char* src_str = src_string; // src_string.c_str();

	dst_dwords.resize(encrypted_int32s + 1);
	dst_dwords[encrypted_int32s] = (string_len ^ magic0) + magic1;

	// Create padded string
	std::vector<unsigned char> padded_string(encrypted_int32s * 4);
	for(i = 0; i < string_len; ++i)			
		padded_string[i] = src_str[i];
	for(; i < encrypted_int32s * 4; ++i)	
		padded_string[i] = src_str[(((i + 1) * string_len / 4) + string_len) % string_len]; // Use text from start (wrap around)

	// create encrypted data
	for(i = 0; i < encrypted_int32s; ++i)
	{
		// permute order, offset with wraparound
		unsigned char chars[4] =
		{
			toUChar(padded_string[i * 4 + permute[0]] + char_offsets[0]),
			toUChar(padded_string[i * 4 + permute[1]] + char_offsets[1]),
			toUChar(padded_string[i * 4 + permute[2]] + char_offsets[2]),
			toUChar(padded_string[i * 4 + permute[3]] + char_offsets[3])
		};

		// pack chars into single dword
		uint32 dword_value = (chars[0] << 0) + (chars[1] << 8) + (chars[2] << 16) + (chars[3] << 24);

		// xor with 0xDEADBEEF
		dword_value ^= magic0;

		// offset by 0xC0FFEE with wraparound
		dword_value += magic1;

		dst_dwords[i] = dword_value;
	}

	return true;
}


bool Transmungify::decrypt(const std::vector<uint32>& src_dwords, std::string& dst_string)
{
	return decrypt(&src_dwords[0], (uint32)src_dwords.size(), dst_string);
}


bool Transmungify::decrypt(const uint32* src_dwords, uint32 src_dwords_count, std::string& dst_string)
{
	const uint32 string_len = (src_dwords[src_dwords_count - 1] - magic1) ^ magic0;

	if(string_len > src_dwords_count * 4)
		throw Indigo::Exception("String decode error");

	dst_string.resize(string_len + 3);
	for(uint32 i = 0; i < (string_len + 3) / 4; ++i)
	{
		const uint32 dword_value = (src_dwords[i] - magic1) ^ magic0;
		unsigned char chars[4] =
		{
			toUChar((dword_value & 0x000000FF) >>  0), 
			toUChar((dword_value & 0x0000FF00) >>  8),
			toUChar((dword_value & 0x00FF0000) >> 16), 
			toUChar((dword_value & 0xFF000000) >> 24)
		};

		dst_string[i * 4 + 0] = (chars[permute[3]] - char_offsets[2]);
		dst_string[i * 4 + 1] = (chars[permute[1]] - char_offsets[1]);
		dst_string[i * 4 + 2] = (chars[permute[0]] - char_offsets[3]);
		dst_string[i * 4 + 3] = (chars[permute[2]] - char_offsets[0]);
	}
	dst_string.erase(string_len, dst_string.size() - string_len);

	return true;
}


#if (BUILD_TESTS)


#include "../indigo/TestUtils.h"
#include <stdlib.h>


void Transmungify::test()
{
	std::string program_string("uint32 dword_value = (chars[0] <<  0) + (chars[1] <<  8) + (chars[2] << 16) + (chars[3] << 24);");

	std::vector<unsigned int> munged_program_string;
	encrypt(program_string, munged_program_string);

	std::string unmunged_program_string;
	if(!decrypt(munged_program_string, unmunged_program_string))
	{
		failTest("error decrypting string");
	}

	if(unmunged_program_string != program_string)
	{
		failTest("original and unmunged strings don't match!");
	}

	srand(1337);

	for(uint32 size = 0; size < 64; ++size)
	{
		std::string orig_string;

		for(uint32 i = 0; i < size; ++i)
			orig_string += program_string[rand() % program_string.size()];

		std::vector<uint32> encrypted_dwords;
		encrypt(orig_string, encrypted_dwords);

		std::string decrypted_string;
		decrypt(encrypted_dwords, decrypted_string);

		testAssert(decrypted_string == orig_string);
	}
}

#endif
