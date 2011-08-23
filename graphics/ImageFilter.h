/*=====================================================================
ImageFilter.h
-------------
File created by ClassTemplate on Sat Aug 05 19:05:41 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __IMAGEFILTER_H_666_
#define __IMAGEFILTER_H_666_


#include "../utils/array2d.h"
#include "../utils/Exception.h"
#include "../maths/Complex.h"
class Image;
class FFTPlan;


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

	//adds the image in, convolved by a Chiu filter, to out.
	//static void chiuFilter(const Image& in, Image& out, float standard_deviation, bool include_center);

	static void chromaticAberration(const Image& in, Image& out, float amount);

	//static void glareFilter(const Image& in, Image& out, int num_blades, float standard_deviation);

	// Chooses convolution technique depending on filter size etc..
	static void convolveImage(const Image& in, const Image& filter, Image& out, FFTPlan& plan); // throws Indigo::Exception on out of mem.
	static void convolveImageSpatial(const Image& in, const Image& filter, Image& out);
	static void convolveImageFFT(const Image& in, const Image& filter, Image& out);
	static void convolveImageFFTSS(const Image& in, const Image& filter, Image& out, FFTPlan& plan);
	static void convolveImageRobinDaviesFFT(const Image& in, const Image& filter, Image& out);
	static void slowConvolveImageFFT(const Image& in, const Image& filter, Image& out);

	static void realFT(const Array2d<double>& data, Array2d<Complexd>& out);
	static void realIFT(const Array2d<Complexd>& data, Array2d<double>& real_out);

	static void realFFT(const Array2d<double>& data, Array2d<Complexd>& out);

	static void FFTSS_realFFT(const Array2d<double>& data, Array2d<Complexd>& out); // throws Indigo::Exception on out of mem.

	static void test();

private:
	static void sampleImage(const Image& im);
};



#endif //__IMAGEFILTER_H_666_




