/*===================================================================
OpenSSL
-------
Copyright Glare Technologies Limited 2026 -
====================================================================*/
#include "OpenSSL.h"


#include <openssl/crypto.h>
#include <openssl/err.h>


// OPENSSL_init_crypto is called lazily now by LibreSSL internals, so we don't need to actively call it.
// It is also called by tls_init() which we call from TLSSocket::initTLS().


void OpenSSL::shutdown()
{
	threadCleanup();
	OPENSSL_cleanup();
}


// Frees some per-thread error queue state that LibreSSL allocates.
void OpenSSL::threadCleanup()
{
	ERR_remove_thread_state(/*thread id=*/nullptr); // Set thread ID to null to use current thread.
}
