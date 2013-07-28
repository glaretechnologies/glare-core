#pragma once


class Image;
class Image4f;

class ImageErrorMetric
{
public:
	ImageErrorMetric(){}
	~ImageErrorMetric(){}



	static float rootMeanSquaredError(const Image4f& a, const Image4f& b);


};
