/*===================================================================
OpenSSL
-------
Copyright Glare Technologies Limited 2016 -
====================================================================*/
#include "OpenSSL.h"


#include "Mutex.h"
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/crypto.h>
#include <vector>


/*
"OpenSSL can safely be used in multi-threaded applications provided that at least two callback functions are set, locking_function and threadid_func." - 
https://www.openssl.org/docs/manmaster/crypto/threads.html

*/


//static std::vector<Mutex*> mutexes;
static Mutex* mutexes = NULL;
static int num_mutexes = 0;


static void locking_callback(int mode, int type, const char* file, int line)
{
	if(mode & CRYPTO_LOCK)
		mutexes[type].acquire();
	else
		mutexes[type].release();
}


unsigned long threadid_func_callback()
{
#ifdef _WIN32
	return ::GetCurrentThreadId();
#else
	return (unsigned long)pthread_self(); // pthread_self - obtain ID of the calling thread - http://man7.org/linux/man-pages/man3/pthread_self.3.html
#endif
}


void OpenSSL::init()
{
	//if(!mutexes.empty()) // If init has already been called:
	//	return;
	if(mutexes)
		return;

	//mutexes.resize(CRYPTO_num_locks());
	num_mutexes = CRYPTO_num_locks();
	mutexes = (Mutex*)malloc(sizeof(Mutex) * num_mutexes);
	for(size_t i=0; i<num_mutexes; ++i)
		new (&mutexes[i]) Mutex(); // Construct it
	//for(size_t i=0; i<mutexes.size(); ++i)
	//	mutexes[i] = new Mutex();

	CRYPTO_set_locking_callback(locking_callback);

	CRYPTO_set_id_callback(threadid_func_callback);
}


void OpenSSL::shutdown()
{
	CRYPTO_cleanup_all_ex_data();

	CRYPTO_set_locking_callback(NULL);

	for(size_t i=0; i<num_mutexes; ++i)
		mutexes[i].~Mutex();
		//delete mutexes[i];
	//mutexes.clear();

	free(mutexes);
}


bool OpenSSL::isInitialised()
{
	return mutexes != NULL;
}
