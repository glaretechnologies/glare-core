/*=====================================================================
HandleWrapper.h
---------------
Copyright Glare Technologies Limited
=====================================================================*/
#pragma once


#include "IncludeWindows.h"
#include "Platform.h"


/*=====================================================================
HandleWrapper
-------------
RAII wrapper for HANDLE.
=====================================================================*/
#if defined(_WIN32)
class HandleWrapper
{
public:
	HandleWrapper() : handle(0) {}
	HandleWrapper(HANDLE handle_) : handle(handle_) {}
	~HandleWrapper() { if(handle) CloseHandle(handle); }

	HANDLE handle;

private:
	// Temporary copies will call CloseHandle() which we don't want.
	INDIGO_DISABLE_COPY(HandleWrapper);
};
#endif
