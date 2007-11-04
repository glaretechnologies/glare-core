/*=====================================================================
ThreadTerminatingMessage.h
--------------------------
File created by ClassTemplate on Sat Nov 03 08:34:52 2007
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __THREADTERMINATINGMESSAGE_H_666_
#define __THREADTERMINATINGMESSAGE_H_666_



#include "ThreadMessage.h"


/*=====================================================================
ThreadTerminatingMessage
------------------------
When the threadmanager gets this message, it may kill the thread that sent it.
=====================================================================*/
class ThreadTerminatingMessage : public ThreadMessage
{
public:
	/*=====================================================================
	ThreadTerminatingMessage
	------------------------
	
	=====================================================================*/
	ThreadTerminatingMessage();

	virtual ~ThreadTerminatingMessage();



	virtual ThreadMessage* clone() const { return new ThreadTerminatingMessage(); }


};



#endif //__THREADTERMINATINGMESSAGE_H_666_




