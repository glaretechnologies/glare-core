/*=====================================================================
KeyPairGen.cpp
--------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "KeyPairGen.h"


#include "Platform.h"
#include "Exception.h"
#include "FileHandle.h"
#include "ConPrint.h"
#include <openssl/opensslv.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>


namespace KeyPairGen
{


EVP_PKEY* generateRSAKey(int bits)
{
	RSA* rsa = RSA_new();
	BIGNUM* e  = BN_new();
 
	if(!rsa || !e)
		throw glare::Exception("RSA_new / BN_new failed");
 
	if(!BN_set_word(e, RSA_F4))
		throw glare::Exception("BN_set_word failed");
 
	if(RSA_generate_key_ex(rsa, bits, e, NULL) != 1)
		throw glare::Exception("RSA_generate_key_ex failed");
 
	EVP_PKEY* pkey = EVP_PKEY_new();
	if(!pkey || !EVP_PKEY_assign_RSA(pkey, rsa))
	{
		RSA_free(rsa);
		throw glare::Exception("EVP_PKEY_assign_RSA failed");
	}
	// rsa is now owned by pkey — don't free it separately
 
	BN_free(e);
	return pkey;
}


//
//  Build a self-signed X.509 certificate
//
void addSubjectEntry(X509_NAME *name, const char *field, const char *value)
{
	if(!X509_NAME_add_entry_by_txt(name, field, MBSTRING_ASC, (const unsigned char *)value, -1, -1, 0))
		throw glare::Exception("X509_NAME_add_entry_by_txt failed");
}
 
X509* createSelfSignedCert(EVP_PKEY *pkey, int days,
									 const char *cn, const char *org,
									 const char *country)
{
	X509* cert = X509_new();
	if(!cert)
		throw glare::Exception("X509_new failed");
 
	// Version 3 (0-indexed, so 2 = v3)
	if(!X509_set_version(cert, 2))
		throw glare::Exception("X509_set_version failed");
 
	// Serial number
	const int SERIAL_NUM = 1;
	ASN1_INTEGER_set(X509_get_serialNumber(cert), SERIAL_NUM);
 
	// Validity period
	X509_gmtime_adj(X509_get_notBefore(cert), 0);
	X509_gmtime_adj(X509_get_notAfter(cert), (long)days * 86400);
 
	// Public key
	if(!X509_set_pubkey(cert, pkey))
		throw glare::Exception("X509_set_pubkey failed");
 
	// Subject = Issuer (self-signed)
	X509_NAME* name = X509_get_subject_name(cert);
	if(country && *country)  addSubjectEntry(name, "C",  country);
	if(org     && *org)      addSubjectEntry(name, "O",  org);
	if(cn      && *cn)       addSubjectEntry(name, "CN", cn);
	X509_set_issuer_name(cert, name);
 
	// Extensions (optional but good practice)
	X509V3_CTX v3ctx;
	X509V3_set_ctx_nodb(&v3ctx);
	X509V3_set_ctx(&v3ctx, cert, cert, NULL, NULL, 0);
 
	X509_EXTENSION *ext;
 
	ext = X509V3_EXT_conf_nid(NULL, &v3ctx, NID_basic_constraints, "critical,CA:TRUE");
	if(ext) { X509_add_ext(cert, ext, -1); X509_EXTENSION_free(ext); }
 
	ext = X509V3_EXT_conf_nid(NULL, &v3ctx, NID_subject_key_identifier, "hash");
	if(ext) { X509_add_ext(cert, ext, -1); X509_EXTENSION_free(ext); }
 
	ext = X509V3_EXT_conf_nid(NULL, &v3ctx, NID_authority_key_identifier, "keyid:always");
	if(ext) { X509_add_ext(cert, ext, -1); X509_EXTENSION_free(ext); }
 
	// Sign with SHA-256
	if(!X509_sign(cert, pkey, EVP_sha256()))
		throw glare::Exception("X509_sign failed");
 
	return cert;
}


void generateRSAKeyPairAndX509Cert(const std::string& priv_key_path, const std::string& public_key_path, const std::string& cert_file_path)
{
	//------------------- Create RSA keypair -------------------
	const int KEY_BITS = 2048;
	EVP_PKEY* pkey = generateRSAKey(KEY_BITS);

	//------------------- Create self-signed certificate -------------------
	const int CERT_DAYS = 3650;
	printf("Creating self-signed certificate (%d days)...\n", CERT_DAYS);
	X509* cert = createSelfSignedCert(
		pkey,
		CERT_DAYS,
		"localhost",        // Common Name
		"My Organisation",  // Organisation
		"NZ"                // Country
	);

	//------------------- Write private key (PEM) -------------------
	{
		FileHandle file(priv_key_path, "wb");
 
		if(!PEM_write_PrivateKey(file.getFile(), pkey, NULL, NULL, 0, NULL, NULL))
			throw glare::Exception("PEM_write_PrivateKey failed");
		conPrint("Private key written to '" + priv_key_path + "'.");
	}

	//------------------- Write public key (PEM) -------------------
	{
		FileHandle file(public_key_path, "wb");
 
		if(!PEM_write_PUBKEY(file.getFile(), pkey))
			throw glare::Exception("PEM_write_PUBKEY failed");
		conPrint("Public key written to '" + public_key_path + "'.");
	}

	//------------------- Write certificate -------------------
	{
		FileHandle file(cert_file_path, "wb");

		if(!PEM_write_X509(file.getFile(), cert))
			throw glare::Exception("PEM_write_X509 failed");
		conPrint("Certificate written to '" + cert_file_path + "'.");
	}

 
	// Cleanup
	X509_free(cert);
	EVP_PKEY_free(pkey);
 
	conPrint("Done.");
}


} // end namespace KeyPairGen


#if BUILD_TESTS


#include "StringUtils.h"
#include "TestUtils.h"


void KeyPairGen::test()
{
	try
	{
		KeyPairGen::generateRSAKeyPairAndX509Cert("test.priv", "test.pub", "test.cert");
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


#endif // BUILD_TESTS
