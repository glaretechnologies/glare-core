/*=====================================================================
GaussianImageFilter.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-08-16 17:22:50 +0100
=====================================================================*/
#pragma once


class Image;
namespace Indigo { class TaskManager; }


/*=====================================================================
GaussianImageFilter
-------------------

=====================================================================*/
namespace GaussianImageFilter
{


	// Adds the image in, convolved by a guassian filter, to out.
	void gaussianFilter(const Image& in, Image& out, float standard_deviation, Indigo::TaskManager& task_manager);

};
