/*=====================================================================
ImageFilter.h
-------------
File created by ClassTemplate on Sat Aug 05 19:05:41 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __IMAGEFILTER_H_666_
#define __IMAGEFILTER_H_666_



class Image;

/*=====================================================================
ImageFilter
-----------

=====================================================================*/
class ImageFilter
{
public:
	/*=====================================================================
	ImageFilter
	-----------
	
	=====================================================================*/
	ImageFilter();

	~ImageFilter();


	//adds the image in, convolved by a guassian filter, to out.
	static void gaussianFilter(const Image& in, Image& out, float standard_deviation);


	//adds the image in, convolved by a Chiu filter, to out.
	static void chiuFilter(const Image& in, Image& out, float standard_deviation, bool include_center);

	static void chromaticAberration(const Image& in, Image& out, float amount);

	static void glareFilter(const Image& in, Image& out, int num_blades, float standard_deviation);
private:
	static void sampleImage(const Image& im);
};



#endif //__IMAGEFILTER_H_666_




