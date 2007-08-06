#ifndef __JS_PARTICLE_H__
#define __JS_PARTICLE_H__

#include "js_object.h"
#include "jscol_raytraceresult.h"
#include "js_worldcolsys.h"
#include <assert.h>

namespace js
{

class Particle : public Object
{
public:
	Particle(const Vec3& startpos, float mass_, const Vec3& startvel, float drag_coeff_)
	:	Object(true)
	{
		pos = startpos;
		mass = mass_;
		vel = startvel;
		assert(drag_coeff_ >= 0 && drag_coeff_ <= 1);
		drag_coeff = drag_coeff_;
		force_accumulator.zero();
	}
	virtual ~Particle(){}


	virtual void impulseWS(const Vec3& impulse_point_ws, const Vec3& impulse)
	{
		force_accumulator += impulse;
	}


	virtual void writeState(Vector::iterator& iter)
	{
		//-----------------------------------------------------------------
		//write position vector
		//-----------------------------------------------------------------
		(*iter) = pos.x;	++iter;
		(*iter) = pos.y;	++iter;
		(*iter) = pos.z;	++iter;

		//-----------------------------------------------------------------
		//write velocity vector
		//-----------------------------------------------------------------
		(*iter) = vel.x;	++iter;
		(*iter) = vel.y;	++iter;
		(*iter) = vel.z;	++iter;
	}

	virtual void setState(Vector::const_iterator& iter)
	{
		pos.x = (*iter);	++iter;
		pos.y = (*iter);	++iter;
		pos.z = (*iter);	++iter;

		vel.x = (*iter);	++iter;
		vel.y = (*iter);	++iter;
		vel.z = (*iter);	++iter;
	}

	virtual bool checkForColAndRebound(WorldColSys& col_sys)
	{
	

		//virtual void traceRay(const Vec3& inipos, const Vec3& endpos, RayTraceResult& results_out) = 0;
		RayTraceResult results;	
		col_sys.traceRay(oldpos, pos, results);

		if(results.rayhitsomething)
		{
			//-----------------------------------------------------------------
			//get projection of vel on object normal
			//-----------------------------------------------------------------
			float normcomp = dot(vel, results.ob_normal);

			//-----------------------------------------------------------------
			//compute required change in vel
			//-----------------------------------------------------------------
			Vec3 dvel;


			if(normcomp < 0)
			{
				dvel = normcomp * results.ob_normal * -2;
			}
			else
			{
				dvel = normcomp * results.ob_normal * -2;
			}

			//-----------------------------------------------------------------
			//apply force to acheive dvel
			//-----------------------------------------------------------------
			//Vec3 force_for_dvel = dvel * mass;

			//force_accumulator += force_for_dvel;
			vel += dvel;
			pos = oldpos;
		

			return true;
		}
		else
		{
			return false;
		}
	}
	/*virtual void setStateCheckForCol(Vector::const_iterator& iter, WorldColSys& col_sys)
	{
		//-----------------------------------------------------------------
		//read in new position
		//-----------------------------------------------------------------
		Vec3 newpos;
		newpos.x = (*iter);	++iter;
		newpos.y = (*iter);	++iter;
		newpos.z = (*iter);	++iter;

		//virtual void traceRay(const Vec3& inipos, const Vec3& endpos, RayTraceResult& results_out) = 0;
		RayTraceResult results;	
		col_sys.traceRay(pos, newpos, results);

		if(results.rayhitsomething)
		{
			//return;//don't update pos
			Vec3 dpos = newpos - pos;
			float normcomp = dot(dpos, results.ob_normal);

			Vec3 dvel;

			if(normcomp < 0)
			{
				dvel = normcomp * results.ob_normal * -2;
			}
			else
			{
				dvel = normcomp * results.ob_normal * 2;
			}

			Vec3 force_for_dvel = dvel * mass;

			force_accumulator += force_for_dvel;
			return;
		}
		pos = newpos;
	}*/


	virtual void saveState()
	{
		oldpos = pos;
		oldvel = vel;
	}

	virtual void undo()
	{
		pos = oldpos;
		vel = oldvel;
	}



		
	virtual void writeDeriv(Vector::iterator& iter)
	{
		force_accumulator -= drag_coeff * vel * mass;	
		//-----------------------------------------------------------------
		//turn force_accumulator into acceleration vector
		//-----------------------------------------------------------------
		force_accumulator /= mass;

		force_accumulator += Vec3(0, -9.81, 0);

		//-----------------------------------------------------------------
		//write d(position)/dt    = velocity
		//-----------------------------------------------------------------
		(*iter) = vel.x;	++iter;
		(*iter) = vel.y;	++iter;
		(*iter) = vel.z;	++iter;	

		//-----------------------------------------------------------------
		//write d(vel)/dt		= force / mass
		//-----------------------------------------------------------------
		(*iter) = force_accumulator.x;	++iter;
		(*iter) = force_accumulator.y;	++iter;
		(*iter) = force_accumulator.z;	++iter;

		force_accumulator.zero();
	}


	const Vec3& getPos() const { return pos; }
private:
	float drag_coeff;
	Vec3 pos;
	Vec3 vel;
	float mass;
	Vec3 force_accumulator;

	Vec3 oldpos;
	Vec3 oldvel;
};


}//end namespace js
#endif //__JS_PARTICLE_H__