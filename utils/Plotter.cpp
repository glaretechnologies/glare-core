/*=====================================================================
Plotter.cpp
-----------
File created by ClassTemplate on Tue Jun 02 12:43:55 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "Plotter.h"


#include <fstream>
#include "platformutils.h"
#include "../utils/stringutils.h"


Plotter::Plotter()
{

}


Plotter::~Plotter()
{

}


void Plotter::plot(
		const std::string& path,
		const std::string& title,
		const std::string& x_label,
		const std::string& y_label,
		const std::vector<DataSet>& data
		)
{
	const std::string temp_data_path = "c:/temp/plotdata";

	// Write data
	for(unsigned int i=0; i<data.size(); ++i)
	{
		std::ofstream f((temp_data_path + ::toString(i) + ".txt").c_str());

		for(size_t z=0; z<data[i].points.size(); ++z)
			f << data[i].points[z].x << " " << data[i].points[z].y << "\n";
	}


	// Write Gnuplot control script
	const std::string temp_path = "c:/temp/plot.txt";

	{
		std::ofstream f(temp_path.c_str());

		f << "set terminal png size 1000,800\n";
		f << "set output \"" + path + "\"\n";
		f << "set style data lines\n";
		f << "set grid linetype rgb \"blue\"  lw 0.4\n";
		f << "set style data linespoints\n";
		f << "set xlabel \"" + x_label + "\"\n";
		f << "set ylabel \"" + y_label + "\"\n";
		f << "set title \"" + title + "\"\n";
		//f << "set encoding iso_8859_1\n"
		f << "plot ";
		for(unsigned int i=0; i<data.size(); ++i)
			f << "\"" + temp_data_path + ::toString(i) + ".txt" + "\" using 1:2 title \"" + data[i].key + "\" " + (i < data.size() - 1 ? "," : "");
		f << "\n";
	}

	// Execute Gnuplot
	const int result = PlatformUtils::execute("pgnuplot " + temp_path);

	//printVar(result);
}


void Plotter::plot3D(
		const std::string& path,
		const std::string& title,
		const std::string& x_label,
		const std::string& y_label,
		const std::string& key,
		const Array2d<Vec3f>& data,
		PlotOptions options
		)
{
	const std::string temp_data_path = "c:/temp/plotdata.txt";

	// Write data
	{
		std::ofstream f(temp_data_path.c_str());

		for(unsigned int y=0; y<data.getHeight(); ++y)
		{
			for(unsigned int x=0; x<data.getWidth(); ++x)
				f << data.elem(x, y).x << " " << data.elem(x, y).y << " " << data.elem(x, y).z << "\n";

			f << "\n";
		}
	}


	// Write Gnuplot control script
	const std::string temp_path = "c:/temp/plot.txt";

	{
		std::ofstream f(temp_path.c_str());

		f << "set terminal png size " << options.w << "," << options.h << "\n";
		f << "set output \"" + path + "\"\n";
		f << "set style data lines\n";
		f << "set grid linetype rgb \"blue\"  lw 0.4\n";
		f << "set style data linespoints\n";
		f << "set xlabel \"" + x_label + "\"\n";
		f << "set ylabel \"" + y_label + "\"\n";
		f << "set title \"" + title + "\"\n";
		//f << "set encoding iso_8859_1\n"
		f << "set contour\n";
		f << "set cntrparam levels 10\n";

		f << "splot	\"" + temp_data_path + "\" using 1:2:3 with lines title \"" + key + "\"\n";
	}

	// Execute Gnuplot
	const int result = PlatformUtils::execute("pgnuplot " + temp_path);
}
