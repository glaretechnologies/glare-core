/*===================================================================
OpenSSL
-------
Copyright Glare Technologies Limited 2022 -
====================================================================*/
#include "OpenSSL.h"


#include <openssl/crypto.h>
#include <cassert>


// OpenSSL::init():
// We used to do a bunch of stuff here with defining mutexes that were used by LibreSSL.  With LibreSSL 3.5.2 and probably earlier that stuff wasn't even used by LibreSSL, so I removed it.
// From LibreSSL changelog (2.9.0 - Development release):
// * CRYPTO_LOCK is now automatically initialized, with the legacy callbacks stubbed for compatibility.


void OpenSSL::shutdown()
{
	CRYPTO_cleanup_all_ex_data();
}
