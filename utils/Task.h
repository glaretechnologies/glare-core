/*=====================================================================
Task.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-10-05 21:56:27 +0100
=====================================================================*/
#pragma once


namespace Indigo
{


/*=====================================================================
Task
-------------------

=====================================================================*/
class Task
{
public:
	Task();
	virtual ~Task();

	virtual void run(size_t thread_index) = 0;
private:

};


} // end namespace Indigo 


