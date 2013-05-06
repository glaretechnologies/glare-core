/*=====================================================================
AESEncryption.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-09-08 13:18:33 +0100
=====================================================================*/
#include "AESEncryption.h"


#include "Exception.h"
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <assert.h>


// Create an 256 bit key and IV using the supplied key_data. salt can be added for taste.
AESEncryption::AESEncryption(const unsigned char* key_data, int key_data_len, const unsigned char* salt)
:	encrypt_context(NULL),
	decrypt_context(NULL)
{
	encrypt_context = new EVP_CIPHER_CTX;
	decrypt_context = new EVP_CIPHER_CTX;

	const int num_rounds = 5;
	unsigned char key[32], iv[32];

	
	// Gen key & IV for AES 256 CBC mode. A SHA1 digest is used to hash the supplied key material.
	// num_rounds is the number of times the we hash the material. More rounds are more secure but
	// slower.
	int i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), salt, key_data, key_data_len, num_rounds, key, iv);
	if(i != 32)
	{
		EVP_CIPHER_CTX_cleanup(encrypt_context);
		EVP_CIPHER_CTX_cleanup(decrypt_context);
		delete encrypt_context; encrypt_context = NULL;
		delete decrypt_context; decrypt_context = NULL;
		throw Indigo::Exception("Invalid key size");
	}

	EVP_CIPHER_CTX_init(encrypt_context); // void return
	if(!EVP_EncryptInit_ex(encrypt_context, EVP_aes_256_cbc(), NULL, key, iv))
		throw Indigo::Exception("EVP_EncryptInit_ex failed.");
	EVP_CIPHER_CTX_init(decrypt_context); // void return
	if(!EVP_DecryptInit_ex(decrypt_context, EVP_aes_256_cbc(), NULL, key, iv))
		throw Indigo::Exception("EVP_DecryptInit_ex failed.");
}


AESEncryption::~AESEncryption()
{
	EVP_CIPHER_CTX_cleanup(encrypt_context);
	EVP_CIPHER_CTX_cleanup(decrypt_context);
	delete encrypt_context;
	delete decrypt_context;
}


// Encrypt plaintext.
std::vector<unsigned char> AESEncryption::encrypt(const std::vector<unsigned char>& plaintext)
{
	if(plaintext.empty())
		throw Indigo::Exception("plaintext cannot be empty.");

	// Max ciphertext len for a n bytes of plaintext is n + AES_BLOCK_SIZE -1 bytes
	int ciphertext_len = (int)plaintext.size() + AES_BLOCK_SIZE;

	std::vector<unsigned char> ciphertext(ciphertext_len);

	if(!EVP_EncryptInit_ex(encrypt_context, NULL, NULL, NULL, NULL))
		throw Indigo::Exception("EVP_EncryptInit_ex failed.");

	// Update ciphertext, ciphertext_len is filled with the length of ciphertext generated,
	if(!EVP_EncryptUpdate(encrypt_context, &ciphertext[0], &ciphertext_len, &plaintext[0], (int)plaintext.size()))
		throw Indigo::Exception("EVP_EncryptUpdate failed.");

	// Update ciphertext with the final remaining bytes
	int final_len = 0;
	if(!EVP_EncryptFinal_ex(encrypt_context, &ciphertext[0] + ciphertext_len, &final_len))
		throw Indigo::Exception("EVP_EncryptFinal_ex failed.");

	int len = ciphertext_len + final_len;
	assert(len <= (int)ciphertext.size());
	ciphertext.resize(len);
	return ciphertext;
}


// Decrypt ciphertext
std::vector<unsigned char> AESEncryption::decrypt(const std::vector<unsigned char>& ciphertext)
{
	if(ciphertext.empty())
		throw Indigo::Exception("ciphertext cannot be empty.");

	int plaintext_len = (int)ciphertext.size() + AES_BLOCK_SIZE;
	std::vector<unsigned char> plaintext(plaintext_len);

	if(!EVP_DecryptInit_ex(decrypt_context, NULL, NULL, NULL, NULL))
		throw Indigo::Exception("EVP_DecryptInit_ex failed.");

	if(!EVP_DecryptUpdate(decrypt_context, &plaintext[0], &plaintext_len, &ciphertext[0], (int)ciphertext.size()))
		throw Indigo::Exception("EVP_DecryptUpdate failed.");

	int final_len = 0;
	
	if(!EVP_DecryptFinal_ex(decrypt_context, &plaintext[0] + plaintext_len, &final_len))
		throw Indigo::Exception("EVP_DecryptFinal_ex failed.");

	int len = plaintext_len + final_len;
	assert(len <= (int)plaintext.size());
	plaintext.resize(len);
	return plaintext;
}


#if BUILD_TESTS


#include "../utils/stringutils.h"
#include "../utils/timer.h"
#include "../maths/mathstypes.h"
#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"


static void testWithPlaintextLength(int N)
{
	const std::string key = "BLEH";
	const std::string salt = "bh6ibytu";
	Timer timer;
	AESEncryption aes((const unsigned char*)key.c_str(), (int)key.size(), (const unsigned char*)salt.c_str());

	conPrint("AESEncryption construction took " + timer.elapsedString());

	std::vector<unsigned char> plaintext(N);
	for(int i=0; i<N; ++i)
		plaintext[i] = i % 256;

	// Encrypt
	timer.reset();

	std::vector<unsigned char> cyphertext = aes.encrypt(plaintext);

	conPrint("encrypt took " + timer.elapsedString());
	conPrint("cyphertext len: " + toString((int64)cyphertext.size()));
	conPrint("cyphertext extract: ");
	for(int i=0; i<myMin(64, (int)cyphertext.size()); ++i)
	{
		conPrintStr(int32ToString(cyphertext[i]) + " ");
		if(i % 16 == 15)
			conPrint("");
	}
	conPrint("");


	// Decrypt
	timer.reset();

	std::vector<unsigned char> decrypted_plaintext = aes.decrypt(cyphertext);

	conPrint("decrypt took " + timer.elapsedString());

	// Check encryption and decryption process preserved the data.
	testAssert(decrypted_plaintext == plaintext);
}


void AESEncryption::test()
{
	testWithPlaintextLength(1);
	testWithPlaintextLength(2);
	testWithPlaintextLength(100);
	testWithPlaintextLength(256);
	testWithPlaintextLength(10000);
	testWithPlaintextLength(256000);
}


#endif

