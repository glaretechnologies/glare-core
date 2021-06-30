/*=====================================================================
SHA256.h
--------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "ArrayRef.h"
#include <vector>
#include <string>


/*=====================================================================
SHA256
------
See http://en.wikipedia.org/wiki/SHA-2 for more info on SHA-256.
=====================================================================*/
class SHA256
{
public:
	SHA256();
	~SHA256();

	// Throws glare::Exception on failure
	inline static void hash(
		const std::vector<unsigned char>& message_text,
		std::vector<unsigned char>& digest_out
	)
	{
		if(message_text.empty())
			hash(0, 0, digest_out);
		else
			hash(&(*message_text.begin()), &(*message_text.begin()) + message_text.size(), digest_out);
	}

	// Throws glare::Exception on failure
	inline static void hash(
		const std::string& message_text,
		std::vector<unsigned char>& digest_out
	)
	{
		if(message_text.empty())
			hash(0, 0, digest_out);
		else
			hash((const unsigned char*)&(*message_text.begin()), (const unsigned char*)&(*message_text.begin()) + message_text.size(), digest_out);
	}

	// Return vector
	inline static std::vector<unsigned char> hash(const std::string& message_text)
	{
		std::vector<unsigned char> digest;
		if(message_text.empty())
			hash(0, 0, digest);
		else
			hash((const unsigned char*)&(*message_text.begin()), (const unsigned char*)&(*message_text.begin()) + message_text.size(), digest);
		return digest;
	}

	// Throws glare::Exception on failure
	static void hash(
		const unsigned char* message_text_begin,
		const unsigned char* message_text_end,
		std::vector<unsigned char>& digest_out
	);


	// Throws glare::Exception on failure
	static void SHA1Hash(
		const unsigned char* message_text_begin,
		const unsigned char* message_text_end,
		std::vector<unsigned char>& digest_out
	);


	static void SHA256HMAC(
		const ArrayRef<unsigned char>& key,
		const ArrayRef<unsigned char>& message,
		std::vector<unsigned char>& digest_out
	);


	static void test();
};
