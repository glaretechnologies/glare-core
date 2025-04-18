/*=====================================================================
Singleton.h
-----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include <assert.h>
#include <stdlib.h> // For NULL


/*=====================================================================
Singleton
---------
Mixin based Singleton.
The template parameter should be set to the deriving class.
Do not use with program startup time objects.  (as instance may not have been set 
to null yet)
=====================================================================*/
template <class T>
class Singleton
{
public:
	Singleton(){}

	virtual ~Singleton(){}

	inline static void createInstance();
	inline static void createInstance(T* new_inst);
	inline static void destroyInstance();

	inline static T& getInstance();

	inline static bool isNull();
	inline static bool isNonNull();

private:
	Singleton(const Singleton& other);
	Singleton& operator = (const Singleton& other);

	static T* instance;
};


template <class T>
T* Singleton<T>::instance = NULL;


template <class T>
void Singleton<T>::createInstance()
{
	assert(!instance);
	instance = new T();
}


template <class T>
void Singleton<T>::createInstance(T* new_inst)
{
	assert(!instance);
	instance = new_inst;
}


template <class T>
void Singleton<T>::destroyInstance()
{
	delete instance;
	instance = NULL;
}


template <class T>
T& Singleton<T>::getInstance()
{
	assert(instance);
	return *instance;
}


template <class T>
bool Singleton<T>::isNull()
{
	return instance == NULL;
}


template <class T>
bool Singleton<T>::isNonNull()
{
	return instance != NULL;
}
