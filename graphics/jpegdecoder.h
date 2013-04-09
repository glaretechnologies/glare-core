/*=====================================================================
jpegdecoder.h
-------------
File created by ClassTemplate on Sat Apr 27 16:22:59 2002
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __JPEGDECODER_H_666_
#define __JPEGDECODER_H_666_




#include <vector>
#include <string>
#include "../utils/Reference.h"
class Map2D;


/*=====================================================================
JPEGDecoder
-----------

=====================================================================*/
class JPEGDecoder
{
public:
	/*=====================================================================
	JPEGDecoder
	-----------
	
	=====================================================================*/

	~JPEGDecoder();



//	static void* decode(const void* image, int numimagebytes, //size of encoded image
//					int& bpp_out, int& width_out, int& height_out);
	static Reference<Map2D> decode(const std::string& indigo_base_dir, const std::string& path);



	/*static void * Compress(const void *buffer, // address of image in memory
                         int width, // width of image in pixels
                         int height, // height of image in pixels.
                         int bpp, // *BYTES* per pixel of image 1 or 3
                         int &len, // returns length of compressed data
                         int quality=75); // image quality as a percentage
*/

	static void test(const std::string& indigo_base_dir);

private:
	JPEGDecoder();

	/*static void *ReadImage(int &width,   // width of the image loaded.
                         int &height,  // height of the image loaded.
                         int &bpp,     // BYTES (not bits) PER PIXEL.
                         const void *buffer, // memory address containing jpeg compressed data.
                         int sizebytes); // size of jpeg compressed data.



*/


};



#endif //__JPEGDECODER_H_666_




