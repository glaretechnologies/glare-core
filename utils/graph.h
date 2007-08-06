/*===================================================================

  
  digital liberation front 2001
  
  _______    ______      _______
 /______/\  |______|    /\______\  
|       \ \ |      |   / /       |    
|	      \| |      |  |/         |  
|_____    \ |      |_ /    ______|       
 ____|    | |      |_||    |_____          
     |____| |________||____|                
           



Code by Nicholas Chapman
nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may not use this code for any commercial project.
====================================================================*/
#ifndef __GRAPH_H__
#define __GRAPH_H__

class Graphics2d;

#include "..\maths\vector.h"
#include <string>

class Graph
{
public:
	Graph(const std::string& name, float lower_y, float upper_y, int width, int height, bool autoscale = false);
	~Graph();

	bool setAutoscale(bool on);

	void plotValue(float value);


	void draw(Graphics2d& graphics, int xpos, int ypos);

	inline int getWidth() const { return width; }
	inline int getHeight() const { return height; }

private:
	inline float smallestMultipleOfXGreaterEqual(float x, float height)
	{
		const float partial_num_xs_in_height = ceil(height / x); 
		return partial_num_xs_in_height * x;
	}

	Vector values;
	int values_index;
	
	float lower_y;
	float upper_y;

	int width;
	int height;

	bool autoscale_on;
	std::string name;

	float graph_to_sceen_factor;
	float y_low_graphspace;
	float ypos;

	inline float graphToScreenSpace(float y)
	{
		const float val_above_ylow = y - y_low_graphspace;

		const float screenspace_above_y_low = val_above_ylow * graph_to_sceen_factor;

		return (ypos + height) - screenspace_above_y_low;
	}
};









#endif //__GRAPH_H__