/*=====================================================================
MyThread.h
----------
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MYTHREAD_H_666_
#define __MYTHREAD_H_666_


#if defined(_WIN32)
// Stop windows.h from defining the min() and max() macros
#ifndef NOMINMAX
#define NOMINMAX
#endif
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

#if defined(_WIN32)
	HANDLE getHandle() { return thread_handle; }
#endif

	bool autoDelete() const { return autodelete; }

private:
#if defined(_WIN32)
	HANDLE thread_handle;
#else
	pthread_t thread_handle;
#endif

	bool autodelete;
	bool joined; // True if this thread had join() called on it.
};


#endif //__MYTHREAD_H_666_
