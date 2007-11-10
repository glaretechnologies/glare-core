/*=====================================================================
TSImage.h
---------
File created by ClassTemplate on Mon Nov 05 17:49:00 2007
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TSIMAGE_H_666_
#define __TSIMAGE_H_666_


#include "../utils/mutex.h"
#include "image.h"


/*=====================================================================
TSImage
-------
Thread-safe image (has mutex)
=====================================================================*/
class TSImage : public Image
{
public:
	/*=====================================================================
	TSImage
	-------
	
	=====================================================================*/
	TSImage();

	~TSImage();



	Mutex mutex;

};



#endif //__TSIMAGE_H_666_




