/*=====================================================================
imformatdecoder.h
-----------------
File created by ClassTemplate on Sat Apr 27 16:12:02 2002
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __IMFORMATDECODER_H_666_
#define __IMFORMATDECODER_H_666_


#include <string>
#include <vector>
//class Bitmap;
class Map2D;
#include "../utils/Reference.h"


class ImFormatExcep
{
public:
	ImFormatExcep(const std::string& s_) : s(s_) {}
	~ImFormatExcep(){}

	const std::string& what() const { return s; }
private:
	std::string s;
};


/*=====================================================================
ImFormatDecoder
---------------

=====================================================================*/
class ImFormatDecoder
{
public:
	/*=====================================================================
	ImFormatDecoder
	---------------
	
	=====================================================================*/
	~ImFormatDecoder();

	//static void decodeImage(const std::string& path, Bitmap& bitmap_out); // throws ImFormatExcep on failure

	static Reference<Map2D> decodeImage(const std::string& indigo_base_dir, const std::string& path);

private:
	ImFormatDecoder();
};



#endif //__IMFORMATDECODER_H_666_




