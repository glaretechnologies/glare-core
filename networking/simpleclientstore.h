/*=====================================================================
simpleclientstore.h
-------------------
File created by ClassTemplate on Tue Sep 07 05:01:57 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __SIMPLECLIENTSTORE_H_666_
#define __SIMPLECLIENTSTORE_H_666_


#pragma warning(disable : 4786)//disable long debug name warning


#include "distoblistener.h"
#include <map>
class TestDistOb;


/*=====================================================================
SimpleClientStore
-----------------

=====================================================================*/
class SimpleClientStore : public DistObListener
{
public:
	/*=====================================================================
	SimpleClientStore
	-----------------
	
	=====================================================================*/
	SimpleClientStore();

	virtual ~SimpleClientStore();


	virtual void distObjectCreated(MyStream& uid_stream, MyStream& state_stream);
	virtual void distObjectDestroyed(MyStream& uid_stream);

	virtual void distObjectStateChange(MyStream& uid_stream, MyStream& state_stream);

	virtual void getLocalObjects(std::vector<DistObject*>& local_objects_out);


private:
	std::map<int, TestDistOb*> objects;

};



#endif //__SIMPLECLIENTSTORE_H_666_




