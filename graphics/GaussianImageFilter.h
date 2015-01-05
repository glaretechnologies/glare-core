/*=====================================================================
GaussianImageFilter.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-08-16 17:22:50 +0100
=====================================================================*/
#pragma once


class Image;
namespace Indigo { class TaskManager; }
class FloatComponentValueTraits;
template <class V, class VTraits> class ImageMap;


/*=====================================================================
GaussianImageFilter
-------------------

=====================================================================*/
namespace GaussianImageFilter
{
	// Adds the image in, convolved by a guassian filter, to out.  Standard_deviation is measured in pixels.
	//void gaussianFilter(const Image& in, Image& out, float standard_deviation, Indigo::TaskManager& task_manager);
	void gaussianFilter(const ImageMap<float, FloatComponentValueTraits>& in, ImageMap<float, FloatComponentValueTraits>& out, float standard_deviation, Indigo::TaskManager& task_manager);

	void test();
};
