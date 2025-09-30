/*=====================================================================
BufferedTimeElapsedQuery.cpp
----------------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "BufferedTimeElapsedQuery.h"


#include "IncludeOpenGL.h"
#include "../maths/mathstypes.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#if EMSCRIPTEN
#define GL_GLEXT_PROTOTYPES 1
#include <GLES3/gl2ext.h>

#define GL_TIME_ELAPSED GL_TIME_ELAPSED_EXT
#define glQueryCounter glQueryCounterEXT
#define glGetQueryObjectui64v glGetQueryObjectui64vEXT
#endif

#if !defined(OSX)
#define QUERIES_SUPPORTED 1
#endif


BufferedTimeElapsedQuery::BufferedTimeElapsedQuery()
:	next_query_i(0),
	largest_queried_i(-1)
{
#if QUERIES_SUPPORTED
	glGenQueries(NUM_QUERIES, query_ids);
#endif
}


BufferedTimeElapsedQuery::~BufferedTimeElapsedQuery()
{
#if QUERIES_SUPPORTED
	glDeleteQueries(NUM_QUERIES, query_ids);
#endif
}


void BufferedTimeElapsedQuery::recordBegin()
{
#if QUERIES_SUPPORTED
	glBeginQuery(GL_TIME_ELAPSED, query_ids[next_query_i]);
#endif
}


void BufferedTimeElapsedQuery::recordEnd()
{
#if QUERIES_SUPPORTED
	glEndQuery(GL_TIME_ELAPSED);

	largest_queried_i = myMax(largest_queried_i, next_query_i);
	next_query_i = (next_query_i + 1) % NUM_QUERIES; // Advance next_query_i
#endif
}



double BufferedTimeElapsedQuery::getLastTimeElapsed()
{
#if QUERIES_SUPPORTED
	
	const int query_index = (next_query_i) % NUM_QUERIES;
	if(query_index <= largest_queried_i)
	{
		uint64 timestamp_ns = 0;
		glGetQueryObjectui64v(query_ids[query_index], GL_QUERY_RESULT, &timestamp_ns); // Blocks
		return (double)timestamp_ns * 1.0e-9;
	}
	else
		return 0.0;

#else
	return 0;
#endif
}
