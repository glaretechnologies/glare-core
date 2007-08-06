#include "js_physics.h"

#include "js_object.h"
#include "js_particle.h"


js::Physics::Physics(WorldColSys* world_col_sys_)
:	objects(1000),
	particles(2000)
{
	gravity.set(0,-9.81,0);
	world_col_sys = world_col_sys_;

}

js::Physics::~Physics()
{}




void js::Physics::evaluateDerivAt(const Vector& state_in, float time, Vector& derivative_out)
{
	{	
	//-----------------------------------------------------------------
	//spill out state of all objects
	//-----------------------------------------------------------------
	Vector::const_iterator newstate_iter = state_in.const_begin();
	for(OBJECTCONTAINER::iterator i = objects.begin(); i != objects.end(); i++)
	{
		if((*i)->isMoveable())//note put me in separate list
		{
			//-----------------------------------------------------------------
			//set state of object
			//-----------------------------------------------------------------
			(*i)->setState(newstate_iter);
		}
	}

	//-----------------------------------------------------------------
	//spill out state of all particles
	//-----------------------------------------------------------------
	for(PointerHash<Particle>::iterator p_iter = particles.begin(); p_iter != particles.end(); p_iter++)
	{
		if((*p_iter)->isMoveable())//note put me in separate list
		{
			//-----------------------------------------------------------------
			//set state of object
			//-----------------------------------------------------------------
			(*p_iter)->setState(newstate_iter);
		}
	}

	}
	

	{
	//-----------------------------------------------------------------
	//gather in derivative of all objects's state
	//-----------------------------------------------------------------
	Vector::iterator deriv_iter = derivative_out.begin();
	for(OBJECTCONTAINER::iterator i = objects.begin(); i != objects.end(); i++)
	{
		if((*i)->isMoveable())//note put me in separate list
		{
			//-----------------------------------------------------------------
			//write state of object to state vector
			//-----------------------------------------------------------------
			(*i)->writeDeriv(deriv_iter);
		}
	}
	
	//-----------------------------------------------------------------
	//gather in derivative of all particles' state
	//-----------------------------------------------------------------
	for(PointerHash<Particle>::iterator p_iter = particles.begin(); p_iter != particles.end(); p_iter++)
	{
		if((*p_iter)->isMoveable())//note put me in separate list
		{
			//-----------------------------------------------------------------
			//write state of object to state vector
			//-----------------------------------------------------------------
			(*p_iter)->writeDeriv(deriv_iter);
		}
	}
	}


}






void js::Physics::advanceTime(float dtime)
{



	Vector state(1000);
	Vector newstate(1000);
	Vector::iterator state_iter = state.begin();

	//-----------------------------------------------------------------
	//gather in state of all objects
	//-----------------------------------------------------------------
	for(OBJECTCONTAINER::iterator i = objects.begin(); i != objects.end(); i++)
	{
		if((*i)->isMoveable())
		{
			//-----------------------------------------------------------------
			//write state of object to state vector
			//-----------------------------------------------------------------
			(*i)->writeState(state_iter);

			(*i)->saveState();
		}
	}

	//-----------------------------------------------------------------
	//gather in state of particles
	//-----------------------------------------------------------------
	for(PointerHash<Particle>::iterator p_iter = particles.begin(); p_iter != particles.end(); p_iter++)
	{
		if((*p_iter)->isMoveable())
		{
			//-----------------------------------------------------------------
			//write state of particle to state vector
			//-----------------------------------------------------------------
			(*p_iter)->writeState(state_iter);

			(*p_iter)->saveState();
		}
	}


	//static void midPointIntegrate(const Vector& state_in, int state_dimension, float time, 
	//						float stepsize, DerivEvaluator& deriv_evaler, Vector& state_out);

	Integrator::midPointIntegrate(state, state_iter.getIndex(), 0, 
							dtime, *this, newstate);




	Vector::const_iterator newstate_iter(newstate.const_begin());
	//-----------------------------------------------------------------
	//spill out state of all objects
	//-----------------------------------------------------------------
	{
	for(OBJECTCONTAINER::iterator i = objects.begin(); i != objects.end(); i++)
	{
		if((*i)->isMoveable())
		{
			//-----------------------------------------------------------------
			//set state of object
			//-----------------------------------------------------------------
			(*i)->setState(newstate_iter);
		}
	}

	//-----------------------------------------------------------------
	//spill out state of all particles
	//-----------------------------------------------------------------	
	for(PointerHash<Particle>::iterator p_iter = particles.begin(); p_iter != particles.end(); p_iter++)
	{
		if((*p_iter)->isMoveable())
		{
			//-----------------------------------------------------------------
			//set state of particle
			//-----------------------------------------------------------------
			//(*p_iter)->setStateCheckForCol(newstate_iter, *world_col_sys);
			(*p_iter)->setState(newstate_iter);



			//-----------------------------------------------------------------
			//check for collision
			//-----------------------------------------------------------------
			if((*p_iter)->checkForColAndRebound(*world_col_sys))
			{
				//(*p_iter)->undo();
			}
		}
	}

	assert(newstate_iter.getIndex() == state_iter.getIndex());
	}








/*
	for(OBJECTCONTAINER::iterator i = objects.begin(); i != objects.end(); i++)
	{
		if((*i)->isMoveable())//note put me in separate list
		{
			
		
			
				
		
		
		
		
		
		
		
		
		
		
		
		*/
			/*
			//-----------------------------------------------------------------
			//bounce off 'ground'
			//-----------------------------------------------------------------
			if((*i)->corestate.mat.getOrigin().y < -30)
			{
				(*i)->corestate.mat.getOrigin().y = -30 + (-30 - (*i)->corestate.mat.getOrigin().y);

				(*i)->corestate.vel.y *= -0.8;

				return;
			}

			Vec3 sumforce(0,0,0);
			sumforce += (*i)->getSelfForces();

			//-----------------------------------------------------------------
			//do Euler integration on each object
			//-----------------------------------------------------------------
			Vec3 dvel = ((sumforce / (*i)->getMass()) + gravity)* dtime;
			(*i)->corestate.vel += dvel;
			//a = ((f / m) + g)* dtime
			//vel += a

			(*i)->corestate.mat.getOrigin() += (*i)->corestate.vel;
			//TEMP

			*/


			//pos += vel
/*		}
	}*/
}