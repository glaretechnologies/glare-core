/*===================================================================
OpenSSL
-------
Copyright Glare Technologies Limited 2022 -
====================================================================*/
#pragma once


namespace OpenSSL
{
	void init(); // Initialise LibreSSL stuff.
	void shutdown(); // Cleans up / frees LibreSSL global state.

	bool isInitialised();
}
