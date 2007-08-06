/*=====================================================================
distobstore.h
-------------
File created by ClassTemplate on Wed Sep 15 03:23:18 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DISTOBSTORE_H_666_
#define __DISTOBSTORE_H_666_


#include <map>
#include "intobuid.h"
#include "distobstoreinterface.h"
#include <vector>
//#include <iostream>//TEMP
#include "../cyberspace/globals.h"//TEMP TEMP TEMP

/*=====================================================================
DistObStore
-----------
Generic implementation of the interface DistObStoreInterface.
Parameterised by the type to store.

the type to store, OBJECT_TYPE, must be a subclass of DistObject
=====================================================================*/
template <class OBJECT_TYPE>
class DistObStore : public DistObStoreInterface
{
public:
	/*=====================================================================
	DistObStore
	-----------
	
	=====================================================================*/
	inline DistObStore();

	inline virtual ~DistObStore();


	inline OBJECT_TYPE* getObjectForUID(const UID_REF uid);
	inline const OBJECT_TYPE* getObjectForUID(const UID_REF uid) const;

	inline void insertObject(/*const IntObUID& uid, */OBJECT_TYPE* distobject);
	inline void removeObject(const UID_REF uid);
	inline void deleteAndRemoveAllObjects();
	







	//inline virtual void insertDistObject(DistObject* ob);
	inline virtual void removeDistObject(const UID_REF uid);
	inline virtual DistObject* getDistObjectForUID(const UID_REF uid);

	//factory insert function
	//inline virtual DistObject* constructObFromStream(MyStream& state_stream);
	inline virtual void constructAndInsertFromStream(MyStream& state_stream, bool proper_object);

	inline virtual void getObjects(std::vector<DistObject*>& objects_out);

	typedef typename std::map<UID_REF, OBJECT_TYPE*> OBJECT_MAP_TYPE;


	OBJECT_MAP_TYPE& getObjects(){ return objects; }
	const OBJECT_MAP_TYPE& getObjects() const { return objects; }

private:
	OBJECT_MAP_TYPE objects;

};

template <class OBJECT_TYPE>
DistObStore<OBJECT_TYPE>::DistObStore()
{
}

template <class OBJECT_TYPE>
DistObStore<OBJECT_TYPE>::~DistObStore()
{
}


template <class OBJECT_TYPE>
OBJECT_TYPE* DistObStore<OBJECT_TYPE>::getObjectForUID(const UID_REF uid)
{
	OBJECT_MAP_TYPE::iterator result = objects.find(uid);

	if(result == objects.end())
		return NULL;
	else
		return (*result).second;
}

template <class OBJECT_TYPE>
const OBJECT_TYPE* DistObStore<OBJECT_TYPE>::getObjectForUID(const UID_REF uid) const
{
	OBJECT_MAP_TYPE::const_iterator result = objects.find(uid);

	if(result == objects.end())
		return NULL;
	else
		return (*result).second;
}



template <class OBJECT_TYPE>
void DistObStore<OBJECT_TYPE>::insertObject(/*const IntObUID& uid, */OBJECT_TYPE* distobject)
{
	//TEMP::debugPrint("inserting object with uid=" + distobject->getUID().toString());

	OBJECT_MAP_TYPE::iterator result = objects.find(distobject->getUID());

	if(result != objects.end())
	{
		//return NULL;
		assert(0);
	}
	else
	{
		objects.insert(OBJECT_MAP_TYPE::value_type(
										distobject->getUID(), distobject));
	}
}

template <class OBJECT_TYPE>
void DistObStore<OBJECT_TYPE>::removeObject(const UID_REF uid)
{
	//::debugPrint("removing object with uid=" + uid.toString());

	objects.erase(uid);
}


template <class OBJECT_TYPE>
void DistObStore<OBJECT_TYPE>::deleteAndRemoveAllObjects()
{
	for(OBJECT_MAP_TYPE::iterator i=objects.begin(); i != objects.end(); ++i)
		delete (*i).second;

	objects.clear();
}




/*template <class OBJECT_TYPE>
void DistObStore<OBJECT_TYPE>::insertDistObject(DistObject* ob)
{
	insertObject(ob->getDistObUID(), ob);
}*/

template <class OBJECT_TYPE>
void DistObStore<OBJECT_TYPE>::removeDistObject(const UID_REF uid)
{
	removeObject(uid);
}

template <class OBJECT_TYPE>
DistObject* DistObStore<OBJECT_TYPE>::getDistObjectForUID(const UID_REF uid)
{
	return getObjectForUID(uid);
}

//factory function
template <class OBJECT_TYPE>
void DistObStore<OBJECT_TYPE>::constructAndInsertFromStream(MyStream& state_stream,
															bool proper_object)
{
	OBJECT_TYPE* new_ob = OBJECT_TYPE::constructObFromStream(state_stream, proper_object);

	insertObject(new_ob);
}

template <class OBJECT_TYPE>
void DistObStore<OBJECT_TYPE>::getObjects(std::vector<DistObject*>& objects_out)
{
	objects_out.resize(0);

	for(OBJECT_MAP_TYPE::iterator i = objects.begin(); i != objects.end(); ++i)
	{
		objects_out.push_back((*i).second);
	}

}


#endif //__DISTOBSTORE_H_666_




