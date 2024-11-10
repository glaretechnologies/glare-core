/*=====================================================================
Query.h
-------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Platform.h"


/*=====================================================================
Query
-----

=====================================================================*/
class Query : public RefCounted
{
public:
	Query();
	~Query();

	void beginTimerQuery();
	void endTimerQuery();

	enum State
	{
		State_Idle,
		State_QueryRunning,
		State_WaitingForResult
	};

	bool isIdle() { return state == State_Idle; }
	bool isRunning() { return state == State_QueryRunning; }
	bool waitingForResult() { return state == State_WaitingForResult; }

	bool checkResultAvailable();

	double getTimeElapsed(); // Blocks until result is ready.

private:
	GLARE_DISABLE_COPY(Query)

	GLuint query_id;
	State state;
};


typedef Reference<Query> QueryRef;
