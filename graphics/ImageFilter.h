/*=====================================================================
ImageFilter.h
-------------
File created by ClassTemplate on Sat Aug 05 19:05:41 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __IMAGEFILTER_H_666_
#define __IMAGEFILTER_H_666_


#include "../graphics/ImageMap.h"
#include "../utils/Array2D.h"
#include "../utils/Reference.h"
#include "../utils/Exception.h"
#include "../maths/Complex.h"
#include "../maths/vec3.h"
class Image;
class Image4f;
class FFTPlan;
namespace glare { class TaskManager; }


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


	static void resizeImage(const Image& in, Image& out, float pixel_enlargement_factor, float mn_b, float mn_c, glare::TaskManager& task_manager);
	static void resizeImage(const Image4f& in, Image4f& out, float pixel_enlargement_factor, float mn_b, float mn_c, glare::TaskManager& task_manager);

	// in and out must have the same number of components (N) and we require N >= 1 and N <= 4
	static void resizeImage(const ImageMapFloat& in, ImageMapFloat& out, float pixel_enlargement_factor, float mn_b, float mn_c, glare::TaskManager& task_manager);

	//adds the image in, convolved by a Chiu filter, to out.
	//static void chiuFilter(const Image& in, Image& out, float standard_deviation, bool include_center);

	static void chromaticAberration(const Image& in, Image& out, float amount, glare::TaskManager& task_manager);

	//static void glareFilter(const Image& in, Image& out, int num_blades, float standard_deviation);

	static void lowResConvolve(const Image& in, const Image& filter_low, int ssf, Image& out, glare::TaskManager& task_manager);
	static void lowResConvolve(const Image4f& in, const Image& filter_low, int ssf, Image4f& out, glare::TaskManager& task_manager);

	// Chooses convolution technique depending on filter size etc..
	static void convolveImage(const Image& in, const Image& filter, Image& out, FFTPlan& plan); // throws glare::Exception on out of mem.
	static void convolveImage(const Image4f& in, const Image& filter, Image4f& out, FFTPlan& plan); // throws glare::Exception on out of mem.
	static void convolveImageSpatial(const Image& in, const Image& filter, Image& out);
	static void convolveImageFFTBySections(const Image& in, const Image& filter, Image& out);
	static void convolveImageFFT(const Image& in, const Image& filter, Image& out);
	static void convolveImageFFT(const Image4f& in, const Image& filter, Image4f& out);
	//static void convolveImageFFTSS(const Image& in, const Image& filter, Image& out, FFTPlan& plan);
	static void convolveImageRobinDaviesFFT(const Image& in, const Image& filter, Image& out);
	static void slowConvolveImageFFT(const Image& in, const Image& filter, Image& out);

	static void realFT(const Array2D<double>& data, Array2D<Complexd>& out);
	static void realIFT(const Array2D<Complexd>& data, Array2D<double>& real_out);

	static void realFFT(const Array2D<double>& data, Array2D<Complexd>& out);

	static void FFTSS_realFFT(const Array2D<double>& data, Array2D<Complexd>& out); // throws glare::Exception on out of mem.

	static Reference<Image> convertDebevecMappingToLatLong(const Reference<Image>& in);

	static void test();

private:
	static void sampleImage(const Image& im);
};



#endif //__IMAGEFILTER_H_666_




