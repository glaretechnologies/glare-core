/*=====================================================================
TimestampQuery.h
----------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Platform.h"


/*=====================================================================
TimestampQuery
--------------
A buffered timestamp query object, that returns GPU timestamps
Has several GL query objects so we can record a timestamp every frame, and
can also retrieve a recent timestamp without causing a stall.
=====================================================================*/
class TimestampQuery : public RefCounted
{
public:
	TimestampQuery();
	~TimestampQuery();

	void recordTimestamp();

	double getLastTimestamp(); // Get last retrieved timestamp, in units of seconds.  Will be NUM_QUERIES behind the most recent timestamp taken.

private:
	GLARE_DISABLE_COPY(TimestampQuery)

	static const int NUM_QUERIES = 8; // Still get stalls with 4 queries.
	GLuint query_ids[NUM_QUERIES];

	int next_query_i;
	int largest_queried_i;
};


typedef Reference<TimestampQuery> TimestampQueryRef;
