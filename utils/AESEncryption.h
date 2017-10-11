/*=====================================================================
AESEncryption.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-09-08 13:18:33 +0100
=====================================================================*/
#pragma once


#include "Platform.h"
#include "ArrayRef.h"
#include <vector>
#include <string>
struct evp_cipher_ctx_st;


/*=====================================================================
AESEncryption
-------------------
Based off the code at http://saju.net.in/code/misc/openssl_aes.c.txt
=====================================================================*/
class AESEncryption
{
public:
	// 'key_data' is the password, which can be any binary buffer.
	// 'salt' must be a string with at least 8 chars (bytes).
	AESEncryption(const std::string& key_data, const std::string& salt);
	~AESEncryption();

	std::vector<unsigned char> encrypt(const ArrayRef<unsigned char>& plaintext);
	std::vector<unsigned char> decrypt(const ArrayRef<unsigned char>& ciphertext);

	static void test();
private:
	INDIGO_DISABLE_COPY(AESEncryption);

	// struct evp_cipher_ctx_st = EVP_CIPHER_CTX
	struct evp_cipher_ctx_st *encrypt_context;
	struct evp_cipher_ctx_st *decrypt_context;

	unsigned char key[32], iv[32];
};
