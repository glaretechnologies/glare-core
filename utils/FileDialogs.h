/*=====================================================================
FileDialogs.h
-------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "Exception.h"
#include <vector>
#include <string>


/*=====================================================================
FileDialogs
-----------
Shows native file pickers
=====================================================================*/
namespace FileDialogs
{
	struct FileTypeInfo
	{
		FileTypeInfo() {}
		FileTypeInfo(const std::string name_, const std::string& filter_, const std::string& extension_) : name(name_), filter(filter_), extension(extension_) {}
		std::string name;   // e.g. "Jpeg files"
		std::string filter; // e.g. "*.jpg;*.jpeg"
		std::string extension; // Extension to add if file type is selected, e.g. "jpg" 
	};
	struct Options
	{
		std::string dialog_title; // Used if non-empty
		std::vector<FileTypeInfo> file_types;
	};


	std::string showOpenFileDialog(const Options& options);
	
	std::string showSaveFileDialog(const Options& options);


} // end namespace FileDialogs
