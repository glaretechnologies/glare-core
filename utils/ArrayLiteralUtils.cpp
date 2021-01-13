/*=====================================================================
ArrayLiteralUtils.cpp
---------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "ArrayLiteralUtils.h"


#include "MemMappedFile.h"
#include "Parser.h"
#include "Exception.h"
#include "FileUtils.h"


namespace ArrayLiteralUtils
{


const std::vector<float> CSVData::getColumnAsFloatVector(size_t col_i) const
{
	std::vector<float> res(data.getHeight());
	for(size_t i=0; i<data.getHeight(); ++i)
		res[i] = (float)data.elem(col_i, i);
	return res;
}


void loadCSVFile(const std::string& CSV_path, CSVData& data_out)
{
	MemMappedFile file(CSV_path);

	Parser parser((const char*)file.fileData(), file.fileSize());

	string_view line0;
	parser.parseLine(line0);
	const size_t num_cols = ::split(line0.to_string(), ',').size();
	parser.setCurrentPos(0);

	size_t num_rows = 0;
	while(!parser.eof())
	{
		string_view line;
		parser.parseLine(line);
		if(!(line.empty() || ::isAllWhitespace(line.to_string())))
			num_rows++;
	}

	data_out.data.resize(num_cols, num_rows);

	parser.setCurrentPos(0);

	int row = 0;
	while(!parser.eof())
	{
		if(row >= num_rows)
			throw glare::Exception("Failed to find expected number of rows.");

		for(int c=0; c<num_cols; ++c)
		{
			if(!parser.parseDouble(data_out.data.elem(c, row)))
				throw glare::Exception("Parse of number failed.");

			if(c + 1 < num_cols)
			{
				if(!parser.parseChar(','))
					throw glare::Exception("Parse of ',' failed.");
			}
		}
		parser.advancePastLine();
		row++;
	}
	if(row != num_rows)
		throw glare::Exception("Failed to find expected number of rows.");
}


void writeArrayLiteral(const Array2D<double>& data, const std::string& dest_literal_file, const std::string& array_literal_name_and_dec, const std::string& array_literal_comment)
{
	try
	{
		std::string res = array_literal_comment + "\n" + array_literal_name_and_dec + "[" + toString(data.getWidth() * data.getHeight()) + "] = {\n";
		for(int y=0; y<(int)data.getHeight(); ++y)
		{
			res += "\t";

			for(int x=0; x<data.getWidth(); ++x)
			{
				const std::string num_str = ::floatLiteralString((float)data.elem(x, y));

				if(x + 1 < (int)data.getWidth())
				{
					res += rightSpacePad(num_str + ",", 17);
				}
				else
				{
					if(y + 1 < (int)data.getHeight())
						res += num_str + ",\n";
					else
						res += num_str + "\n";
				}
			}
		}
		res += "};\n";

		FileUtils::writeEntireFileTextMode(dest_literal_file, res);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw glare::Exception(e.what());
	}
}


// Untested
void writeArrayLiteral(const std::vector<float>& data, size_t num_elems_per_line, const std::string& dest_literal_file, const std::string& array_literal_name_and_dec, const std::string& array_literal_comment)
{
	try
	{
		std::string res = array_literal_comment + "\n" + array_literal_name_and_dec + "[" + toString(data.size()) + "] = {\n";
		for(int y=0; y<(int)data.size() / num_elems_per_line; ++y)
		{
			res += "\t";

			for(int x=0; x<(int)num_elems_per_line; ++x)
			{
				const std::string num_str = ::floatLiteralString((float)data[y * num_elems_per_line + x]);

				if(x + 1 < (int)num_elems_per_line)
				{
					res += rightSpacePad(num_str + ",", 17);
				}
				else
				{
					if(y + 1 < (int)data.size() / num_elems_per_line)
						res += num_str + ",\n";
					else
						res += num_str + "\n";
				}
			}
		}
		res += "};\n";

		FileUtils::writeEntireFileTextMode(dest_literal_file, res);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw glare::Exception(e.what());
	}
}


void convertCSVToArrayLiteral(const std::string& CSV_path, const std::string& dest_literal_file, const std::string& array_literal_name_and_dec, const std::string& array_literal_comment)
{
	CSVData CSV_data;
	loadCSVFile(CSV_path, CSV_data);

	writeArrayLiteral(CSV_data.data, dest_literal_file, array_literal_name_and_dec, array_literal_comment);
}


} // end namespace ArrayLiteralUtils


#if BUILD_TESTS


#include "../utils/TestUtils.h"


void ArrayLiteralUtils::test()
{
	try
	{
		CSVData data;
		loadCSVFile(TestUtils::getIndigoTestReposDir() + "/data/CIE/cie2006-xyzbar-390+1+830.csv.txt", data);
		testAssert(data.data.getWidth() == 3 && data.data.getHeight() == 441);

		testAssert(data.data.elem(0, 0) == 0.00295242);
		testAssert(data.data.elem(1, 0) == 0.0004076779);
		testAssert(data.data.elem(2, 0) == 0.01318752);

		testAssert(data.data.elem(0, 440) == 1.579199E-06);
		testAssert(data.data.elem(1, 440) == 6.34538E-07);
		testAssert(data.data.elem(2, 440) == 0);


		//convertCSVToArrayLiteral(TestUtils::getIndigoTestReposDir() + "/data/CIE/cie2006-xyzbar-390+1+830.csv.txt", "d:/files/test.cpp",
		//	"static const float test_array", "// The test array\n// Comment line two");
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


#endif // BUILD_TESTS