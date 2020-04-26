/*=====================================================================
ArrayLiteralUtils.h
-------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "Array2D.h"
#include <string>
#include <vector>


namespace ArrayLiteralUtils
{


struct CSVData
{
	const std::vector<float> getColumnAsFloatVector(size_t col_i) const;

	Array2D<double> data;
};
void loadCSVFile(const std::string& CSV_path, CSVData& data_out);


void writeArrayLiteral(const Array2D<double>& data, const std::string& dest_literal_file, const std::string& array_literal_name_and_dec, const std::string& array_literal_comment);

// Untested
void writeArrayLiteral(const std::vector<float>& data, size_t num_elems_per_line, const std::string& dest_literal_file, const std::string& array_literal_name_and_dec, const std::string& array_literal_comment);

/*
For example: 
convertCSVToArrayLiteral(TestUtils::getIndigoTestReposDir() + "/data/CIE/cie2006-xyzbar-390+1+830.csv.txt", "d:/files/test.cpp",
			"static const float test_array", "// The test array\n// Comment line two");
*/
void convertCSVToArrayLiteral(const std::string& CSV_path, const std::string& dest_literal_file, const std::string& array_literal_name_and_dec, const std::string& array_literal_comment);

void test();


} // End namespace ArrayLiteralUtils
