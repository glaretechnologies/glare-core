/*=====================================================================
TimestampQuery.cpp
------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "TimestampQuery.h"


#include "IncludeOpenGL.h"
#include "../maths/mathstypes.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"


#if !defined(OSX) && !defined(EMSCRIPTEN)
#define QUERIES_SUPPORTED 1
#endif



/*

_________________________
|  0  |  1  |  2  |  3  |
-------------------------
   ^
   |
next_query_i


recordTimestamp(): glQueryCounter() called on query_ids[0]


glQueryCounter called
   v
_________________________
|  0  |  1  |  2  |  3  |
-------------------------
         ^
         |
      next_query_i


recordTimestamp(): glQueryCounter() called on query_ids[1]

glQueryCounter called
   v     v
_________________________
|  0  |  1  |  2  |  3  |
-------------------------
               ^
               |
            next_query_i


recordTimestamp(): glQueryCounter() called on query_ids[2]

glQueryCounter called
   v     v     v
_________________________
|  0  |  1  |  2  |  3  |
-------------------------
                     ^
                     |
                  next_query_i



recordTimestamp(): glQueryCounter() called on query_ids[3]

glQueryCounter called
   v     v     v     v
_________________________
|  0  |  1  |  2  |  3  |
-------------------------
   ^
   |
next_query_i
 
 
recordTimestamp(): glQueryCounter() called on query_ids[0]

glQueryCounter called
   v     v     v     v
_________________________
|  0  |  1  |  2  |  3  |
-------------------------
         ^
         |
      next_query_i
 
 
 */
TimestampQuery::TimestampQuery()
:	next_query_i(0),
	largest_queried_i(-1)
{
#if QUERIES_SUPPORTED
	glGenQueries(NUM_QUERIES, query_ids);
#endif
}


TimestampQuery::~TimestampQuery()
{
#if QUERIES_SUPPORTED
	glDeleteQueries(NUM_QUERIES, query_ids);
#endif
}


void TimestampQuery::recordTimestamp()
{
#if QUERIES_SUPPORTED
	//conPrint("glQueryCounter() with ID " + toString(next_query_i));
	glQueryCounter(query_ids[next_query_i], GL_TIMESTAMP);

	largest_queried_i = myMax(largest_queried_i, next_query_i);
	next_query_i = (next_query_i + 1) % NUM_QUERIES; // Advance next_query_i
#endif
}


double TimestampQuery::getLastTimestamp()
{
#if QUERIES_SUPPORTED
	uint64 timestamp_ns = 0;
	const int query_index = (next_query_i) % NUM_QUERIES;
	if(query_index <= largest_queried_i)
	{
		//Timer timer;
		glGetQueryObjectui64v(query_ids[query_index], GL_QUERY_RESULT, &timestamp_ns); // Blocks
		//conPrint("glGetQueryObjectui64v for ID " + toString(query_index) + " took " + timer.elapsedStringMS());
		return (double)timestamp_ns * 1.0e-9;
	}
	else
		return 0.0;
#else
	return 0;
#endif
}
