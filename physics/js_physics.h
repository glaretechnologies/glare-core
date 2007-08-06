#ifndef __PHYSICS_H__
#define __PHYSICS_H__

#include <list>
#include "C:\programming\pointerhash\pointerhash.h"
#include "C:\programming\maths\maths.h"
#include "C:\programming\integrator\integrator.h"





/*==================================================================================
main physics conponents:

Dynamics
--------
*	solver




World-static collision system
*	spacial grouping data structure
==================================================================================*/

#include "js_dynamics.h"






/*==================================================================================
Physics
-------
An imp of dynamics interface
==================================================================================*/


//class js::Object;

namespace js
{

class Object;
class WorldColSys;
class Particle;

class Physics : public Dynamics, public DerivEvaluator
{
public:
	Physics(WorldColSys* world_col_sys);
	virtual ~Physics();

	virtual void insertObject(Object* object){ objects.add(object); }

	virtual void removeObject(Object* object){ objects.remove(object); }

	void insertParticle(Particle* p){ particles.add(p); }
	void removeParticle(Particle* p){ particles.remove(p); }

	virtual void advanceTime(float dtime);

	//-----------------------------------------------------------------
	//inherited from derivevaluator
	//-----------------------------------------------------------------
	virtual void evaluateDerivAt(const Vector& state_in, float time, Vector& derivative_out);


private:
	//std::list<Object*> objects;
	typedef PointerHash<Object> OBJECTCONTAINER;
	OBJECTCONTAINER objects;

	PointerHash<Particle> particles;


	Vec3 gravity;
	WorldColSys* world_col_sys;
};





}//end namespace js
#endif //__PHYSICS_H__