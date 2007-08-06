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
class Bitmap;


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


	//returns NULL on failure
	//static void* decodeImage(const void* image, int imagesize, const char* image_name, 
	//	int& bpp_out, int& width_out, int& height_out);

	static void decodeImage(const std::vector<unsigned char>& encoded_image, 
		const std::string& imagename, Bitmap& bitmap_out);

	//returns newly alloced image, or NULL if not resized
	//static void* resizeImageToPow2(const void* image, int width, int height, int bytespp, 
	//						int& width_out, int& height_out);

	static bool powerOf2Dims(int width, int height);

	//static void freeImage(void* image);//for dlls, just delete[]s image


	//returns pointer to newly alloced buffer
	//static void* resizeToPowerOf2Dims(const void* image, int width, int height, int bytespp, 
	//											int& newwidth_out, int& newheight_out);
	static void resizeToPowerOf2Dims(const Bitmap& srcimage, Bitmap& image_out);

	//TEMP hackish location
	//static void saveImageToDisk(const std::string& pathname, const Bitmap& bitmap);
private:
	ImFormatDecoder();

};



#endif //__IMFORMATDECODER_H_666_




