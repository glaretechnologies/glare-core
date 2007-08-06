#ifndef __JS_DYNAMICS_H__
#define __JS_DYNAMICS_H__



namespace js
{

class Object;

class Dynamics
{
public:
	virtual ~Dynamics(){}



	void insertObject(Object* object);
	void removeObject(Object* object);


	void advanceTime(float dtime);
};



}//end namespace js
#endif //__JS_DYNAMICS_H__