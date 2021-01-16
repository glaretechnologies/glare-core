/*=====================================================================
Plotter.h
---------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include <string>
#include <vector>
#include "../maths/vec2.h"
#include "../maths/vec3.h"
#include "../utils/Array2D.h"


/*=====================================================================
Plotter
-------
Draws a graph and saves it to disk using Gnuplot.
pgnuplot has to be on your path for this to work.
=====================================================================*/
class Plotter
{
public:
	struct DataSet
	{
		DataSet() {}
		DataSet(const std::string& key_) : key(key_) {}
		std::string key;
		std::vector<Vec2f> points;
	};

	struct PlotOptions
	{
		PlotOptions() : w(1800), h(1200), x_axis_log(false), y_axis_log(false), r_log(false), polar(false), z_rot(30), explicit_x_range(false), explicit_y_range(false), explicit_z_range(false) {}
		int w, h;
		bool x_axis_log;
		bool y_axis_log;
		bool r_log;
		bool polar;
		double z_rot;

		bool explicit_x_range;
		double x_start;
		double x_end;

		bool explicit_y_range;
		double y_start;
		double y_end;

		bool explicit_z_range;
		double z_start;
		double z_end;

		std::vector<float> vertical_lines;
		std::vector<float> horizontal_lines;
	};

	static void plot(
		const std::string& path,
		const std::string& title,
		const std::string& x_label,
		const std::string& y_label,
		const std::vector<DataSet>& data,
		PlotOptions options = PlotOptions()
	);

	// throws glare::Exception on failure.
	static void plot3D(
		const std::string& path,
		const std::string& title,
		const std::string& x_label,
		const std::string& y_label,
		const std::string& key,
		const Array2D<Vec3f>& data,
		PlotOptions options = PlotOptions()
	);

	static void scatterPlot(
		const std::string& path,
		const std::string& title,
		const std::string& x_label,
		const std::string& y_label,
		const std::vector<DataSet>& data,
		PlotOptions options = PlotOptions()
	);

};
