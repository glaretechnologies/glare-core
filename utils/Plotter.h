/*=====================================================================
Plotter.h
---------
File created by ClassTemplate on Tue Jun 02 12:43:55 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PLOTTER_H_666_
#define __PLOTTER_H_666_


#include <string>
#include <vector>
#include "../maths/vec2.h"
#include "../maths/vec3.h"
#include "../utils/array2d.h"


/*=====================================================================
Plotter
-------
Draws a graph and saves it to disk using Gnuplot.
pgnuplot has to be on your path for this to work.
=====================================================================*/
class Plotter
{
public:
	/*=====================================================================
	Plotter
	-------

	=====================================================================*/
	Plotter();

	~Plotter();


	struct DataSet
	{
		std::string key;
		std::vector<Vec2f> points;
	};

	struct PlotOptions
	{
		PlotOptions() : w(1000), h(800), x_axis_log(false), y_axis_log(false), r_log(false), polar(false), z_rot(30) {}
		int w, h;
		bool x_axis_log;
		bool y_axis_log;
		bool r_log;
		bool polar;
		float z_rot;
	};

	static void plot(
		const std::string& path,
		const std::string& title,
		const std::string& x_label,
		const std::string& y_label,
		const std::vector<DataSet>& data,
		PlotOptions options = PlotOptions()
		);

	// throws Indigo::Exception on failure.
	static void plot3D(
		const std::string& path,
		const std::string& title,
		const std::string& x_label,
		const std::string& y_label,
		const std::string& key,
		const Array2d<Vec3f>& data,
		PlotOptions options = PlotOptions()
		);

};



#endif //__PLOTTER_H_666_




