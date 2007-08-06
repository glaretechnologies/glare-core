/*=====================================================================
distobserverthread.h
--------------------
File created by ClassTemplate on Sun Sep 05 20:37:58 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DISTOBSERVERTHREAD_H_666_
#define __DISTOBSERVERTHREAD_H_666_


#include "../utils/mythread.h"


/*=====================================================================
DistObServerThread
------------------

=====================================================================*/
class DistObServerThread : public MyThread
{
public:
	/*=====================================================================
	DistObServerThread
	------------------
	
	=====================================================================*/
	DistObServerThread(int listenport);

	virtual ~DistObServerThread();


	virtual void run();

private:

};



#endif //__DISTOBSERVERTHREAD_H_666_




