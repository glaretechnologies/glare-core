/*=====================================================================
FloatDecoder.h
--------------
File created by ClassTemplate on Wed Jul 30 07:25:31 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __FLOATDECODER_H_666_
#define __FLOATDECODER_H_666_


#include <vector>
#include <map>
#include <string>
#include "../utils/reference.h"
class Map2D;
class Bitmap;


/*=====================================================================
FloatDecoder
------------

=====================================================================*/
class FloatDecoder
{
public:
	/*=====================================================================
	FloatDecoder
	------------
	
	=====================================================================*/
	FloatDecoder();

	~FloatDecoder();


	// throws ImFormatExcep
	static Reference<Map2D> decode(const std::string& path);

};



#endif //__FLOATDECODER_H_666_




