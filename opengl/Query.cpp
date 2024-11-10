/*=====================================================================
Query.cpp
---------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "Query.h"


#include "IncludeOpenGL.h"


#if !defined(OSX) && !defined(EMSCRIPTEN)
#define QUERIES_SUPPORTED 1
#endif


Query::Query()
:	state(State_Idle)
{
#if QUERIES_SUPPORTED
	glGenQueries(1, &query_id);
#endif
}


Query::~Query()
{
#if QUERIES_SUPPORTED
	glDeleteQueries(1, &query_id);
#endif
}


void Query::beginTimerQuery()
{
#if QUERIES_SUPPORTED
	glBeginQuery(GL_TIME_ELAPSED, query_id);
#endif
	state = State_QueryRunning;
}


void Query::endTimerQuery()
{
#if QUERIES_SUPPORTED
	glEndQuery(GL_TIME_ELAPSED);
#endif
	state = State_WaitingForResult;
}


bool Query::checkResultAvailable()
{
	assert(state == State_WaitingForResult);

	GLint ready = false;
#if QUERIES_SUPPORTED
	glGetQueryObjectiv(query_id, GL_QUERY_RESULT_AVAILABLE, &ready);
#endif
	if(ready != 0)
		state = State_Idle;

	return ready != 0;
}


double Query::getTimeElapsed()
{
	uint64 elapsed_ns = 0;
#if QUERIES_SUPPORTED
	glGetQueryObjectui64v(query_id, GL_QUERY_RESULT, &elapsed_ns); // Blocks
#endif
	return (double)elapsed_ns * 1.0e-9;
}
