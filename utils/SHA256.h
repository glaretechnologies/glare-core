/*=====================================================================
SHA256.h
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-04-14 18:38:43 +0100
=====================================================================*/
#pragma once


#include <vector>
#include <string>


/*=====================================================================
SHA256
-------------------
See http://en.wikipedia.org/wiki/SHA-2 for more info on SHA-256.
=====================================================================*/
class SHA256
{
public:
	SHA256();
	~SHA256();

	// Throws Indigo::Exception on failure
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

	// Throws Indigo::Exception on failure
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

	// Throws Indigo::Exception on failure
	static void hash(
		const unsigned char* message_text_begin,
		const unsigned char* message_text_end,
		std::vector<unsigned char>& digest_out
	);


	// Throws Indigo::Exception on failure
	static void SHA1Hash(
		const unsigned char* message_text_begin,
		const unsigned char* message_text_end,
		std::vector<unsigned char>& digest_out
	);


	static void test();
};
