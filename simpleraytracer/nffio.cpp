/*=====================================================================
nffio.cpp
---------
File created by ClassTemplate on Fri Nov 19 08:04:42 2004
Code By Nicholas Chapman.

Feel free to use for any commercial or other purpose
=====================================================================*/
#include "nffio.h"


#include <fstream>
#include <assert.h>



//throws NFFioExcep
void NFFio::writeNFF(const std::string& pathname, const std::vector<float>& imgdata,
							int width, int height)
{
	const int num_image_floats = width * height * 3;

	assert(imgdata.size() >= num_image_floats);

	if(imgdata.size() < num_image_floats)
		throw NFFioExcep("imgdata.size() is too small");

	//------------------------------------------------------------------------
	//open file
	//------------------------------------------------------------------------
	std::ofstream outfile(pathname.c_str(), std::ios::binary);

	if(!outfile)
		throw NFFioExcep("could not open file '" + pathname + "' for writing.");

	//------------------------------------------------------------------------
	//write dimensions
	//------------------------------------------------------------------------
	outfile.write(reinterpret_cast<const char*>(&width), sizeof(int));
	outfile.write(reinterpret_cast<const char*>(&height), sizeof(int));

	//------------------------------------------------------------------------
	//write img data
	//------------------------------------------------------------------------
	for(int i=0; i<num_image_floats; ++i)
		outfile.write(reinterpret_cast<const char*>(&imgdata[i]), sizeof(float));

	/*for(int x=0; x<width; ++x)
		for(int y=0; y<height; ++y)
			for(int c=0; c<3; ++c)
			{
				outfile.write(reinterpret_cast<const char*>(&imgdata[(x + y*(width)) * 3 + c]), sizeof(float));
			}*/

	if(!outfile.good())
		throw NFFioExcep("an IO error occurred while writing file.");
}


	//throws NFFioExcep
void NFFio::readNNF(const std::string& pathname, std::vector<float>& imgdata_out, 
							int& width_out, int& height_out)
{
	std::ifstream infile(pathname.c_str(), std::ios::binary);
	
	if(!infile)
		throw NFFioExcep("could not open file '" + pathname + "' for reading.");

	//------------------------------------------------------------------------
	//read dimensions
	//------------------------------------------------------------------------
	infile.read(reinterpret_cast<char*>(&width_out), sizeof(int));
	infile.read(reinterpret_cast<char*>(&height_out), sizeof(int));

	if(width_out < 0 || width_out > 1e9)
		throw NFFioExcep("Invalid image width");
	if(height_out < 0 || height_out > 1e9)
		throw NFFioExcep("Invalid image width");

	//------------------------------------------------------------------------
	//read image data
	//------------------------------------------------------------------------
	const int num_image_floats = width_out * height_out * 3;
	imgdata_out.resize(num_image_floats);

	for(int i=0; i<num_image_floats; ++i)
		infile.read(reinterpret_cast<char*>(&imgdata_out[i]), sizeof(float));

	if(!infile.good())
		throw NFFioExcep("an IO error occurred while reading file.");
}





