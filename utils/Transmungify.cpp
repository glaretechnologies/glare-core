/*=====================================================================
Transmungify.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Sun May 30 21:03:25 +1200 2010
=====================================================================*/
#include "Transmungify.h"


Transmungify::Transmungify()
{

}


Transmungify::~Transmungify()
{

}


bool Transmungify::encrypt(const std::string& src_string, std::vector<unsigned int>& dst_dwords)
{
	unsigned int i, string_len = src_string.size(), encrypted_int32s = (string_len + 3) / 4;
	const char* src_str = src_string.c_str();

	dst_dwords.resize(encrypted_int32s);

	// Create padded string
	unsigned char* padded_string = new unsigned char[encrypted_int32s * 4];
	for(i = 0; i < string_len; ++i)			padded_string[i] = src_str[i];
	for(; i < encrypted_int32s * 4; ++i)	padded_string[i] = src_str[i - string_len]; // Use text from start (wrap around)

	// create encrypted data
	for(i = 0; i < encrypted_int32s; ++i)
	{
		// permute order, offset with wraparound
		unsigned char chars[4] =
		{
			padded_string[i * 4 + permute[0]] + char_offsets[0],
			padded_string[i * 4 + permute[1]] + char_offsets[1],
			padded_string[i * 4 + permute[2]] + char_offsets[2],
			padded_string[i * 4 + permute[3]] + char_offsets[3]
		};

		// pack chars into single dword
		unsigned int dword_value = (chars[0] << 0) + (chars[1] << 8) + (chars[2] << 16) + (chars[3] << 24);

		// xor with 0xDEADBEEF
		dword_value ^= magic0;

		// offset by 0xC0FFEE with wraparound
		dword_value += magic1;

		dst_dwords[i] = dword_value;
	}

	delete padded_string;

	return true;
}


bool Transmungify::decrypt(const std::vector<unsigned int>& src_dwords, std::string& dst_string, int string_len)
{
	unsigned char* decrypted_src = new unsigned char[src_dwords.size() * 4 + 1];

	for(unsigned int i = 0; i < src_dwords.size(); ++i)
	{
		const unsigned int dword_value = (src_dwords[i] - magic1) ^ magic0;

		unsigned char chars[4] =
		{
			((dword_value & 0x000000FF) >>  0), ((dword_value & 0x0000FF00) >>  8),
			((dword_value & 0x00FF0000) >> 16), ((dword_value & 0xFF000000) >> 24)
		};

		decrypted_src[i * 4 + 0] = chars[permute[3]] - char_offsets[2];
		decrypted_src[i * 4 + 1] = chars[permute[1]] - char_offsets[1];
		decrypted_src[i * 4 + 2] = chars[permute[0]] - char_offsets[3];
		decrypted_src[i * 4 + 3] = chars[permute[2]] - char_offsets[0];
	}

	// terminate string
	decrypted_src[string_len] = 0;
	dst_string = (char *)decrypted_src;
	delete decrypted_src;

	return true;
}


bool Transmungify::decrypt(const unsigned int* src_dwords, std::string& dst_string, int string_len)
{
	unsigned int encrypted_int32s = (string_len + 3) / 4;
	unsigned char* decrypted_src = new unsigned char[encrypted_int32s * 4 + 1];

	for(unsigned int i = 0; i < encrypted_int32s; ++i)
	{
		const unsigned int dword_value = (src_dwords[i] - magic1) ^ magic0;

		unsigned char chars[4] =
		{
			((dword_value & 0x000000FF) >>  0), ((dword_value & 0x0000FF00) >>  8),
			((dword_value & 0x00FF0000) >> 16), ((dword_value & 0xFF000000) >> 24)
		};

		decrypted_src[i * 4 + 0] = chars[permute[3]] - char_offsets[2];
		decrypted_src[i * 4 + 1] = chars[permute[1]] - char_offsets[1];
		decrypted_src[i * 4 + 2] = chars[permute[0]] - char_offsets[3];
		decrypted_src[i * 4 + 3] = chars[permute[2]] - char_offsets[0];
	}

	// terminate string
	decrypted_src[string_len] = 0;
	dst_string = (char *)decrypted_src;
	delete decrypted_src;

	return true;
}
