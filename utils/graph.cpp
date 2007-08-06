#include "graph.h"

#include "../graphics2d/graphics2d.h"
#include <assert.h>
#include "stringutils.h"
#include "../maths/maths.h"

//TEMP
//#include "C:\programming\speech recognition 2\main.h"

Graph::Graph(const std::string& name_, float lower_y_, float upper_y_, int width_, int height_, bool autoscale)
:	values(width_),
	name(name_)
{
	values.zero();
	values_index = 0;

	lower_y = lower_y_;
	upper_y = upper_y_;

	assert(upper_y >= lower_y);

	
	width = width_;
	height = height_;
	assert(width >= 0 && height >= 0);

	autoscale_on = autoscale;
}

Graph::~Graph()
{
}

bool Graph::setAutoscale(bool on)
{
	bool oldon = autoscale_on;

	autoscale_on = on;

	return oldon;
}

void Graph::plotValue(float value)
{
	values[values_index] = value;

	values_index++;

	//-----------------------------------------------------------------
	//check for wrap around to beginning of array
	//-----------------------------------------------------------------
	if(values_index >= values.size())
	{
		values_index = 0;
	}
}


void Graph::draw(Graphics2d& graphics, int xpos, int ypos_)
{
	this->ypos = ypos_;

	//-----------------------------------------------------------------
	//blank out graph background
	//-----------------------------------------------------------------	
	graphics.drawRect(Vec2(xpos, ypos), width, height, Vec3(0, 0, 0));


	//-----------------------------------------------------------------
	//draw box around graph
	//-----------------------------------------------------------------
	const Vec3 boxcolour(0.3, 0.3, 0.3);
	graphics.drawLine(Vec2(xpos, ypos), Vec2(xpos + width, ypos),					boxcolour);//top 
	graphics.drawLine(Vec2(xpos, ypos), Vec2(xpos, ypos + height),					boxcolour);//left
	graphics.drawLine(Vec2(xpos + width, ypos),  Vec2(xpos + width, ypos + height), boxcolour);//right
	graphics.drawLine(Vec2(xpos, ypos + height), Vec2(xpos + width, ypos + height), boxcolour);//bottom

	//-----------------------------------------------------------------
	//draw graph name
	//-----------------------------------------------------------------
	graphics.drawText(Vec2(xpos+50, ypos), name, Vec3(0.8, 0.8, 0.8));	

	//-----------------------------------------------------------------
	//calc scale
	//-----------------------------------------------------------------
	float y_low;//lower bound in graph space
	float y_high;//upper bound in graph space

	if(autoscale_on)
	{
		//-----------------------------------------------------------------
		//find largest value
		//-----------------------------------------------------------------
		y_high = -10000000;

		for(int i=0; i<values.size(); ++i)
		{
			if(values[i] > y_high)
				y_high = values[i];
		}

		y_low = 10000000;

		for(i=0; i<values.size(); ++i)
		{
			if(values[i] < y_low)
				y_low = values[i];
		}
	}
	else
	{
		y_low = lower_y;
		y_high = upper_y;
	}

	this->y_low_graphspace = y_low;

	//-----------------------------------------------------------------
	//calc range and y_scale 
	//-----------------------------------------------------------------
	float range_graphspace = y_high - y_low;
	//assert(range > 0);
	if(range_graphspace == 0)
		range_graphspace = 10;

	//if(range_graphspace <


	//range = 1;
	//height = 100;
	this->graph_to_sceen_factor = (float)height / range_graphspace;//= 100

	//-----------------------------------------------------------------
	//find appropriate graph-space scale of markings for y axis
	//-----------------------------------------------------------------
	const int MAX_NUM_MARKINGS = height / 20;//marking every 20 pixels

	float graphspace_markinggap = 0.00001;


	while(1)
	{

		const float nummarkings = range_graphspace / graphspace_markinggap;

		if(nummarkings < MAX_NUM_MARKINGS)
			break;

		graphspace_markinggap *= 10;
	}

	assert(graphspace_markinggap > 0);

	//-----------------------------------------------------------------
	//draw markings
	//-----------------------------------------------------------------
	const int MARK_WIDTH = 7;

	const float screenspace_marking_gap = graphspace_markinggap * graph_to_sceen_factor;



	//-----------------------------------------------------------------
	//calc value of the first mark in graph space.  (ie its label)
	//-----------------------------------------------------------------
	//value of the bottom label
	//int mark_factor = ceil(y_low / graphspace_markinggap);
	//float mark_label = (float)mark_factor *  graphspace_markinggap;

	float mark_label = smallestMultipleOfXGreaterEqual(graphspace_markinggap, y_low);


	
	//-----------------------------------------------------------------
	//calc value of the first mark in screen space.
	//-----------------------------------------------------------------
	/*int value_above_y_low = mark_label - y_low;//in graph space

	int dist_above_graph_bottom;
	
	if(value_above_y_low == 0)
	{
		dist_above_graph_bottom = 0;
	}
	else
	{
		dist_above_graph_bottom	= (ypos + height) % value_above_y_low;
	}



	float screenspace_marking_y = (ypos + height) - dist_above_graph_bottom;*/

	//float screenspace_marking_y = (ypos + height) - graph_to_sceen_factor * mark_label;

	float screenspace_marking_y = graphToScreenSpace(mark_label);

	//TEMP NO ASSERT assert((int)screenspace_marking_y >= ypos && (int)screenspace_marking_y <= (ypos + height));
	//assert it is on the graph

	//-----------------------------------------------------------------
	//draw the marks and labels on the marks up the y-axis.
	//-----------------------------------------------------------------
	while(screenspace_marking_y > ypos)
	{
		//-----------------------------------------------------------------
		//draw label
		//-----------------------------------------------------------------
		graphics.drawText(Vec2(xpos+MARK_WIDTH, screenspace_marking_y-5), floatToString(mark_label, 3), Vec3(0.8, 0.8, 0.8)  );//buildString("%1.2f", mark_label));

		//-----------------------------------------------------------------
		//update label
		//-----------------------------------------------------------------
		mark_label += graphspace_markinggap;

		//-----------------------------------------------------------------
		//draw mark
		//-----------------------------------------------------------------
		graphics.drawLine(Vec2(xpos, screenspace_marking_y), 
			Vec2(xpos + MARK_WIDTH, screenspace_marking_y), Vec3(0, 0, 0.8) );

		screenspace_marking_y -= screenspace_marking_gap;
	}




	//-----------------------------------------------------------------
	//draw y = 0 line
	//-----------------------------------------------------------------
	if(y_low <= 0 && y_high >= 0)//if y=0 will be on graph
	{
		const float line_y = ypos + graph_to_sceen_factor * y_high;
		graphics.drawLine(Vec2(xpos, line_y), Vec2(xpos + width, line_y), Vec3(0, 0, 0.8) );//top 
	}

	

	//-----------------------------------------------------------------
	//draw actual values
	//-----------------------------------------------------------------
	int last_y = ypos;//hack

	for(int i = 0; i<values.size(); ++i)
	{
		int index_to_use = (i + values_index) % values.size();
	
		const float value = values[index_to_use];

		if(value >= y_low && value <= y_high)//if value is not off the graph
		{
			//-----------------------------------------------------------------
			//calc x coord of pixel
			//-----------------------------------------------------------------
			int x = xpos + i;

			//-----------------------------------------------------------------
			//calc y coord of pixel
			//-----------------------------------------------------------------
			float floaty = (float)ypos + graph_to_sceen_factor * (y_high - value);
			int y = (int)floaty;

			assert(x >= xpos && x <= xpos + width);
			assert(y >= ypos && y <= ypos + height);

			//debugPrint("range: %f\n", range);
		//	debugPrint("y_scale: %f\n", y_scale);
		//	debugPrint("x: %i, y: %f\n", x, y);

			if(y == last_y)
			{
				graphics.drawPixel(x, y, Vec3(0, 0.8, 0));
			}
			else
				graphics.drawLine(Vec2(x, last_y), Vec2(x, (int)y), Vec3(0, 0.8, 0) ); 

			last_y = y;
		}
	}







}