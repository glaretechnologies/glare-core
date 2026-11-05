/*===================================================================
OpenSSL
-------
Copyright Glare Technologies Limited 2026 -
====================================================================*/
#pragma once


namespace OpenSSL
{
	// init() is no longer needed and has been removed.

	void shutdown(); // Cleans up / frees LibreSSL global state.

	// Frees some per-thread error queue state that LibreSSL allocates.  Call at the end of any thread that uses LibreSSL/TLSSocket.
	// Should be harmless to call if any LibreSSL/TLSSocket functions are called afterwards on the thread inadvertently.
	// Does not throw.
	void threadCleanup();
}
