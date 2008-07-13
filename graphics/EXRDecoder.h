/*=====================================================================
EXRDecoder.h
------------
File created by ClassTemplate on Fri Jul 11 02:36:44 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __EXRDECODER_H_666_
#define __EXRDECODER_H_666_


#include <vector>
#include <map>
#include <string>
#include "../utils/reference.h"
class Map2D;
class Bitmap;


/*=====================================================================
EXRDecoder
----------

=====================================================================*/
class EXRDecoder
{
public:
	/*=====================================================================
	EXRDecoder
	----------
	
	=====================================================================*/
	EXRDecoder();

	~EXRDecoder();


	// throws ImFormatExcep
	static Reference<Map2D> decode(const std::string& path);


};



#endif //__EXRDECODER_H_666_




