/*=====================================================================
TIFFDecoder.h
-------------
File created by ClassTemplate on Fri May 02 16:51:32 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TIFFDECODER_H_666_
#define __TIFFDECODER_H_666_


#include <string>
#include "../utils/Reference.h"
class Map2D;


/*=====================================================================
TIFFDecoder
-----------

=====================================================================*/
class TIFFDecoder
{
public:
	/*=====================================================================
	TIFFDecoder
	-----------
	
	=====================================================================*/
	TIFFDecoder();

	~TIFFDecoder();


	// throws ImFormatExcep
	static Reference<Map2D> decode(const std::string& path);



};



#endif //__TIFFDECODER_H_666_




