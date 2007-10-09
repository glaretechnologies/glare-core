/*=====================================================================
platformutils.h
---------------
File created by ClassTemplate on Mon Jun 06 00:24:52 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PLATFORMUTILS_H_666_
#define __PLATFORMUTILS_H_666_


/*=====================================================================
PlatformUtils
-------------

=====================================================================*/
namespace PlatformUtils
{

	
void Sleep(int x);//make current thread sleep for x milliseconds


//unsigned int getMinWorkingSetSize();
//unsigned int getMaxWorkingSetSize();

unsigned int getNumLogicalProcessors();

}//end namespace PlatformUtils



#endif //__PLATFORMUTILS_H_666_




