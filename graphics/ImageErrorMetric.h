#pragma once


class Image;


class ImageErrorMetric
{
public:
	ImageErrorMetric(){}
	~ImageErrorMetric(){}



	static float standardDeviation(const Image& a, const Image& b);


};
