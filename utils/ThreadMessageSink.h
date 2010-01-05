/*=====================================================================
ThreadMessageSink.h
-------------------
File created by ClassTemplate on Mon Dec 28 17:12:22 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __THREADMESSAGESINK_H_666_
#define __THREADMESSAGESINK_H_666_


class ThreadMessage;


/*=====================================================================
ThreadMessageSink
-----------------

=====================================================================*/
class ThreadMessageSink
{
public:
	/*=====================================================================
	ThreadMessageSink
	-----------------
	
	=====================================================================*/
	ThreadMessageSink();

	virtual ~ThreadMessageSink();


	virtual void handleMessage(ThreadMessage* msg) = 0;

};



#endif //__THREADMESSAGESINK_H_666_




