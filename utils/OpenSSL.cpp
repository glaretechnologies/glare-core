/*===================================================================
OpenSSL
-------
Copyright Glare Technologies Limited 2021 -
====================================================================*/
#include "OpenSSL.h"


#include "Mutex.h"
#include <openssl/crypto.h>
#include <vector>
#include <cassert>


/*
"OpenSSL can safely be used in multi-threaded applications provided that at least two callback functions are set, locking_function and threadid_func." - 
https://www.openssl.org/docs/manmaster/crypto/threads.html

*/


static Mutex* mutexes = NULL;
static int num_mutexes = 0;


// Not going to try and do thread safety analysis on this function, hence the NO_THREAD_SAFETY_ANALYSIS.
static void locking_callback(int mode, int type, const char* file, int line)		NO_THREAD_SAFETY_ANALYSIS
{
	assert(type >= 0 && type < num_mutexes);

	if(mode & CRYPTO_LOCK)
		mutexes[type].acquire();
	else
		mutexes[type].release();
}


static unsigned long threadid_func_callback()
{
#ifdef _WIN32
	return ::GetCurrentThreadId();
#else
	return (unsigned long)pthread_self(); // pthread_self - obtain ID of the calling thread - http://man7.org/linux/man-pages/man3/pthread_self.3.html
#endif
}


void OpenSSL::init()
{
	if(mutexes) // If init has already been called:
		return;

	num_mutexes = CRYPTO_num_locks();
	mutexes = (Mutex*)malloc(sizeof(Mutex) * num_mutexes);
	for(int i=0; i<num_mutexes; ++i)
		new (&mutexes[i]) Mutex(); // Construct it

	CRYPTO_set_locking_callback(locking_callback);

	CRYPTO_set_id_callback(threadid_func_callback);
}


void OpenSSL::shutdown()
{
	CRYPTO_cleanup_all_ex_data();

	CRYPTO_set_locking_callback(NULL);

	for(int i=0; i<num_mutexes; ++i)
		mutexes[i].~Mutex();

	free(mutexes);

	mutexes = NULL;
}


bool OpenSSL::isInitialised()
{
	return mutexes != NULL;
}
