#include "staticgraph.h"

//#include "stdio.h"
//#include "C:/crytek/cai/cai_caifile.h"

#include "assert.h"
#include <vector>
#include "../graphics2d/graphics2d.h"
#include "../maths/maths.h"

#include "../utils/utils.h"


StaticGraph::StaticGraph(/*const std::string& name_*/)
:	datapoints(1)/*, name(name_)*/
{

	mousepos.set(-666, -666);

}
StaticGraph::~StaticGraph()
{

}

const std::string readLine(cai::CAIFile& file)
{
	std::string token;

	while(1)
	{
		char c = file.readChar();

		if(c == '\n' || c == 13 || file.endOfFile())
			break;
	
		token += charToString(c);
	}

	return token;
}

const std::string readToken(cai::CAIFile& file)
{
	std::string token;

	char c = -1;
	//-----------------------------------------------------------------
	//read in whitespace before token
	//-----------------------------------------------------------------

	while(1)
	{
		c = file.readChar();
	
		if(file.endOfFile())
			return token;

		if(isWhitespace(c))
			continue;
		else
			break;
	}

	//-----------------------------------------------------------------
	//read in token
	//-----------------------------------------------------------------
	while(1)
	{

		if(c == EOF)
			return token;

		if(isWhitespace(c))
			return token;
		else
		{
			token += charToString(c);
		}

		c = file.readChar();

		if(file.endOfFile())
			return token;
	}
}

		


void StaticGraph::enterData(const std::string& filename)
{
	try
	{
		cai::CAIFile file(filename, "rb");

		assert(file.isOpen());

		name = readLine(file);

		xaxislabel = readToken(file);//NOTE: this should be changed to readQuote()
		yaxislabel = readToken(file);


		std::vector<Vec2> data;

		while(1)
		{

			Vec2 newpoint;

			//-----------------------------------------------------------------
			//read in x component of point
			//-----------------------------------------------------------------
			const std::string tokenstring = readToken(file);

			if(tokenstring.size() == 0)//if EOF
				break;	

			newpoint.x = stringToFloat(tokenstring);

			//-----------------------------------------------------------------
			//read in y component of point
			//-----------------------------------------------------------------
			const std::string ystring = readToken(file);

			if(ystring.size() == 0)//if EOF
			{
				//NOTE: report error in file here, shopuld be y component here.

				break;	
			}

			newpoint.y = stringToFloat(ystring);

			//-----------------------------------------------------------------
			//add new point to list of points
			//-----------------------------------------------------------------
			data.push_back(newpoint);
		}

		if(data.size() == 0)//NOTE: dodgy
		{
			throw EnterDataException("no data found in file [may be parse error]");
		}


		//-----------------------------------------------------------------
		//copy over to member data array
		//-----------------------------------------------------------------
		datapoints.resize(data.size());

		for(int i=0; i<datapoints.getSize(); i++)
		{
			datapoints[i] = data[i];
		}



		return;

	}
	catch(cai::FileNotFoundExcep& e)
	{
		//assert(0);// :)
		throw EnterDataException("failed to open file '" + filename + "' for reading.");
	}

}
	
void StaticGraph::enterData(const Array<Vec2>& newdatapoints, const std::string& graph_name,
		const std::string& xlabel, const std::string& ylabel)
{
	name = graph_name;
	xaxislabel = xlabel;
	yaxislabel = ylabel;

	datapoints = newdatapoints;

}

void StaticGraph::draw(Graphics2d& graphics, int xpos, int ypos, int drawwidth, int drawheight)
{
	//-----------------------------------------------------------------
	//get maxy GRAPHSPACE
	//-----------------------------------------------------------------
	float maxy_gs = -9999999;

	for(int i=0; i<datapoints.getSize(); i++)
	{
		if(datapoints[i].y >= maxy_gs)
			maxy_gs = datapoints[i].y;
	}

	//-----------------------------------------------------------------
	//get miny GRAPHSPACE
	//-----------------------------------------------------------------
	float miny_gs = 999999999;

	for(i=0; i<datapoints.getSize(); i++)
	{
		if(datapoints[i].y <= miny_gs)
			miny_gs = datapoints[i].y;
	}

	float y_range_gs = maxy_gs - miny_gs;




	//-----------------------------------------------------------------
	//get maxx GRAPHSPACE
	//-----------------------------------------------------------------
	float maxx_gs = -9999999;

	for(i=0; i<datapoints.getSize(); i++)
	{
		if(datapoints[i].x >= maxx_gs)
			maxx_gs = datapoints[i].x;
	}

	//-----------------------------------------------------------------
	//get miny GRAPHSPACE
	//-----------------------------------------------------------------
	float minx_gs = 999999999;

	for(i=0; i<datapoints.getSize(); i++)
	{
		if(datapoints[i].x <= minx_gs)
			minx_gs = datapoints[i].x;
	}

	float x_range_gs = maxx_gs - minx_gs;





	const float BORDERWIDTH = 40;

	const float screenspace_available_x = drawwidth - BORDERWIDTH*2;
	const float screenspace_available_y = drawheight - BORDERWIDTH*2;

	const Vec2 screenoffset(xpos, ypos);


	graph_to_screen_factor = Vec2(	screenspace_available_x / x_range_gs, 
									screenspace_available_y / y_range_gs);
									

	const Vec2 graph_topleft_ss = Vec2(BORDERWIDTH, BORDERWIDTH) + screenoffset;
	const Vec2 graph_botright_ss = Vec2(drawwidth - BORDERWIDTH, drawheight - BORDERWIDTH) + screenoffset;

	const Vec2 graph_topright_ss = Vec2(drawwidth - BORDERWIDTH, BORDERWIDTH) + screenoffset;
	this->graph_botleft_ss = Vec2(BORDERWIDTH, drawheight - BORDERWIDTH) + screenoffset;



	//-----------------------------------------------------------------
	//draw box around graph
	//-----------------------------------------------------------------
	const Vec3 heavyboxcolour(0.3, 0.3, 0.3);
	const Vec3 lightboxcolour(0.8, 0.8, 0.8);
	graphics.drawLine(graph_topleft_ss, graph_topright_ss,		lightboxcolour);//top 
	graphics.drawLine(graph_topright_ss, graph_botright_ss,		lightboxcolour);//right
	graphics.drawLine(graph_botright_ss,  graph_botleft_ss,		heavyboxcolour);//bottom
	graphics.drawLine(graph_botleft_ss, graph_topleft_ss,		heavyboxcolour);//left

	//-----------------------------------------------------------------
	//draw graph name
	//-----------------------------------------------------------------
	graphics.drawText(Vec2(100 /*drawwidth/2 - 45*/, ypos+10), name, Vec3(0.8, 0.8, 0.8));	



	const Vec3 LABEL_COLOUR(0.3, 0.8, 0);
	const Vec3 MARK_COLOUR(0.3, 0,  0.6);
	const float MARK_LENGTH = 10;



	//-----------------------------------------------------------------
	//draw labels on x axis...
	//-----------------------------------------------------------------
	graphics.drawLine(graph_botleft_ss, graph_botleft_ss + Vec2(0, MARK_LENGTH), MARK_COLOUR);
		//draw left mark

	graphics.drawLine(graph_botright_ss, graph_botright_ss + Vec2(0, MARK_LENGTH), MARK_COLOUR);
		//draw right mark

	//-----------------------------------------------------------------
	//draw leftmost label
	//-----------------------------------------------------------------
	const float LEFT_OFFSET = -10;
	graphics.drawText(graph_botleft_ss + Vec2(LEFT_OFFSET, MARK_LENGTH + 2), floatToString(minx_gs), LABEL_COLOUR);
	
	//-----------------------------------------------------------------
	//draw rightmost label
	//-----------------------------------------------------------------
	graphics.drawText(graph_botright_ss + Vec2(LEFT_OFFSET, MARK_LENGTH + 2), floatToString(maxx_gs), LABEL_COLOUR);




	//-----------------------------------------------------------------
	//draw labels on y axis...
	//-----------------------------------------------------------------
	graphics.drawLine(graph_topleft_ss, graph_topleft_ss - Vec2(MARK_LENGTH, 0), MARK_COLOUR);
		//draw top mark

	graphics.drawLine(graph_botleft_ss, graph_botleft_ss - Vec2(MARK_LENGTH, 0), MARK_COLOUR);
		//draw bottom mark

	//-----------------------------------------------------------------
	//draw top label
	//-----------------------------------------------------------------
	const Vec2 LABELTEXTOFFSET(-1*(BORDERWIDTH - 4), -9);//offset from corresponding point on y axis.

	graphics.drawText(graph_topleft_ss + LABELTEXTOFFSET, floatToString(maxy_gs), LABEL_COLOUR);
	
	//-----------------------------------------------------------------
	//draw rightmost label
	//-----------------------------------------------------------------
	graphics.drawText(graph_botleft_ss + LABELTEXTOFFSET, floatToString(miny_gs), LABEL_COLOUR);



	//-----------------------------------------------------------------
	//draw x and y axis labels
	//-----------------------------------------------------------------
	graphics.drawText(Vec2(drawwidth/2, drawheight - 25), xaxislabel, Vec3(0,0,0));//draw y axis label 
	graphics.drawText(Vec2(3, drawheight/2), yaxislabel, Vec3(0,0,0));//draw y axis label 





	//-----------------------------------------------------------------
	//draw datapoints
	//-----------------------------------------------------------------
	
	//-----------------------------------------------------------------
	//draw first point
	//-----------------------------------------------------------------
	drawPoint(  Vec2(datapoints[0].x - minx_gs, datapoints[0].y - miny_gs) , graphics);

	const Vec3 LINECOLOUR(0, 1, 0);

	for(i=1; i<datapoints.getSize(); i++)
	{
		//-----------------------------------------------------------------
		//draw line from last point to this point
		//-----------------------------------------------------------------
		graphics.drawLine(graphToScreenSpace( Vec2(datapoints[i-1].x - minx_gs, datapoints[i-1].y - miny_gs) ), 
							graphToScreenSpace( Vec2(datapoints[i].x - minx_gs, datapoints[i].y - miny_gs) ), 
							LINECOLOUR);

		//-----------------------------------------------------------------
		//draw this point
		//-----------------------------------------------------------------
		drawPoint( Vec2(datapoints[i].x - minx_gs, datapoints[i].y - miny_gs), graphics);
	}





	//-----------------------------------------------------------------
	//draw coords at mouse pointer
	//-----------------------------------------------------------------
	graphics.drawText(mousepos, "(" + floatToString(mousepos.x)
						+ ", " + floatToString(mousepos.y) + ")", Vec3(0,0,0));

}

void StaticGraph::drawPoint(const Vec2& point_graphspace, Graphics2d& graphics)
{

	const Vec2 pos_ss = graphToScreenSpace(point_graphspace);

	const float CROSS_LINE_LENGTH = 10;
	const Vec3 CROSSCOLOUR(0.6, 0, 0);

	//-----------------------------------------------------------------
	//draw horizontal line
	//-----------------------------------------------------------------
	graphics.drawLine(pos_ss - Vec2(CROSS_LINE_LENGTH/2, 0), 
						pos_ss + Vec2(CROSS_LINE_LENGTH/2, 0), CROSSCOLOUR);

	//-----------------------------------------------------------------
	//draw vertical line
	//-----------------------------------------------------------------
	graphics.drawLine(pos_ss - Vec2(0, CROSS_LINE_LENGTH/2), 
						pos_ss + Vec2(0, CROSS_LINE_LENGTH/2), CROSSCOLOUR);

}


const Vec2 StaticGraph::graphToScreenSpace(const Vec2& point_gs)
{
	return Vec2(graph_botleft_ss.x + point_gs.x * graph_to_screen_factor.x, 
				graph_botleft_ss.y - point_gs.y * graph_to_screen_factor.y);
}

/*
const Vec2 StaticGraph::screenToGraphSpace(const Vec2& point_ss)
{
	const Vec2 withoutborder = point_ss - graph_topleft_ss;

	return Vec2(point_ss.x / graph_to_screen_factor.x, 
				point_ss.y / graph_to_screen_factor.y);
}*/



void StaticGraph::setMousePos(const Vec2& mousepos_)
{
	mousepos = mousepos_;
}
