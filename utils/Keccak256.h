/*=====================================================================
Keccak256.h
-----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <string>


/*=====================================================================
Keccak256
---------
Keccak hash with 256 bit digest
=====================================================================*/
namespace Keccak256
{
	// digest_out should be a pointer to a 32 byte buffer.
	// NOTE: require input_str.size() < 65536 currently.
	// Throws glare::Exception if that is not the case.
	void hash(const std::string& input_str, uint8* digest_out);

	void test();
};
