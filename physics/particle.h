#ifndef __PARTICLE_H__
#define __PARTICLE_H__


#include "stubglobals.h"
#include "C:\programming\simple3d\point.h"
#include "js_particle.h"

class Particle
{
public:
	Particle(const Vec3& startpos, float mass, const Vec3& startvel, float drag_coeff)
	{
		//-----------------------------------------------------------------
		//insert model into dynamics system
		//-----------------------------------------------------------------
		phys_particle = new js::Particle(startpos, mass, startvel, drag_coeff);
		physics->insertParticle(phys_particle);


		//-----------------------------------------------------------------
		//construct graphics rep and insert into graphics system
		//-----------------------------------------------------------------
		gr_particle = new Point(startpos);
		simple3d->insertObject(gr_particle);
	}

	~Particle()
	{
		physics->removeParticle(phys_particle);

		delete phys_particle;

		simple3d->removeObject(gr_particle);

		delete gr_particle;
	}


	void think()
	{
		gr_particle->set( phys_particle->getPos() );
	}



private:
	js::Particle* phys_particle;
	Point* gr_particle;

};





#endif //__PARTICLE_H__