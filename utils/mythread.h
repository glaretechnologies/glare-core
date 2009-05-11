/*=====================================================================
MyThread.h
----------
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MYTHREAD_H_666_
#define __MYTHREAD_H_666_


#if defined(WIN32) || defined(WIN64)
// Stop windows.h from defining the min() and max() macros
#define NOMINMAX
#include <windows.h>
#else
#include <pthread.h>
#endif
#include <string>


class MyThreadExcep
{
public:
	MyThreadExcep(const std::string& text_) : text(text_) {}
	const std::string& what() const { return text; }
private:
	std::string text;
};


/*=====================================================================
MyThread
--------
Thanks to someone from the COTD on flipcode for some of this code
=====================================================================*/
class MyThread
{
public:
	MyThread();
	virtual ~MyThread();

	virtual void run() = 0;

	void launch(bool autodelete = true);

	void join(); // Wait for thread termination

	enum Priority
	{
		Priority_Normal,
		Priority_BelowNormal
	};
	void setPriority(Priority p); // throws MyThreadExcep

private:

#if defined(WIN32) || defined(WIN64)
	static void 
#else
	static void*
#endif
		/*_cdecl*/ threadFunction(void* the_thread);


#if defined(WIN32) || defined(WIN64)
	HANDLE thread_handle;
#else
	pthread_t thread_handle;
#endif

	bool autodelete;
};


#endif //__MYTHREAD_H_666_
