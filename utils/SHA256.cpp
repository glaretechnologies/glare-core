/*=====================================================================
SHA256.cpp
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-04-14 18:38:43 +0100
=====================================================================*/
#include "SHA256.h"


#include "Exception.h"
#include <openssl/sha.h>
#include <assert.h>


SHA256::SHA256()
{

}


SHA256::~SHA256()
{

}


void SHA256::hash(
		const unsigned char* message_text_begin,
		const unsigned char* message_text_end,
		std::vector<unsigned char>& digest_out
	)
{
	SHA256_CTX context;
	digest_out.resize(SHA256_DIGEST_LENGTH);

	if(SHA256_Init(&context) == 0)
		throw Indigo::Exception("Hash init failed");

	if(message_text_end > message_text_begin)
		if(SHA256_Update(&context, message_text_begin, message_text_end - message_text_begin) == 0)
			throw Indigo::Exception("Hash update failed");

	if(SHA256_Final(&digest_out[0], &context) == 0)
		throw Indigo::Exception("Hash finalise failed");
}


void SHA256::SHA1Hash(
		const unsigned char* message_text_begin,
		const unsigned char* message_text_end,
		std::vector<unsigned char>& digest_out
	)
{
	SHA_CTX context;
	digest_out.resize(SHA_DIGEST_LENGTH);

	if(SHA1_Init(&context) == 0)
		throw Indigo::Exception("Hash init failed");

	if(message_text_end > message_text_begin)
		if(SHA1_Update(&context, message_text_begin, message_text_end - message_text_begin) == 0)
			throw Indigo::Exception("Hash update failed");

	if(SHA1_Final(&digest_out[0], &context) == 0)
		throw Indigo::Exception("Hash finalise failed");
}


#if BUILD_TESTS


#include "../utils/stringutils.h"
#include "../utils/timer.h"
#include "../maths/mathstypes.h"
#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"


static std::vector<unsigned char> stringToByteArray(const std::string& s)
{
	std::vector<unsigned char> bytes(s.size());
	for(size_t i=0; i<s.size(); ++i)
		bytes[i] = s[i];
	return bytes;
}


static std::vector<unsigned char> hexToByteArray(const std::string& s)
{
	std::vector<unsigned char> bytes(s.size() / 2);
	for(size_t i=0; i<s.size()/2; ++i)
	{
		unsigned int nibble_a = hexCharToUInt(s[i*2]);
		unsigned int nibble_b = hexCharToUInt(s[i*2 + 1]);
		bytes[i] = (unsigned char)((nibble_a << 4) + nibble_b);
	}
	return bytes;
}


void SHA256::test()
{
	// Test against example hashes on wikipedia (http://en.wikipedia.org/wiki/SHA-2)

	// Test empty message
	{
		const std::vector<unsigned char> message;
		std::vector<unsigned char> digest;
		hash(message, digest);

		const std::string target_hex = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
		const std::vector<unsigned char> target = hexToByteArray(target_hex);
		testAssert(digest == target);
	}

	// Test "The quick brown fox jumps over the lazy dog"
	{
		const std::vector<unsigned char> message = stringToByteArray("The quick brown fox jumps over the lazy dog");
		std::vector<unsigned char> digest;
		hash(message, digest);

		const std::string target_hex = "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592";
		const std::vector<unsigned char> target = hexToByteArray(target_hex);
		testAssert(digest == target);
	}

	// Test "The quick brown fox jumps over the lazy dog."
	{
		const std::vector<unsigned char> message = stringToByteArray("The quick brown fox jumps over the lazy dog.");
		std::vector<unsigned char> digest;
		hash(message, digest);

		const std::string target_hex = "ef537f25c895bfa782526529a9b63d97aa631564d5d789c2b765448c8635fb6c";
		const std::vector<unsigned char> target = hexToByteArray(target_hex);
		testAssert(digest == target);
	}

	// Do a negative test
	{
		const std::vector<unsigned char> message = stringToByteArray("AAAA The quick brown fox jumps over the lazy dog.");
		std::vector<unsigned char> digest;
		hash(message, digest);

		const std::string target_hex = "ef537f25c895bfa782526529a9b63d97aa631564d5d789c2b765448c8635fb6c";
		const std::vector<unsigned char> target = hexToByteArray(target_hex);
		testAssert(digest != target);
	}

}


#endif
