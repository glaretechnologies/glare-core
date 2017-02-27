/*=====================================================================
Drawing.cpp
-------------------
Copyright Glare Technologies Limited 2014 -
Generated at 2014-05-20 11:29:28 +0100
=====================================================================*/
#include "Drawing.h"


#include "bitmap.h"
#include "MitchellNetravali.h"
#include "../maths/mathstypes.h"


namespace Drawing
{


void drawLine(Bitmap& bitmap, const Colour3f& colour, const Vec2f& start_, const Vec2f& end_)
{
	MitchellNetravali<float> mn(1.f/3, 1.f/3);

	Vec2f start = start_;
	Vec2f end = end_;

	const int W_1 = (int)bitmap.getWidth()  - 1;
	const int H_1 = (int)bitmap.getHeight() - 1;
	
	if(std::fabs(end.y - start.y) < std::fabs(end.x - start.x)) // If the line is more horizontal than vertical:
	{
		// Step along x
		// We require end.x >= start.x.  So swap endpoints if this is not the case
		if(end.x < start.x)
			mySwap(start, end);

		const int start_x = myClamp((int)std::floor(start.x), 0, W_1);
		const int end_x   = myClamp((int)std::floor(end.x),   0, W_1);

		const float delta_y = end.y - start.y;
		const float delta_x = end.x - start.x;

		// Compute equation of line: y = mx + c
		// c = y - mx
		// also mx = y - c
		//       x = (y - c)/m
		const float m = delta_y / delta_x;
		const float c = start.y - m*start.x;

		for(int xi=start_x; xi<end_x; ++xi)
		{
			float x = (float)xi;
			float line_center_x = x;
			float line_center_y = m*x + c;

			int y_lower = myClamp((int)std::floor(line_center_y) - 2, 0, H_1);
			int y_upper = myClamp((int)std::floor(line_center_y) + 2, 0, H_1);

			for(int yi = y_lower; yi <= y_upper; ++yi)
			{
				float y = (float)yi;

				// Get dist to line center
				float d = std::sqrt(Maths::square(x - line_center_x) + Maths::square(y - line_center_y));

				// Eval filter func
				const float f_d = mn.eval(d);

				// Update pixel
				for(int z=0; z<bitmap.getBytesPP(); ++z)
				{
					const uint8 val_i = bitmap.getPixel(xi, yi)[z];
					const float val = (float)val_i;
					const float newval = f_d * 255.f * colour[z] + val * (1 - f_d);
					const uint8 newval_i = (uint8)myClamp(newval, 0.f, 255.9f);
					bitmap.getPixelNonConst(xi, yi)[z] = newval_i;
				}
			}
		}
	}
	else
	{
		// Step along y
		// We require end.y >= start.y.  So swap endpoints if this is not the case
		if(end.y < start.y)
			mySwap(start, end);

		const int start_y = myClamp((int)std::floor(start.y), 0, H_1);
		const int end_y   = myClamp((int)std::floor(end.y),   0, H_1);

		const float delta_y = end.y - start.y;
		const float delta_x = end.x - start.x;

		// Compute equation expressing x as a function of y
		// x = my + c
		const float m = delta_x / delta_y;
		const float c = start.x - m*start.y;

		for(int yi=start_y; yi<end_y; ++yi)
		{
			float y = (float)yi;
			float line_center_y = y;
			float line_center_x = m*y + c;

			int x_lower = myClamp((int)std::floor(line_center_x) - 2, 0, W_1);
			int x_upper = myClamp((int)std::floor(line_center_x) + 2, 0, W_1);

			for(int xi = x_lower; xi <= x_upper; ++xi)
			{
				float x = (float)xi;

				// Get dist to line center
				float d = std::sqrt(Maths::square(x - line_center_x) + Maths::square(y - line_center_y));

				// Eval filter func
				const float f_d = mn.eval(d);

				// Update pixel
				for(int z=0; z<bitmap.getBytesPP(); ++z)
				{
					const uint8 val_i = bitmap.getPixel(xi, yi)[z];
					const float val = (float)val_i;
					const float newval = f_d * 255.f * colour[z] + val * (1 - f_d);
					const uint8 newval_i = (uint8)myClamp(newval, 0.f, 255.9f);
					bitmap.getPixelNonConst(xi, yi)[z] = newval_i;
				}
			}
		}
	}
}


} // end namespace Drawing


#if BUILD_TESTS


#include "PNGDecoder.h"
#include "imformatdecoder.h"
#include "../indigo/TestUtils.h"


void Drawing::test()
{
	const int W = 800;
	Bitmap bitmap(W, W, 3, NULL);

	bitmap.zero();

	// Draw lines facing down and right
	drawLine(bitmap, Colour3f(1.f), Vec2f(400, 400), Vec2f(700, 500));

	drawLine(bitmap, Colour3f(0.4f, 0.6f, 1.0f), Vec2f(400, 400), Vec2f(600, 700));

	// Draw lines facing left and up
	drawLine(bitmap, Colour3f(0.6f, 1.f, 0.4f), Vec2f(400, 400), Vec2f(100, 300));

	drawLine(bitmap, Colour3f(1.f, 0.6f, 0.4f), Vec2f(400, 400), Vec2f(200, 100));

	try
	{
		PNGDecoder::write(bitmap, "drawing_test.png");
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}
}


#endif
