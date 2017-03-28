/*=====================================================================
AESEncryption.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-09-08 13:18:33 +0100
=====================================================================*/
#pragma once


#include "Platform.h"
#include <vector>
struct evp_cipher_ctx_st;


/*=====================================================================
AESEncryption
-------------------
Based off the code at http://saju.net.in/code/misc/openssl_aes.c.txt
=====================================================================*/
class AESEncryption
{
public:
	// 'key_data' points to the password, which can be any binary buffer.
	// 'key_data_len' is the size of the key_data buffer in bytes.
	// 'salt' must point to an 8 byte buffer.
	AESEncryption(const unsigned char *key_data, int key_data_len, const unsigned char *salt);
	~AESEncryption();

	std::vector<unsigned char> encrypt(const std::vector<unsigned char>& plaintext);
	std::vector<unsigned char> decrypt(const std::vector<unsigned char>& ciphertext);

	static void test();
private:
	INDIGO_DISABLE_COPY(AESEncryption);

	// struct evp_cipher_ctx_st = EVP_CIPHER_CTX
	struct evp_cipher_ctx_st *encrypt_context;
	struct evp_cipher_ctx_st *decrypt_context;

	unsigned char key[32], iv[32];
};
