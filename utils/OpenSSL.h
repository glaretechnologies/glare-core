/*===================================================================
OpenSSL
-------
Copyright Glare Technologies Limited 2016 -
====================================================================*/
#pragma once


namespace OpenSSL
{
	void init(); // Initialise OpenSSL stuff.
	void shutdown(); // Cleans up / frees OpenSSL global state.

	bool isInitialised();
}
