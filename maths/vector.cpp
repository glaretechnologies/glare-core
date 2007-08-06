#include "vector.h"


#include "../maths/mathstypes.h"


/*
//template <class double>
void Vector<double>::test()
{
	const double d[] = {0, 1, 2};
	Vector<double> v(3, d);

	assert(::epsEqual(v.averageValue(), 1.0));
	assert(::epsEqual(v.variance(), 0.6666666));
}
*/














//#include "C:\programming\graphics2d\graphics2d.h"
//#include "types.h"//for myMin() etc

#if 0
void Vector::draw(Graphics2d& graphics, int xpos, int ypos, int maxbaramp, float scale, 
				  float width, const Vec3& colour) const
{
	assert(dimension);
	//-----------------------------------------------------------------
	//blank out graph background
	//-----------------------------------------------------------------
	graphics.drawRect(Vec2(xpos, ypos-maxbaramp), width, maxbaramp*2, Vec3(0,0,0));
	/*
	{	
	for(int x=xpos; x<xpos + width; x++)
		for(int y=ypos - maxbaramp; y<ypos + maxbaramp; y++)
		{
			graphics.drawPixel(x, y, 0, 0, 0);
		}
	}*/

	//-----------------------------------------------------------------
	//draw the graph
	//-----------------------------------------------------------------
	const float BAR_WIDTH = width / (float)dimension;


	for(int bar=0; bar<dimension; bar++)
	{
		
		int barleftx = xpos + (int)((float)bar * BAR_WIDTH);

		int barheight = (int)(data[bar] * scale);
		

		//-----------------------------------------------------------------
		//clip bar
		//-----------------------------------------------------------------
		if(barheight > maxbaramp)
			barheight = maxbaramp;
		else if(barheight < -maxbaramp)
			barheight = -maxbaramp;

		int endbar_y = ypos - barheight;//minus as positive amplitude should go up the screen

		int small_y = myMin(endbar_y, ypos);
		int big_y = myMax(endbar_y, ypos);

		assert(big_y >= small_y);

		graphics.drawRect(Vec2(barleftx, small_y), BAR_WIDTH, big_y - small_y, colour); 
		/*for(int x=barleftx; x< barleftx+BAR_WIDTH; x++)//for each vertical set of pixels in the bar
		{		
			for(int y=small_y; y<big_y; y++)
			{
				graphics.drawPixel(x, y, r, g, b);
			}
		
		}*/
	}


	//-----------------------------------------------------------------
	//draw lines around the graph
	//-----------------------------------------------------------------

	//-----------------------------------------------------------------
	//draw line across top
	//-----------------------------------------------------------------
	graphics.drawLine(Vec2(xpos, ypos - maxbaramp), Vec2(xpos + width, ypos - maxbaramp), Vec3(0.3, 0.3, 0.3)); 

	//-----------------------------------------------------------------
	//draw line across bottom
	//-----------------------------------------------------------------
	graphics.drawLine(Vec2(xpos, ypos + maxbaramp), Vec2(xpos + width, ypos + maxbaramp), Vec3(0.3, 0.3, 0.3));

	//-----------------------------------------------------------------
	//draw line down left hand side
	//-----------------------------------------------------------------
	graphics.drawLine(Vec2(xpos, ypos - maxbaramp), Vec2(xpos, ypos + maxbaramp), Vec3(0.3, 0.3, 0.3));

	//-----------------------------------------------------------------
	//draw line down right hand side
	//-----------------------------------------------------------------
	graphics.drawLine(Vec2(xpos + width, ypos - maxbaramp), Vec2( xpos + width, ypos + maxbaramp), Vec3(0.3, 0.3, 0.3));
}
#endif