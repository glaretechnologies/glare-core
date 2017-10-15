/*=====================================================================
OpenCLContext.h
---------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"


#ifdef OSX
#include <OpenCL/cl.h>
#include <OpenCL/cl_platform.h>
#else
#include <CL/cl.h>
#include <CL/cl_platform.h>
#endif


class ProgramCache;


/*=====================================================================
OpenCLContext
-------------
An OpenCL context
=====================================================================*/
class OpenCLContext: public ThreadSafeRefCounted
{
public:
	OpenCLContext(cl_platform_id platform_id);
	~OpenCLContext();

	cl_context getContext() { return context; }

	Reference<ProgramCache> program_cache; // Built programs are OpenCL context-specific.  So the context will own the program cache, so that
	// the lifetime of programs in the in-memory cache is <= the lifetime of the context.
private:
	cl_context context;
};


typedef Reference<OpenCLContext> OpenCLContextRef;
