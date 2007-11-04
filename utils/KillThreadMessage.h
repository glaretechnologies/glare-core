/*=====================================================================
KillThreadMessage.h
-------------------
File created by ClassTemplate on Sat Nov 03 08:34:44 2007
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __KILLTHREADMESSAGE_H_666_
#define __KILLTHREADMESSAGE_H_666_


#include "ThreadMessage.h"


/*=====================================================================
KillThreadMessage
-----------------
If a thread gets this message, it should try and finish its work and terminate
ASAP.
=====================================================================*/
class KillThreadMessage : public ThreadMessage
{
public:
	/*=====================================================================
	KillThreadMessage
	-----------------
	
	=====================================================================*/
	KillThreadMessage();

	virtual ~KillThreadMessage();


	virtual ThreadMessage* clone() const { return new KillThreadMessage(); }



};



#endif //__KILLTHREADMESSAGE_H_666_




