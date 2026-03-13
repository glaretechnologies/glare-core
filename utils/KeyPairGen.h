/*=====================================================================
KeyPairGen.h
------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include <string>


/*=====================================================================
KeyPairGen
----------
=====================================================================*/
namespace KeyPairGen
{
	// Create a RSA keypair and a self-signed X.509 Certificate
	// Writes them to disk at the given paths.
	void generateRSAKeyPairAndX509Cert(const std::string& priv_key_path, const std::string& public_key_path, const std::string& cert_file_path);

	void test();
};
