/*=====================================================================
CheckDLLSignature.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed May 26 14:52:09 +1200 2010
=====================================================================*/
#pragma once

#include <string>


/*=====================================================================
CheckDLLSignature
-------------------

=====================================================================*/
class CheckDLLSignature
{
public:
	CheckDLLSignature();
	~CheckDLLSignature();

	static bool verifyDLL(std::string dll_path, std::string expected_sig);

private:

};
