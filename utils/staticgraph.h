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
#ifndef __STATICGRAPH_H__
#define __STATICGRAPH_H__

#include "array.h"
#include "../maths/vec2.h"
#include <string>
class Graphics2d;



class EnterDataException
{
public:
	EnterDataException(const std::string& message_)
		:	message(message_){}
	~EnterDataException(){}

	std::string message;
};



class StaticGraph
{
public:
	StaticGraph(/*const std::string& name*/);
	~StaticGraph();


	void enterData(const std::string& filename) throw(EnterDataException);

	void enterData(const Array<Vec2>& datapoints, const std::string& graph_name,
		const std::string& xaxis_label, const std::string& yaxis_label);

	void draw(Graphics2d& graphics, int xpos, int ypos, int drawwidth, int drawHeight);

	void setMousePos(const Vec2& mousepos);

private:
	std::string name;
	std::string xaxislabel;
	std::string yaxislabel;

	Array<Vec2> datapoints;

	Vec2 graph_to_screen_factor;
//	Vec2 graph_topleft_ss;
	Vec2 graph_botleft_ss;

	const Vec2 graphToScreenSpace(const Vec2& point_gs);
//	const Vec2 screenToGraphSpace(const Vec2& point_ss);

	void drawPoint(const Vec2& point_graphspace, Graphics2d& graphics);

	Vec2 mousepos;
};



#endif //__STATICGRAPH_H__