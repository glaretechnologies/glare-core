/*=====================================================================
x509Certificate.h
-----------------
Copyright Glare Technologies Limited 2011 -
Generated at Fri Nov 11 13:48:01 +0000 2011
=====================================================================*/
#pragma once

#include <string>


/*=====================================================================
x509Certificate
---------------

=====================================================================*/
class x509Certificate
{
public:
	static bool verifyCertificate(const std::string& subject, const std::string& public_key_string);

	static void test();

private:

};
