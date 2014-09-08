#if 0

/*=====================================================================
X509Certificate.h
-----------------
Copyright Glare Technologies Limited 2011 -
Generated at Fri Nov 11 13:48:01 +0000 2011
=====================================================================*/
#pragma once


#include <string>


/*=====================================================================
X509Certificate
---------------

=====================================================================*/
class X509Certificate
{
public:
	static bool verifyCertificate(const std::string& store, const std::string& subject, const std::string& public_key_string);

	// Enumerates all certificates in the given store
	static void enumCertificates(const std::string& store);

	static void test();

private:

};

#endif
