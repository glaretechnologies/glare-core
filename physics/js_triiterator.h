#ifndef __TRIITERATOR_H__
#define __TRIITERATOR_H__


namespace js
{

class jsTriIterator
{
public:
	/*jsTriIterator(jsPhysics* physics_)
	{
		physics = physics_
	}*/

	jsTriIterator(const jsTriIterator& rhs)
	{
		*this = rhs;
	}

	jsTriIterator(	jsPhysics* physics_,
					std:::list<Object*>::const_iterator& ob_it_, 
					std::list<jsTriangle>::const_iterator tri_it_)
	{
		physics = physics_;
		ob_it = ob_it_;
		tri_it = tri_it_;
	}

	jsTriIterator& operator = (const jsTriIterator& rhs)
	{
		physics = rhs.physics;
		ob_it = rhs.ob_it;
		tri_it = rhs.tri_it;
	}

					





private:
	jsPhysics* physics
	std:::list<Object*>::const_iterator ob_it;
	std::list<jsTriangle>::const_iterator tri_it;
};







}//end namespace js








#endif //__TRIITERATOR_H__