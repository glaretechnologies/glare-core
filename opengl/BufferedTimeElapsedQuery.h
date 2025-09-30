/*=====================================================================
BufferedTimeElapsedQuery.h
--------------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Platform.h"


/*=====================================================================
BufferedTimeElapsedQuery
------------------------
A buffered query object, that returns GPU elapsed times.
Has several GL query objects so we can record a time-elapsed every frame, and
can also retrieve a recent elapsed time without causing a stall.
=====================================================================*/
class BufferedTimeElapsedQuery : public RefCounted
{
public:
	BufferedTimeElapsedQuery();
	~BufferedTimeElapsedQuery();

	void recordBegin();
	void recordEnd();

	double getLastTimeElapsed(); // Get last retrieved elapsed time, in units of seconds.  Will be NUM_QUERIES behind the most recent elapsed time recorded.

private:
	GLARE_DISABLE_COPY(BufferedTimeElapsedQuery)

	static const int NUM_QUERIES = 8; // Still get stalls with 4 queries.
	GLuint query_ids[NUM_QUERIES];

	int next_query_i;
	int largest_queried_i;
};


typedef Reference<BufferedTimeElapsedQuery> BufferedTimeElapsedQueryRef;
