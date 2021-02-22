/*=====================================================================
CryptoRNG.h
-----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <stddef.h> // For size_t


/*=====================================================================
CryptoRNG
----------
Provides cryptographically secure random bytes - e.g. high quality random 
bytes that can't be reverse engineered.
Uses OS APIs.
=====================================================================*/
namespace CryptoRNG
{


// Fill buffer with random bytes.
// May block on Linux - https://man7.org/linux/man-pages/man2/getrandom.2.html
// May also fail on Linux if buflen > 256. 
void getRandomBytes(uint8* buf, size_t buflen); // Throws glare::Exception on failure.


void test();


}; // End namespace CryptoRNG
