/*=====================================================================
Plotter.cpp
-----------
File created by ClassTemplate on Tue Jun 02 12:43:55 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "Plotter.h"


#include <fstream>
#include "PlatformUtils.h"
#include "../indigo/globals.h"
#include "../utils/StringUtils.h"
#include "../utils/Exception.h"
#include "../utils/FileUtils.h"


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
		const std::vector<DataSet>& data,
		PlotOptions options
		)
{
	const std::string temp_dir = StringUtils::replaceCharacter(PlatformUtils::getTempDirPath(), '\\', '/'); // Gnuplot needs forwards slashes
	const std::string temp_data_path = temp_dir + "/plotdata.txt";

	// Write data
	for(unsigned int i=0; i<data.size(); ++i)
	{
		std::ofstream f(FileUtils::convertUTF8ToFStreamPath(temp_data_path + ::toString(i) + ".txt").c_str());

		for(size_t z=0; z<data[i].points.size(); ++z)
			f << data[i].points[z].x << " " << data[i].points[z].y << "\n";
	}


	// Write Gnuplot control script
	const std::string temp_path = temp_dir + "/plot.txt";

	{
		std::ofstream f(FileUtils::convertUTF8ToFStreamPath(temp_path).c_str());

		// NOTE: old terminal type was 'png'

		f << "set terminal pngcairo size " + toString(options.w) + "," + toString(options.h) + "\n";
		f << "set output \"" + path + "\"\n";
		if(options.polar)
		{
			float r_max = 0.5f;
			f << "set polar\n";
			f << "set grid polar\n";
			f << "set view equal xy\n";
			f << "set size ratio 0.5\n";
			f << "set xrange [-" << r_max << ":" << r_max << "]\n";
			f << "set yrange [0:" << r_max << "]\n";
			//f << "set xrange [-10:10]\n";
			//f << "set yrange [0:10]\n";
			f << "set xtics 0.5\n";
			//if(options.r_log)
			//	f << "set logscale r 10\n";

		}
		f << "set style data lines\n";
		//f << "set grid linetype rgb \"blue\"  lw 0.4\n";
		//f << "set style data linespoints\n";
		if(!options.polar)
		{
			f << "set xlabel \"" + x_label + "\"\n";
			f << "set ylabel \"" + y_label + "\"\n";
		}
		f << "set title \"" + title + "\"\n";
		//f << "set encoding iso_8859_1\n"
		if(options.x_axis_log)
			f << "set logscale x\n";
		if(options.y_axis_log)
			f << "set logscale y\n";
		f << "plot ";
		for(unsigned int i=0; i<data.size(); ++i)
			f << "\"" + temp_data_path + ::toString(i) + ".txt" + "\" using 1:2 title \"" + data[i].key + "\" " + (i < data.size() - 1 ? "," : "");
		f << "\n";
	}

	// Execute Gnuplot
	/*const int result = */PlatformUtils::execute("pgnuplot " + temp_path);

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

	try
	{
		const std::string temp_dir = StringUtils::replaceCharacter(PlatformUtils::getTempDirPath(), '\\', '/'); // Gnuplot needs forwards slashes
		const std::string temp_data_path = temp_dir + "/plotdata.txt";
	
		// Write data
		{
			std::ofstream f(FileUtils::convertUTF8ToFStreamPath(temp_data_path).c_str());

			for(unsigned int y=0; y<data.getHeight(); ++y)
			{
				for(unsigned int x=0; x<data.getWidth(); ++x)
					f << data.elem(x, y).x << " " << data.elem(x, y).y << " " << data.elem(x, y).z << "\n";

				f << "\n";
			}
		}


		// Write Gnuplot control script
		const std::string temp_path = temp_dir + "/plot.txt";

		conPrint("Writing plot instructions to '" + temp_path + "'...");

		{
			std::ofstream f(FileUtils::convertUTF8ToFStreamPath(temp_path).c_str());

			f << "set terminal pngcairo size " << options.w << "," << options.h << "\n";
			f << "set output \"" + path + "\"\n";
			f << "set style data lines\n";
			//f << "set grid linetype rgb \"blue\"  lw 0.4\n";
			f << "set grid linetype rgb \"blue\"\n";
			f << "set style data linespoints\n";
			f << "set xlabel \"" + x_label + "\"\n";
			f << "set ylabel \"" + y_label + "\"\n";
			f << "set title \"" + title + "\"\n";
			//f << "set encoding iso_8859_1\n"
			f << "set contour\n";
			f << "set cntrparam levels 10\n";
			f << "set view 60, " + toString(options.z_rot) + "\n"; // TEMP

			f << "splot	\"" + temp_data_path + "\" using 1:2:3 with lines title \"" + key + "\"\n";
		}

		// Execute Gnuplot
		const int result = PlatformUtils::execute("pgnuplot " + temp_path);

		conPrint("Gnuplot result: " + toString(result));

	}
	catch(PlatformUtils::PlatformUtilsExcep& e)
	{
		throw Indigo::Exception(e.what());
	}
}
