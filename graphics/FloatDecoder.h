/*=====================================================================
FloatDecoder.h
--------------
File created by ClassTemplate on Wed Jul 30 07:25:31 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __FLOATDECODER_H_666_
#define __FLOATDECODER_H_666_


#include <string>
class Map2D;
template <class T> class Reference;


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




