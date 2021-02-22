/*=====================================================================
CryptoRNG.cpp
-------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "CryptoRNG.h"


#include "Exception.h"
#include "PlatformUtils.h"
#ifdef _WIN32
#include <IncludeWindows.h>
#include <wincrypt.h>
#elif defined(OSX)
#include <Security/Security.h>
#else // Linux:
#include <sys/random.h>
#endif


namespace CryptoRNG
{


// Fill buffer with random bytes
void getRandomBytes(uint8* buf, size_t buflen)
{
	if(buflen == 0)
		return;

#ifdef _WIN32

	HCRYPTPROV provider;
	if(!CryptAcquireContext(
		&provider,
		NULL, // szContainer
		NULL, // szProvider - use default
		PROV_RSA_FULL, // dwProvType
		CRYPT_VERIFYCONTEXT // dwFlags - we do not require access to persisted private keys.
	)) 
		throw glare::Exception("CryptAcquireContext failed: " + PlatformUtils::getLastErrorString());

	if(!CryptGenRandom(
		provider, 
		(DWORD)buflen, // len
		buf
	)) 
		throw glare::Exception("CryptGenRandom failed: " + PlatformUtils::getLastErrorString());

	if(!CryptReleaseContext(provider, /*flags=*/0))
		throw glare::Exception("CryptReleaseContext failed: " + PlatformUtils::getLastErrorString());

#elif defined(OSX)

	const int res = SecRandomCopyBytes(kSecRandomDefault, buflen, buf);
	if(res != 0)
		throw glare::Exception("SecRandomCopyBytes failed: " + PlatformUtils::getLastErrorString()); 

#else // Linux:

	const ssize_t res = getrandom(
		buf, // buf
		buflen, // buflen
		0 // flags
	);
	if(res < buflen)
		throw glare::Exception("getrandom failed: " + PlatformUtils::getLastErrorString());

#endif
}


} // end namespace CryptoRNG


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "StringUtils.h"
#include "Timer.h"


void CryptoRNG::test()
{
	conPrint("CryptoRNG::test()");
	try
	{
		for(int i=0; i<16; ++i)
		{
			const int BUFSIZE = 16;
			uint8 data[BUFSIZE];
			Timer timer;
			CryptoRNG::getRandomBytes(data, i);
			const double elapsed = timer.elapsed();

			// Check the values are not equal to some random bytes than Windows spat out on one test run
			testAssert(!(
				data[0] == 78 &&
				data[1] == 195 &&
				data[2] == 143 &&
				data[3] == 78 &&
				data[4] == 171
			));

			for(int z=0; z<i; ++z)
				conPrintStr(toString(data[z]) + " ");
			conPrint("(generation took " + doubleToStringNSigFigs(elapsed * 1.0e3, 4) + " ms)");
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
	conPrint("CryptoRNG::test() done.");
}


#endif // BUILD_TESTS
