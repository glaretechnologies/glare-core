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
AESEncryption::AESEncryption(const std::string& key_data, const std::string& salt)
:	encrypt_context(NULL),
	decrypt_context(NULL)
{
	if(salt.size() < 8)
		throw glare::Exception("Invalid salt size");

	encrypt_context = new EVP_CIPHER_CTX;
	decrypt_context = new EVP_CIPHER_CTX;

	const int num_rounds = 5;
	
	// Generate key & IV for AES 256 CBC mode. A SHA1 digest is used to hash the supplied key material.
	// num_rounds is the number of times the we hash the material. More rounds are more secure but
	// slower.
	int i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), (const unsigned char*)salt.c_str(), (const unsigned char*)key_data.c_str(), (int)key_data.size(), num_rounds, this->key, this->iv);
	if(i != 32)
	{
		EVP_CIPHER_CTX_cleanup(encrypt_context);
		EVP_CIPHER_CTX_cleanup(decrypt_context);
		delete encrypt_context; encrypt_context = NULL;
		delete decrypt_context; decrypt_context = NULL;
		throw glare::Exception("Invalid key size");
	}

	EVP_CIPHER_CTX_init(encrypt_context); // void return
	//if(!EVP_EncryptInit_ex(encrypt_context, EVP_aes_256_cbc(), NULL, key, iv))
	//	throw glare::Exception("EVP_EncryptInit_ex failed.");
	EVP_CIPHER_CTX_init(decrypt_context); // void return
	//if(!EVP_DecryptInit_ex(decrypt_context, EVP_aes_256_cbc(), NULL, key, iv))
	//	throw glare::Exception("EVP_DecryptInit_ex failed.");
}


AESEncryption::~AESEncryption()
{
	EVP_CIPHER_CTX_cleanup(encrypt_context);
	EVP_CIPHER_CTX_cleanup(decrypt_context);
	delete encrypt_context;
	delete decrypt_context;
}


// Encrypt plaintext.
std::vector<unsigned char> AESEncryption::encrypt(const ArrayRef<unsigned char>& plaintext)
{
	if(plaintext.empty())
		throw glare::Exception("plaintext cannot be empty.");

	// Max ciphertext len for a n bytes of plaintext is n + AES_BLOCK_SIZE -1 bytes
	int ciphertext_len = (int)plaintext.size() + AES_BLOCK_SIZE;

	std::vector<unsigned char> ciphertext(ciphertext_len);

	if(!EVP_EncryptInit_ex(encrypt_context, EVP_aes_256_cbc(), NULL, key, iv)) // Needed to reset state for CBC mode
		throw glare::Exception("EVP_EncryptInit_ex failed.");

	// Update ciphertext, ciphertext_len is filled with the length of ciphertext generated,
	if(!EVP_EncryptUpdate(encrypt_context, &ciphertext[0], &ciphertext_len, &plaintext[0], (int)plaintext.size()))
		throw glare::Exception("EVP_EncryptUpdate failed.");

	// Update ciphertext with the final remaining bytes
	int final_len = 0;
	if(!EVP_EncryptFinal_ex(encrypt_context, &ciphertext[0] + ciphertext_len, &final_len))
		throw glare::Exception("EVP_EncryptFinal_ex failed.");

	int len = ciphertext_len + final_len;
	assert(len <= (int)ciphertext.size());
	ciphertext.resize(len);
	return ciphertext;
}


// Decrypt ciphertext
std::vector<unsigned char> AESEncryption::decrypt(const ArrayRef<unsigned char>& ciphertext)
{
	if(ciphertext.empty())
		throw glare::Exception("ciphertext cannot be empty.");

	int plaintext_len = (int)ciphertext.size() + AES_BLOCK_SIZE;
	std::vector<unsigned char> plaintext(plaintext_len);

	if(!EVP_DecryptInit_ex(decrypt_context, EVP_aes_256_cbc(), NULL, key, iv)) // Needed to reset state for CBC mode
		throw glare::Exception("EVP_DecryptInit_ex failed.");

	if(!EVP_DecryptUpdate(decrypt_context, &plaintext[0], &plaintext_len, &ciphertext[0], (int)ciphertext.size()))
		throw glare::Exception("EVP_DecryptUpdate failed.");

	int final_len = 0;
	
	if(!EVP_DecryptFinal_ex(decrypt_context, &plaintext[0] + plaintext_len, &final_len))
		throw glare::Exception("EVP_DecryptFinal_ex failed.");

	int len = plaintext_len + final_len;
	assert(len <= (int)plaintext.size());
	plaintext.resize(len);
	return plaintext;
}


#if BUILD_TESTS


#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "../maths/mathstypes.h"
#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"


static void testWithPlaintextLength(int N)
{
	const std::string key = "BLEH";
	const std::string salt = "bh6ibytu";
	Timer timer;
	AESEncryption aes(key, salt);

	conPrint("Testing with plaintext of length " + toString(N) + "...");
	conPrint("AESEncryption construction took " + timer.elapsedString());

	std::vector<unsigned char> plaintext(N);
	for(int i=0; i<N; ++i)
		plaintext[i] = i % 256;

	// Encrypt
	timer.reset();

	std::vector<unsigned char> cyphertext = aes.encrypt(plaintext);

	const double elapsed = timer.elapsed();
	const double speed = N / elapsed;
	conPrint("encrypt took " + toString(elapsed) + " s, speed: " + toString(speed * 1.0e-6) + " MB/s");
	conPrint("cyphertext len: " + toString((int64)cyphertext.size()));
	conPrint("cyphertext extract: ");
	conPrint("-----------------");
	for(int i=0; i<myMin(64, (int)cyphertext.size()); ++i)
	{
		conPrintStr(int32ToString(cyphertext[i]) + " ");
		if(i % 16 == 15)
			conPrint("");
	}
	conPrint("-----------------");


	// Decrypt
	timer.reset();

	std::vector<unsigned char> decrypted_plaintext = aes.decrypt(cyphertext);

	conPrint("decrypt took " + timer.elapsedString());
	conPrint("");

	// Check encryption and decryption process preserved the data.
	testAssert(decrypted_plaintext == plaintext);

	// Execute again, check we get same results
	std::vector<unsigned char> cyphertext2 = aes.encrypt(plaintext);
	std::vector<unsigned char> decrypted_plaintext2 = aes.decrypt(cyphertext2);
	testAssert(decrypted_plaintext2 == plaintext);
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
