/*=====================================================================
FileUtils.cpp
-------------
File created by ClassTemplate on Sat Mar 01 18:05:09 2003
Code By Nicholas Chapman.
=====================================================================*/
#include "FileUtils.h"


#include "../utils/IncludeWindows.h"
#if !defined(_WIN32)
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>	// For open / close
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#endif

#include "StringUtils.h"
#include "MemMappedFile.h"
#include "Exception.h"
#include "PlatformUtils.h"
#include "Timer.h"
#include <cstring>
#include <stdlib.h>
#include <assert.h>
#include <fstream>


namespace FileUtils
{


#if defined(_WIN32)
const char* PLATFORM_DIR_SEPARATOR = "\\";
const char PLATFORM_DIR_SEPARATOR_CHAR = '\\';
#else
const char* PLATFORM_DIR_SEPARATOR = "/";
const char PLATFORM_DIR_SEPARATOR_CHAR = '/';
#endif


const std::string join(const std::string& dirpath, const std::string& filename)
{
	if(dirpath == "")
		return filename;

	if(isPathAbsolute(filename))
		return filename;

	return dirpath + PLATFORM_DIR_SEPARATOR + filename;
}


const std::string toPlatformSlashes(const std::string& pathname)
{
	std::string result = pathname;
	//NOTE: inefficient.
	::replaceChar(result, '\\', PLATFORM_DIR_SEPARATOR_CHAR);
	::replaceChar(result, '/', PLATFORM_DIR_SEPARATOR_CHAR);
	return result;
}


void createDir(const std::string& dirname)
{
#if defined(_WIN32)

	const BOOL result = ::CreateDirectory(StringUtils::UTF8ToPlatformUnicodeEncoding(dirname).c_str(), NULL);
	if(!result)
		throw FileUtilsExcep("Failed to create directory '" + dirname + "': " + PlatformUtils::getLastErrorString());
#else
	// Create with r/w/x permissions for user and group
	if(mkdir(dirname.c_str(), S_IRWXU | S_IRWXG) != 0)
		throw FileUtilsExcep("Failed to create directory '" + dirname + "': " + PlatformUtils::getLastErrorString());
#endif
}


void createDirsForPath(const std::string& path)
{
	std::vector<std::string> dirs;
	getDirectoriesFromPath(path, dirs);

	std::string dir;
	for(unsigned int i=0; i<dirs.size(); ++i)
	{
		//std::cout << "dirs[" << i << "]: '" << dirs[i] << "'" << std::endl;
		//if(i > 0 || !isPathAbsolute(path)) // don't create first
		//{
		if(dirs[i] == "")
		{
			assert(i == 0);
			assert(std::string(PLATFORM_DIR_SEPARATOR) == "/");
			// This is the root dir on Unix.  Don't try and create it.
			dir = "/";
		}
		else // Else this is not the root dir.
		{
			dir += dirs[i];

			//std::cout << "dir: '" << dir << "'" << std::endl;

			if(!(fileExists(dir) || hasSuffix(dir, ":")))
				createDir(dir);

			dir += PLATFORM_DIR_SEPARATOR;
		}
	}
}


void createDirIfDoesNotExist(const std::string& dirname)
{
	if(!fileExists(dirname))
		createDir(dirname);
}


// Gets the directory of a file from the pathname.
// If the pathname is just the filename, returns ""
// Path will *not* have a trailing slash
const std::string getDirectory(const std::string& pathname)
{
	const int pathname_size = (int)pathname.size();
	if(pathname_size == 0)
		return "";

	// Walk backwards from end of string until we hit a slash.
	for(int i=pathname_size-1; i >= 0; --i)
		if(pathname[i] == '/' || pathname[i] == '\\')
			return pathname.substr(0, i);

	return "";

	//const std::string path = toPlatformSlashes(pathname_);
	//const std::string::size_type lastslashpos = path.find_last_of(PLATFORM_DIR_SEPARATOR_CHAR);
	//if(lastslashpos == std::string::npos)
	//	return "";
	//return path.substr(0, lastslashpos);
}


const std::string getFilename(const std::string& pathname)
{
	const int pathname_size = (int)pathname.size();
	if(pathname_size == 0)
		return "";

	// Walk backwards from end of string until we hit a slash.
	for(int i=pathname_size-1; i >= 0; --i)
		if(pathname[i] == '/' || pathname[i] == '\\')
			return pathname.substr(i + 1);

	// If there were no slashes, just return whole path.
	return pathname;
}


const std::vector<std::string> getFilesInDir(const std::string& dir_path)
{
#if defined(_WIN32)
	WIN32_FIND_DATA find_data;
	HANDLE search_handle = FindFirstFile(StringUtils::UTF8ToPlatformUnicodeEncoding(dir_path + "\\*").c_str(), &find_data);
	if(search_handle == INVALID_HANDLE_VALUE)
		throw FileUtilsExcep("Failed to open dir '" + dir_path + "': " + PlatformUtils::getLastErrorString());


	std::vector<std::string> paths;
	paths.push_back(StringUtils::PlatformToUTF8UnicodeEncoding(find_data.cFileName));

	while(FindNextFile(search_handle, &find_data) != 0)
	{
		paths.push_back(StringUtils::PlatformToUTF8UnicodeEncoding(find_data.cFileName));
	}

	FindClose(search_handle);

	return paths;
#else
	DIR* dir = opendir(dir_path.c_str());
	if(!dir)
		throw FileUtilsExcep("Failed to open dir '" + dir_path + "': " + PlatformUtils::getLastErrorString());

	std::vector<std::string> paths;

	struct dirent* f = NULL;
	while((f = readdir(dir)) != NULL)
	{
		paths.push_back(f->d_name);	
	}

	closedir(dir);

	return paths;
#endif
}


const std::vector<std::string> getFilesInDirFullPaths(const std::string& dir_path)
{
	const std::vector<std::string> paths = getFilesInDir(dir_path);

	std::vector<std::string> fullpaths;
	fullpaths.reserve(paths.size());
	
	for(size_t i=0; i<paths.size(); ++i)
		if(paths[i] != "." && paths[i] != "..")
			fullpaths.push_back(join(dir_path, paths[i]));
	
	return fullpaths;
}


const std::vector<std::string> getFilesInDirWithExtensionFullPaths(const std::string& dir_path, const std::string& extension)
{
	const std::vector<std::string> paths = getFilesInDir(dir_path);

	std::vector<std::string> fullpaths;
	fullpaths.reserve(paths.size());
	
	for(size_t i=0; i<paths.size(); ++i)
		if(hasExtension(paths[i], extension) && paths[i] != "." && paths[i] != "..")
			fullpaths.push_back(join(dir_path, paths[i]));
	
	return fullpaths;
}


static void doGetFilesInDirWithExtensionFullPathsRecursive(const std::string& dir_path, const std::string& extension, std::vector<std::string>& fullpaths_out)
{
	const std::vector<std::string> paths = getFilesInDir(dir_path);

	for(size_t i=0; i<paths.size(); ++i)
	{
		const std::string full_path = join(dir_path, paths[i]);

		if((paths[i] != ".") && (paths[i] != ".."))
		{
			if(isDirectory(full_path))
				doGetFilesInDirWithExtensionFullPathsRecursive(full_path, extension, fullpaths_out); // Recurse
			else if(hasExtension(paths[i], extension))
				fullpaths_out.push_back(full_path);
		}
	}
}


const std::vector<std::string> getFilesInDirWithExtensionFullPathsRecursive(const std::string& dir_path, const std::string& extension)
{
	std::vector<std::string> fullpaths;
	doGetFilesInDirWithExtensionFullPathsRecursive(dir_path, extension, fullpaths);
	return fullpaths;
}


bool fileExists(const std::string& pathname)
{
#if defined(_WIN32)
	// Apparently GetFileAttributes is faster than using FindFirstFile() etc..:
	// https://blogs.msdn.microsoft.com/oldnewthing/20071023-00/?p=24713/
	// NOTE: could also use PathFileExists().
	const DWORD res = GetFileAttributes(StringUtils::UTF8ToPlatformUnicodeEncoding(pathname).c_str());
	return res != INVALID_FILE_ATTRIBUTES;
#else
	struct stat buffer;
	const int status = stat(pathname.c_str(), &buffer);
	return status == 0;
#endif
}


uint64 getFileSize(const std::string& path)
{
#if defined(_WIN32)
	WIN32_FILE_ATTRIBUTE_DATA info;

	BOOL res = GetFileAttributesEx(
		StringUtils::UTF8ToPlatformUnicodeEncoding(path).c_str(),
		GetFileExInfoStandard,
		&info
	);
	if(!res)
		throw FileUtilsExcep("Failed to get size of file '" + path + "': " + PlatformUtils::getLastErrorString());

	// This seems to be the standard way of converting the high and low parts to a 64-bit value
	ULARGE_INTEGER ul;
	ul.HighPart = info.nFileSizeHigh;
	ul.LowPart = info.nFileSizeLow;
	return ul.QuadPart;

#else
	struct stat info;
	if(stat(StringUtils::UTF8ToPlatformUnicodeEncoding(path).c_str(), &info) != 0)
		throw FileUtilsExcep("Failed to get size of file: '" + path + "': " + PlatformUtils::getLastErrorString());
	return info.st_size;
#endif
}


void getDirectoriesFromPath(const std::string& pathname_, std::vector<std::string>& dirs_out)
{
	dirs_out.clear();

	//-----------------------------------------------------------------
	//replace all /'s with \'s.
	//-----------------------------------------------------------------
	std::string pathname = pathname_;
	replaceChar(pathname, '/', '\\');

	std::string::size_type startpos = 0;

	while(1)
	{
		const std::string::size_type slashpos = pathname.find_first_of('\\', startpos);

		if(slashpos == std::string::npos)
			break;

		assert(slashpos - startpos >= 0);
		assert(startpos >= 0);
		assert(slashpos < pathname.size());

		//if(slashpos - startpos > 0) // Don't include zero length dirs, e.g. the root dir on Unix.
		dirs_out.push_back(pathname.substr(startpos, slashpos - startpos));

		startpos = slashpos + 1;
	}
}


bool isDirectory(const std::string& pathname)
{
#if defined(_WIN32)
	WIN32_FILE_ATTRIBUTE_DATA file_data;
	const BOOL res = GetFileAttributesEx(StringUtils::UTF8ToPlatformUnicodeEncoding(pathname).c_str(), GetFileExInfoStandard, &file_data);
	if(!res)
		return false;

	return (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? true : false;
#else
	struct stat buffer;
	const int status = stat(pathname.c_str(), &buffer);
	if(status != 0)
		return false; // No such file

	return S_ISDIR(buffer.st_mode);
#endif
}


bool isPathSafe(const std::string& pathname)
{
	if(pathname.size() == 0)
		return false;

	if(isPathAbsolute(pathname))
		return false;

	std::vector<std::string> dirs;
	getDirectoriesFromPath(pathname, dirs);
	for(unsigned int i=0; i<dirs.size(); ++i)
		if(dirs[i] == "..")
			return false;

	return true;
}


void readEntireFile(const std::string& pathname,
					std::string& filecontents_out)
{
	try
	{
		MemMappedFile file(pathname);
		filecontents_out.resize(file.fileSize());
		if(file.fileSize() > 0)
			std::memcpy(&filecontents_out[0], file.fileData(), file.fileSize());
	}
	catch(Indigo::Exception& e)
	{
		throw FileUtilsExcep("Could not open '" + pathname + "' for reading: " + e.what());
	}
}


void readEntireFile(const std::string& pathname,
					std::vector<unsigned char>& filecontents_out)
{
	try
	{
		MemMappedFile file(pathname);
		filecontents_out.resize(file.fileSize());
		if(file.fileSize() > 0)
			std::memcpy(&filecontents_out[0], file.fileData(), file.fileSize());
	}
	catch(Indigo::Exception& e)
	{
		throw FileUtilsExcep("Could not open '" + pathname + "' for reading: " + e.what());
	}
}


// Read the contents of a file.  If the file was unable to be opened or read from, retry until it succeeds.
// Will keep trying until success or total_retry_period has elapsed, in which case an exception is thrown.
void readEntireFileWithRetries(const std::string& pathname,
					double total_retry_period,
					std::vector<unsigned char>& filecontents_out)
{
	Timer timer;
	std::string last_excep_msg;
	do
	{
		try
		{
			MemMappedFile file(pathname);
			filecontents_out.resize(file.fileSize());
			if(file.fileSize() > 0)
				std::memcpy(&filecontents_out[0], file.fileData(), file.fileSize());
			return;
		}
		catch(Indigo::Exception& e)
		{
			last_excep_msg = e.what();
			PlatformUtils::Sleep(50); // Sleep briefly then try again.
		}
	} while(timer.elapsed() < total_retry_period);

	throw FileUtilsExcep("Could not open '" + pathname + "' for reading (last error: " + last_excep_msg + ").");
}


void writeEntireFile(const std::string& pathname,
					 const std::vector<unsigned char>& filecontents)
{
	std::ofstream file(convertUTF8ToFStreamPath(pathname).c_str(), std::ios::binary);

	if(!file)
		throw FileUtilsExcep("Could not open '" + pathname + "' for writing.");

	if(filecontents.size() > 0)
		file.write((const char*)filecontents.data(), filecontents.size());

	if(file.bad())
		throw FileUtilsExcep("Write to '" + pathname + "' failed.");
}


void writeEntireFile(const std::string& pathname,
					 const std::string& filecontents)
{
	std::ofstream file(convertUTF8ToFStreamPath(pathname).c_str(), std::ios::binary);

	if(!file)
		throw FileUtilsExcep("Could not open '" + pathname + "' for writing.");

	if(filecontents.size() > 0)
		file.write(filecontents.data(), filecontents.size());

	if(file.bad())
		throw FileUtilsExcep("Write to '" + pathname + "' failed.");
}


void writeEntireFileTextMode(const std::string& pathname,
					 const std::string& filecontents)
{
	std::ofstream file(convertUTF8ToFStreamPath(pathname).c_str());

	if(!file)
		throw FileUtilsExcep("Could not open '" + pathname + "' for writing.");

	if(filecontents.size() > 0)
		file.write(filecontents.data(), filecontents.size());

	if(file.bad())
		throw FileUtilsExcep("Write to '" + pathname + "' failed.");
}


void writeEntireFile(const std::string& pathname, const char* data, size_t data_size)
{
	std::ofstream file(convertUTF8ToFStreamPath(pathname).c_str(), std::ios::binary);

	if(!file)
		throw FileUtilsExcep("Could not open '" + pathname + "' for writing.");

	if(data_size)
		file.write(data, data_size);

	if(file.bad())
		throw FileUtilsExcep("Write to '" + pathname + "' failed.");
}


void writeEntireFileAtomically(const std::string& pathname, const char* data, size_t data_size) // throws FileUtilsExcep
{
#ifdef WIN32
	//-------------------- Write data to a unique temporary file first -----------------------
	//
	const std::string dir = FileUtils::getDirectory(pathname);
	TCHAR temp_file_name[MAX_PATH];
	GetTempFileName(
		StringUtils::UTF8ToPlatformUnicodeEncoding(dir).c_str(), // dir path
		TEXT("glare"), // lpPrefixString
		0, // uUnique
		temp_file_name
	);

	const std::string temp_pathname = StringUtils::PlatformToUTF8UnicodeEncoding(temp_file_name);// pathname + "_tmp";

	writeEntireFile(temp_pathname, data, data_size);


	//-------------------- Replace target file contents with temporary file contents -----------------------

	// A single iteration of the loop suffers from a possible race condition, if another process creates a file or deletes a file between the fileExists() call
	// and the MoveFileEx() or ReplaceFile() call.
	// So loop a few times until we are successful, or give up.

	bool succeeded = false;
	for(int i=0; i<10 && !succeeded; ++i)
	{
		if(!fileExists(pathname)) // ReplaceFile doesn't work when the target file doesn't exist
		{
			// If file doesn't exist, we should be able to move to it (unless some other process just created it)
			if(MoveFileEx(
				StringUtils::UTF8ToPlatformUnicodeEncoding(temp_pathname).c_str(), // existing file name
				StringUtils::UTF8ToPlatformUnicodeEncoding(pathname).c_str(), // new file name
				0 // flags.  Don't MOVEFILE_REPLACE_EXISTING
			))
				succeeded = true;
		}
		else // Else if file exists:
		{
			if(ReplaceFile(
				StringUtils::UTF8ToPlatformUnicodeEncoding(pathname).c_str(), // lpReplacedFileName
				StringUtils::UTF8ToPlatformUnicodeEncoding(temp_pathname).c_str(), // lpReplacementFileName
				NULL, // lpBackupFileName 
				0, // dwReplaceFlags 
				0, // lpExclude (reserved)
				0 // dwReplaceFlags (reserved)
			))
				succeeded = true;
		}
	}

	if(!succeeded)
		throw FileUtilsExcep("Failed to replace file '" + pathname + "' with '" + temp_pathname + "': " + PlatformUtils::getLastErrorString());

#else
	//-------------------- Write data to a unique temporary file first -----------------------
	// Create temp file
	std::string temp_pathname = pathname + "_XXXXXX";
	const int file = mkstemp(&temp_pathname[0]);
	if(file == -1)
		throw FileUtilsExcep("Failed to create temp file: " + PlatformUtils::getLastErrorString());

	// Write the data to it
	size_t remaining_size = data_size;
	while(remaining_size > 0)
	{
		const ssize_t bytes_written = write(file, data, remaining_size);
		if(bytes_written == -1)
			throw FileUtilsExcep("Error while writing to temp file: " + PlatformUtils::getLastErrorString());
		data += bytes_written;
		remaining_size -= bytes_written;
	}

	close(file);

	//-------------------- Replace target file contents with temporary file contents -----------------------
	if(rename(temp_pathname.c_str(), pathname.c_str()) != 0)
		throw FileUtilsExcep("Failed to move file '" + temp_pathname + "' to '" + pathname + "': " + PlatformUtils::getLastErrorString());
#endif
}


std::string writeEntireFileToTempFile(const char* data, size_t data_size)
{
	std::string temp_dir;
	try
	{
		temp_dir = PlatformUtils::getTempDirPath();
	}
	catch(PlatformUtils::PlatformUtilsExcep& e)
	{
		throw FileUtilsExcep(e.what());
	}
	
#ifdef WIN32
	//-------------------- Write data to a unique temporary file first -----------------------
	//
	const std::string dir = FileUtils::getDirectory(temp_dir); // TODO: pick a better dir?
	TCHAR temp_file_name[MAX_PATH];
	GetTempFileName(
		StringUtils::UTF8ToPlatformUnicodeEncoding(dir).c_str(), // dir path
		TEXT("glare"), // lpPrefixString
		0, // uUnique
		temp_file_name
	);

	const std::string temp_pathname = StringUtils::PlatformToUTF8UnicodeEncoding(temp_file_name);// pathname + "_tmp";

	writeEntireFile(temp_pathname, data, data_size);

	return temp_pathname;
#else
	//-------------------- Write data to a unique temporary file first -----------------------
	// Create temp file
	std::string temp_pathname = temp_dir + "/glare_XXXXXX";
	const int file = mkstemp(&temp_pathname[0]);
	if(file == -1)
		throw FileUtilsExcep("Failed to create temp file: " + PlatformUtils::getLastErrorString());

	// Write the data to it
	size_t remaining_size = data_size;
	while(remaining_size > 0)
	{
		const ssize_t bytes_written = write(file, data, remaining_size);
		if(bytes_written == -1)
			throw FileUtilsExcep("Error while writing to temp file: " + PlatformUtils::getLastErrorString());
		data += bytes_written;
		remaining_size -= bytes_written;
	}

	close(file);

	return temp_pathname;
#endif
}


void readEntireFileTextMode(const std::string& pathname, std::string& s_out) // throws FileUtilsExcep
{
	s_out = readEntireFileTextMode(pathname);
}


// Reads the entire file, but does some conversion - converts CRLF to LF.
std::string readEntireFileTextMode(const std::string& pathname) // throws FileUtilsExcep
{
	try
	{
		MemMappedFile file(pathname);

//#if defined(_WIN32)
		// Do text-mode processing: convert CRLF to LF

		const char* const data = (const char*)file.fileData();
		const size_t data_size = file.fileSize();

		std::string res;
		res.reserve(data_size);
		
		for(size_t i=0; i<data_size; ++i)
			if((data[i] == '\r') && (i + 1 < data_size) && (data[i+1] == '\n'))
			{
				res.push_back('\n');
				i++; // Skip both CR and LF
			}
			else
				res.push_back(data[i]);

		return res;
/*#else
		std::string data;
		if(file.fileSize() > 0)
			std::memcpy(&data[0], file.fileData(), file.fileSize());
		return data;
#endif*/
	}
	catch(Indigo::Exception& e)
	{
		throw FileUtilsExcep("Could not open '" + pathname + "' for reading: " + e.what());
	}
}


void copyFile(const std::string& srcpath, const std::string& dstpath)
{
#if defined(_WIN32)

	if(!CopyFile(
		StringUtils::UTF8ToPlatformUnicodeEncoding(srcpath).c_str(),
		StringUtils::UTF8ToPlatformUnicodeEncoding(dstpath).c_str(),
		FALSE // fail if exists
		))
	{
		throw FileUtilsExcep("Failed to copy file '" + srcpath + "' to '" + dstpath + "': " + PlatformUtils::getLastErrorString());
	}

#else
	std::vector<unsigned char> data;
	readEntireFile(srcpath, data);
	writeEntireFile(dstpath, data);
#endif
}


void deleteFile(const std::string& path)
{
#if defined(_WIN32)
	if(!DeleteFile(
		StringUtils::UTF8ToPlatformUnicodeEncoding(path).c_str()
		))
	{
		throw FileUtilsExcep("Failed to delete file '" + path + "': " + PlatformUtils::getLastErrorString());
	}

#else
	if(remove(path.c_str()) != 0)
		throw FileUtilsExcep("Failed to delete file '" + path + "': " + PlatformUtils::getLastErrorString());
#endif
}


void deleteEmptyDirectory(const std::string& path)
{
#if defined(_WIN32)
	if(!RemoveDirectory(
		StringUtils::UTF8ToPlatformUnicodeEncoding(path).c_str()
		))
	{
		throw FileUtilsExcep("Failed to delete directory '" + path + "': " + PlatformUtils::getLastErrorString());
	}

#else
	if(rmdir(path.c_str()) != 0)
		throw FileUtilsExcep("Failed to delete directory '" + path + "': " + PlatformUtils::getLastErrorString());
#endif
}


// Throws FileUtilsExcep
// TODO: Probably shouldn't follow symlinks in this traversal.
void deleteDirectoryRecursive(const std::string& path)
{
	if(!isDirectory(path)) return;

	const std::vector<std::string> files = getFilesInDir(path);

	for(size_t i = 0; i < files.size(); ++i)
	{
		if(files[i] != "." && files[i] != "..")
		{
			std::string file_path = join(path, files[i]);

			if(isDirectory(file_path))
				deleteDirectoryRecursive(file_path);
			else 
				deleteFile(file_path);
		}
	}

	deleteEmptyDirectory(path);
}


void deleteFilesInDir(const std::string& path)
{
	if(!isDirectory(path)) return;

	const std::vector<std::string> files = getFilesInDir(path);

	for(size_t i = 0; i < files.size(); ++i)
	{
		if(files[i] != "." && files[i] != "..")
		{
			std::string file_path = join(path, files[i]);

			if(!isDirectory(file_path))
				deleteFile(file_path);
		}
	}
}


void moveFile(const std::string& srcpath, const std::string& dstpath)
{
#if defined(_WIN32)
	// NOTE: To make this atomic on Windows, we could use ReplaceFile, with some care.
	// See http://msdn.microsoft.com/en-us/library/windows/desktop/aa365512(v=vs.85).aspx
	if(!MoveFileEx(
		StringUtils::UTF8ToPlatformUnicodeEncoding(srcpath).c_str(),
		StringUtils::UTF8ToPlatformUnicodeEncoding(dstpath).c_str(),
		MOVEFILE_REPLACE_EXISTING
	))
		throw FileUtilsExcep("Failed to move file '" + srcpath + "' to '" + dstpath + "': " + PlatformUtils::getLastErrorString());
#else
	if(rename(srcpath.c_str(), dstpath.c_str()) != 0)
		throw FileUtilsExcep("Failed to move file '" + srcpath + "' to '" + dstpath + "': " + PlatformUtils::getLastErrorString());
#endif
}


bool isPathAbsolute(const std::string& p)
{
	// Take into account both Windows and Unix prefixes.
	// This fixes a bug with network rendering across both Windows and Unix machines: http://www.indigorenderer.com/forum/viewtopic.php?f=5&t=11482
	// Note that this is maybe not the ideal way to solve the problem.

	return 
		(p.length() >= 2 && p[1] == ':')  || // e.g. C:/programming/
		(p.length() >= 1 && p[0] == '\\') || // Windows network share, e.g. \\Avarice
		(p.length() >= 1 && p[0] == '/'); // Unix root
}


FILE* openFile(const std::string& pathname, const std::string& openmode)
{
#if defined(_WIN32)
	// If we are on Windows, then, in order to use Unicode filenames, we will convert from UTF-8 to wstring and use _wfopen()
	return _wfopen(StringUtils::UTF8ToWString(pathname).c_str(), StringUtils::UTF8ToWString(openmode).c_str());
#else
	// On Linux (and on OS X?), fopen accepts UTF-8 encoded Unicode filenames natively.
	return fopen(pathname.c_str(), openmode.c_str());
#endif
}


// remove non alphanumeric characters etc..
const std::string makeOSFriendlyFilename(const std::string& name)
{
	//std::string r(name.size(), ' ');
	std::string r;
	for(unsigned int i=0; i<name.size(); ++i)
	{
		if(::isAlphaNumeric(name[i]) || name[i] == ' ' || name[i] == '_' || name[i] == '.' || name[i] == '(' || name[i] == ')' || name[i] == '-')
			r.push_back(name[i]);
		else
			r += "_" + int32ToString(r[i]); //r[i] = '_' + ;
	}

	return r;
}


#if defined(_WIN32)

static const std::string getCanonicalPath(const std::string& p)
{
	// NOTE: When we can assume Vista support, use GetFinalPathNameByHandle().
	return p;
}

#else

static const std::string getCanonicalPath(const std::string& p)
{
	char buf[PATH_MAX];
	char* result = realpath(p.c_str(), buf);
	if(result == NULL)
		throw FileUtilsExcep("realpath failed: " + PlatformUtils::getLastErrorString());
	return std::string(buf);
}

#endif


/*
Changes slashes to platform slashes.  Also tries to guess the correct case by scanning directory and doing case-insensitive matches.
On Unix (Linux / OS X) will also return the canonical path name.
*/
const std::string getActualOSPath(const std::string& path_)
{
	const std::string path = toPlatformSlashes(path_);

	if(fileExists(path))
		return getCanonicalPath(path);
	
	// We don't have an exact match.
	// Try to guess the correct case by scanning directory and doing case-insensitive matches.

	const std::string dir = getDirectory(path);
	const std::vector<std::string> files = getFilesInDir(dir);

	const std::string target_filename_lowercase = ::toLowerCase(getFilename(path));

	for(size_t i=0; i<files.size(); ++i)
	{
		if(::toLowerCase(files[i]) == target_filename_lowercase)
			return getCanonicalPath(join(dir, files[i]));
	}

	throw FileUtilsExcep("Could not find file '" + path_ + "'");
}


const std::string getPathKey(const std::string& pathname)
{
	const std::string use_path = FileUtils::getActualOSPath(pathname);

#if defined(_WIN32)
	return ::toLowerCase(use_path);
#else
	return use_path;
#endif
}


uint64 getFileCreatedTime(const std::string& filename)
{
#if defined(_WIN32)
	HANDLE file = CreateFile(
		StringUtils::UTF8ToPlatformUnicodeEncoding(filename).c_str(), 
		GENERIC_READ, 
		FILE_SHARE_READ,
		NULL, // lpSecurityAttributes 
		OPEN_EXISTING, // OPEN_EXISTING = Opens a file or device, only if it exists.
		FILE_ATTRIBUTE_NORMAL, // dwFlagsAndAttributes
		NULL // hTemplateFile
	);

	if(file == INVALID_HANDLE_VALUE)
		throw FileUtilsExcep("Failed to open file '" + filename + "': " + PlatformUtils::getLastErrorString());

	FILETIME creation_time;
	BOOL res = GetFileTime(
		file,
		&creation_time,
		nullptr,
		nullptr
	);

	if(res == FALSE)
		throw FileUtilsExcep("Failed to get created time for file '" + filename + "': " + PlatformUtils::getLastErrorString());

	const uint64 time_uint64 = ((uint64)creation_time.dwHighDateTime << 32) + creation_time.dwLowDateTime;
	return time_uint64;
#else
	throw FileUtilsExcep("getFileCreatedTime() not implemented on non-Windows.");
#endif
}



#if defined(_WIN32) && !defined(__MINGW32__)

const std::wstring convertUTF8ToFStreamPath(const std::string& p)
{
	return StringUtils::UTF8ToPlatformUnicodeEncoding(p);
}

#elif defined(__MINGW32__)

const std::string convertUTF8ToFStreamPath(const std::string& p)
{
	return p;
}

#else

const std::string convertUTF8ToFStreamPath(const std::string& p)
{
	return StringUtils::UTF8ToPlatformUnicodeEncoding(p);
}

#endif


} // End namespace FileUtils


#if BUILD_TESTS


#include "PlatformUtils.h"
#include "../indigo/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/MyThread.h"


namespace FileUtils
{


class FileUtilsTestThread : public MyThread
{
public:
	virtual void run()
	{
		const std::string testdir = PlatformUtils::getTempDirPath() + "/fileutils_tests";
		for(int it=0; it<1000; ++it)
		{
			std::string data(1 << 16, fill_char);
			try
			{
				writeEntireFileAtomically(testdir + "/a", (const char*)data.data(), data.size());

				// Open and read the file, and make sure the data is consistent (all bytes the same), and that the file has the correct size.
				MemMappedFile file(testdir + "/a");
				testAssert(file.fileSize() == 1 << 16);
				testAssert(((const char*)file.fileData())[0] == '\0' || ((const char*)file.fileData())[0] == '\1');
				for(size_t i=1; i<file.fileSize(); ++i)
					testAssert(((const char*)file.fileData())[i] == ((const char*)file.fileData())[0]);
			}
			catch(FileUtils::FileUtilsExcep&)
			{}
			catch(Indigo::Exception&)
			{}
		}
		conPrint("FileUtilsTestThread done.");
	}
	char fill_char;
};


// This thread just reads.
class FileUtilsReadTestThread : public MyThread
{
public:
	virtual void run()
	{
		const std::string testdir = PlatformUtils::getTempDirPath() + "/fileutils_tests";
		for(int it=0; it<1000; ++it)
		{
			try
			{
				MemMappedFile file(testdir + "/a");
				testAssert(file.fileSize() == 1 << 16);
				testAssert(((const char*)file.fileData())[0] == '\0' || ((const char*)file.fileData())[0] == '\1');
				for(size_t i=1; i<file.fileSize(); ++i)
					testAssert(((const char*)file.fileData())[i] == ((const char*)file.fileData())[0]);
			}
			catch(Indigo::Exception&)
			{}
		}
		conPrint("FileUtilsReadTestThread done.");
	}
};



void doUnitTests()
{
	conPrint("FileUtils::doUnitTests()");

	//========================= Test writeEntireFileAtomically() ================================

	// Stress test with multiple threads reading and writing to the same file.
	//if(true) // Takes a bit long to run in normal test suite execution.
	{
		conPrint("Doing writeEntireFileAtomically stress test...");

		const std::string testdir = PlatformUtils::getTempDirPath() + "/fileutils_tests";
		createDirIfDoesNotExist(testdir);
		deleteFilesInDir(testdir);

		Reference<FileUtilsTestThread> thread_a = new FileUtilsTestThread();
		thread_a->fill_char = '\0';
		thread_a->launch();
		Reference<FileUtilsTestThread> thread_b = new FileUtilsTestThread();
		thread_b->fill_char = '\1';
		thread_b->launch();
		Reference<FileUtilsReadTestThread> read_thread = new FileUtilsReadTestThread();
		read_thread->launch();
		
		thread_a->join();
		thread_b->join();
		read_thread->join();
	}

	try
	{
		const std::string testdir = PlatformUtils::getTempDirPath() + "/fileutils_tests";
		//const std::string testdir = "D:/tempfiles/fileutils_tests";
		createDirIfDoesNotExist(testdir);
		deleteFilesInDir(testdir);

		testAssert(getFilesInDirFullPaths(testdir).empty());

		std::string data = "hello";
		writeEntireFileAtomically(testdir + "/a", data.data(), data.size());

		testAssert(fileExists(testdir + "/a") && readEntireFileTextMode(testdir + "/a") == "hello");
		testAssert(!fileExists(testdir + "/a_tmp"));

		// Now overwrite with new data
		data = "world";
		writeEntireFileAtomically(testdir + "/a", data.data(), data.size());

		testAssert(fileExists(testdir + "/a") && readEntireFileTextMode(testdir + "/a") == "world");
		testAssert(!fileExists(testdir + "/a_tmp"));

		//const std::vector<std::string> files = getFilesInDir(testdir);
		//testAssert(files.size() == 1 && files[0] == "a");
	}
	catch(FileUtilsExcep& e)
	{
		failTest(e.what());
	}


	//========================= Test getDirectory() ================================
	// Gets the directory of a file from the pathname.
	// If the pathname is just the filename, returns ""
	// Path will *not* have a trailing slash
	try
	{
		testAssert(getDirectory("testfiles/teapot.obj") == "testfiles");
		testAssert(getDirectory("testfiles\\teapot.obj") == "testfiles");

		testAssert(getDirectory("a/b/c") == "a/b");
		testAssert(getDirectory("a\\b\\c") == "a\\b");

		testAssert(getDirectory("a\\b/c") == "a\\b");
		testAssert(getDirectory("a/b\\c") == "a/b");

		testAssert(getDirectory("teapot.obj") == "");
		testAssert(getDirectory("") == "");
		testAssert(getDirectory("testfiles/") == "testfiles");
		testAssert(getDirectory("testfiles\\") == "testfiles");
		testAssert(getDirectory("/") == "");
		testAssert(getDirectory("\\") == "");
	}
	catch(FileUtilsExcep& e)
	{
		failTest(e.what());
	}


	//========================= Test getFilename() ================================
	try
	{
		testAssert(getFilename("testfiles/teapot.obj") == "teapot.obj");
		testAssert(getFilename("testfiles\\teapot.obj") == "teapot.obj");

		testAssert(getFilename("teapot.obj") == "teapot.obj");
		testAssert(getFilename("") == "");
		testAssert(getFilename("testfiles/") == "");
		testAssert(getFilename("testfiles\\") == "");
		testAssert(getFilename("/") == "");
		testAssert(getFilename("\\") == "");
	}
	catch(FileUtilsExcep& e)
	{
		failTest(e.what());
	}


	//========================= Test fileExists() ================================
	try
	{
		testAssert(fileExists(TestUtils::getIndigoTestReposDir() + "/testfiles/teapot.obj"));
		testAssert(!fileExists(TestUtils::getIndigoTestReposDir() + "/testfiles/teapot_DOES_NOT_EXIST.obj"));

		// fileExists should return true for directories as well.
		testAssert(fileExists(TestUtils::getIndigoTestReposDir() + "/testfiles"));
		testAssert(fileExists(TestUtils::getIndigoTestReposDir() + "/testfiles/"));
		testAssert(!fileExists(TestUtils::getIndigoTestReposDir() + "/testfiles_DOES_NOT_EXIST"));
	}
	catch(FileUtilsExcep& e)
	{
		failTest(e.what());
	}

	
	//========================= Test isDirectory() ================================
	try
	{
		// Should return false for files that exist and that do not exist
		testAssert(!isDirectory(TestUtils::getIndigoTestReposDir() + "/testfiles/teapot.obj"));
		testAssert(!isDirectory(TestUtils::getIndigoTestReposDir() + "/testfiles/teapot_DOES_NOT_EXIST.obj"));

		testAssert(isDirectory(TestUtils::getIndigoTestReposDir() + "/testfiles"));
		testAssert(isDirectory(TestUtils::getIndigoTestReposDir() + "/testfiles/"));
		testAssert(!isDirectory(TestUtils::getIndigoTestReposDir() + "/testfiles_DOES_NOT_EXIST"));
	}
	catch(FileUtilsExcep& e)
	{
		failTest(e.what());
	}


	//========================= Test getActualOSPath() ================================
	try
	{
		//const std::vector<std::string> files = getFilesInDir(TestUtils::getIndigoTestReposDir() + "/testfiles");
		//for(size_t i=0; i<files.size(); ++i)
		///	conPrint("file: " + files[i]);

#if defined(_WIN32)
		testAssert(getActualOSPath(TestUtils::getIndigoTestReposDir() + "/testfiles/sphere.obj") == TestUtils::getIndigoTestReposDir() + "\\testfiles\\sphere.obj");
		testAssert(getActualOSPath(TestUtils::getIndigoTestReposDir() + "/testfiles/SpHerE.ObJ") == TestUtils::getIndigoTestReposDir() + "\\testfiles\\SpHerE.ObJ");
#else
		testAssert(getActualOSPath(TestUtils::getIndigoTestReposDir() + "/testfiles/sphere.obj") == TestUtils::getIndigoTestReposDir() + "/testfiles/sphere.obj");
		testAssert(getActualOSPath(TestUtils::getIndigoTestReposDir() + "/testfiles/SpHerE.ObJ") == TestUtils::getIndigoTestReposDir() + "/testfiles/sphere.obj");
		testAssert(getActualOSPath(TestUtils::getIndigoTestReposDir() + "\\testfiles/SpHerE.ObJ") == TestUtils::getIndigoTestReposDir() + "/testfiles/sphere.obj");
		testAssert(getActualOSPath(TestUtils::getIndigoTestReposDir() + "/testfiles\\SpHerE.ObJ") == TestUtils::getIndigoTestReposDir() + "/testfiles/sphere.obj");
#endif
	}
	catch(FileUtilsExcep& e)
	{
		failTest(e.what());
	}


	//========================= Test handling of Unicode pathnames ============================
	
	const std::string euro_txt_pathname = TestUtils::getIndigoTestReposDir() + "/testfiles/\xE2\x82\xAC.txt";

	try
	{
		testAssert(fileExists(euro_txt_pathname)); // Euro sign.txt

		std::string contents;
		FileUtils::readEntireFile(euro_txt_pathname, contents);

		const std::string target_contents = "\xE2\x82\xAC\n";

		testAssert(contents == target_contents);
	}
	catch(FileUtilsExcep& e)
	{
		conPrint(e.what());
		testAssert(0);
	}

	// Test openFile() with a Unicode pathname
	FILE* infile = FileUtils::openFile(euro_txt_pathname, "rb");
	testAssert(infile != NULL);
	if(infile) fclose(infile);


	// Test std::ifstream with a Unicode pathname
	{
	std::ifstream file(FileUtils::convertUTF8ToFStreamPath(euro_txt_pathname).c_str(), std::ios_base::in | std::ios_base::binary);

	testAssert(file.is_open());
	}

	// Test std::ifstream without a Unicode pathname
	{
	std::ifstream file(FileUtils::convertUTF8ToFStreamPath(TestUtils::getIndigoTestReposDir() + "/testfiles/a_test_mesh.obj").c_str(), std::ios_base::in | std::ios_base::binary);

	testAssert(file.is_open());
	}


	//============================ Test getFileSize() ============================
	try
	{
		testAssert(getFileSize(TestUtils::getIndigoTestReposDir() + "/testfiles/a_test_mesh.obj") == 231);
	}
	catch(FileUtilsExcep& e)
	{
		failTest(e.what());
	}

	try
	{
		getFileSize(TestUtils::getIndigoTestReposDir() + "/testfiles/a_test_mesh_that_doesnt_exist.obj");

		failTest("No exception thrown by getFileSize() for nonexistent file.");
	}
	catch(FileUtilsExcep&)
	{
		//Test successful.
	}
	catch(...)
	{
		failTest("Wrong type of exception thrown by getFileSize()");
	}



	//============================ Test isPathAbsolute() ============================

//#if defined(_WIN32)
	testAssert(isPathAbsolute("C:/windows"));
	testAssert(isPathAbsolute("a:/dsfsdf/sdfsdf/sdf/"));
	testAssert(isPathAbsolute("a:\\dsfsdf"));
	testAssert(isPathAbsolute("\\\\lust\\dsfsdf"));

	testAssert(!isPathAbsolute("a/b/"));
	testAssert(!isPathAbsolute("somefile"));
	testAssert(!isPathAbsolute("."));
	testAssert(!isPathAbsolute(".."));
	testAssert(!isPathAbsolute(""));
	testAssert(!isPathSafe("c:\\windows\\haxored.dll"));
	testAssert(!isPathSafe("c:\\haxored.dll"));
	testAssert(!isPathSafe("c:\\"));
	testAssert(!isPathSafe("c:\\haxored.txt"));	//no absolute path names allowed
	testAssert(!isPathSafe("..\\..\\haxored.dll"));
	testAssert(!isPathSafe("..\\..\\haxored"));
	testAssert(!isPathSafe("a\\..\\..\\haxored"));
	testAssert(!isPathSafe("a/../../b/../haxored"));
	//testAssert(!isPathSafe("/b/file.txt"));	//can't start with /
	testAssert(!isPathSafe("\\b/file.txt"));	//can't start with backslash
	testAssert(!isPathSafe("\\localhost\\c\\something.txt"));	//can't start with host thingey
	testAssert(!isPathSafe("\\123.123.123.123\\c\\something.txt"));	//can't start with host thingey

	testAssert(isPathSafe("something.txt"));
	testAssert(isPathSafe("dir\\something.txt"));
//	testAssert(!isPathSafe("dir/something.txt"));//don't use forward slashes!!!!
	testAssert(isPathSafe("a\\b\\something.txt"));
	testAssert(isPathSafe("a\\b\\something"));
	testAssert(isPathSafe("a\\.\\something"));
//#else
	testAssert(isPathAbsolute("/etc/"));
	testAssert(!isPathAbsolute("dfgfdgdf/etc/"));

//#endif


	try
	{
		testAssert(eatExtension("hello.there") == "hello.");


		const std::string TEST_PATH = "TESTING_TEMP_FILE";
		const std::string TEST_PATH2 = "TESTING_TEMP_FILE_2";
		writeEntireFile(TEST_PATH, std::vector<unsigned char>(0, 100));
		testAssert(fileExists(TEST_PATH));
		moveFile(TEST_PATH, TEST_PATH2);
		testAssert(!fileExists(TEST_PATH));
		testAssert(fileExists(TEST_PATH2));
		deleteFile(TEST_PATH2);
		testAssert(!fileExists(TEST_PATH2));

		// Test dir stuff

		const std::string TEST_PATH3 = "TEMP_TESTING_DIR/TESTING_TEMP_FILE";
		testAssert(getDirectory(TEST_PATH3) == "TEMP_TESTING_DIR");
		testAssert(getFilename(TEST_PATH3) == "TESTING_TEMP_FILE");
		testAssert(isPathSafe(TEST_PATH3));

		createDirsForPath(TEST_PATH3);
		testAssert(fileExists("TEMP_TESTING_DIR"));
		deleteEmptyDirectory("TEMP_TESTING_DIR");
		testAssert(!fileExists("TEMP_TESTING_DIR"));

		const std::string TEST_PATH_4 = "TEMP_TESTING_DIR/a/b";
		createDirsForPath(TEST_PATH_4);
		testAssert(fileExists("TEMP_TESTING_DIR"));
		testAssert(fileExists("TEMP_TESTING_DIR/a"));
		deleteEmptyDirectory("TEMP_TESTING_DIR/a");
		deleteEmptyDirectory("TEMP_TESTING_DIR");

		// Windows sometimes takes a while to delete the dir, so wait a while if it's not done yet.
		for(int i=0; i<10; ++i)
		{
			if(fileExists("TEMP_TESTING_DIR"))
				::PlatformUtils::Sleep(20);
			else
				break;
		}
		testAssert(!fileExists("TEMP_TESTING_DIR"));
		testAssert(!fileExists("TEMP_TESTING_DIR/a"));

		// Test getFilesInDir
		createDir("TEMP_TESTING_DIR");
		writeEntireFile("TEMP_TESTING_DIR/a", std::vector<unsigned char>(0, 100));
		writeEntireFile("TEMP_TESTING_DIR/b", std::vector<unsigned char>(0, 100));
		
		const std::vector<std::string> files = getFilesInDir("TEMP_TESTING_DIR");
		testAssert(files.size() == 4);
		bool found_a = false;
		bool found_b = false;
		for(size_t i=0; i<files.size(); ++i)
		{
			if(files[i] == "a")
				found_a = true;
			if(files[i] == "b")
				found_b = true;
			//conPrint("file[" + ::toString((int) i) + "]: '" + files[i] + "'");
		}
		testAssert(found_a);
		testAssert(found_b);

		deleteFile("TEMP_TESTING_DIR/a");
		deleteFile("TEMP_TESTING_DIR/b");
		deleteEmptyDirectory("TEMP_TESTING_DIR");

		// Test deleteDirectoryRecursive
		createDir("TEMP_TESTING_DIR");
		writeEntireFile("TEMP_TESTING_DIR/a", std::vector<unsigned char>(0, 100));
		writeEntireFile("TEMP_TESTING_DIR/b", std::vector<unsigned char>(0, 100));
		createDir("TEMP_TESTING_DIR/subdir");
		writeEntireFile("TEMP_TESTING_DIR/subdir/b", std::vector<unsigned char>(0, 100));

		deleteDirectoryRecursive("TEMP_TESTING_DIR");

		// Test deletion of empty dir with deleteDirectoryRecursive
		createDir("TEMP_TESTING_DIR");

		deleteDirectoryRecursive("TEMP_TESTING_DIR");



		//============ Test getFileCreatedTime() ====================
#if defined(_WIN32)
		testAssert(getFileCreatedTime(TestUtils::getIndigoTestReposDir() + "/testfiles/a_test_mesh.obj") > 0);
#endif

		//============ Test readEntireFile() ====================
		{
			// CRLF should not be converted to LF (as this is a binary read)
			std::string s;
			readEntireFile(TestUtils::getIndigoTestReposDir() + "/testfiles/textfile_windows_line_endings.txt", s);
			testAssert(s == "a\r\nb");
		}
		{
			std::string s;
			readEntireFile(TestUtils::getIndigoTestReposDir() + "/testfiles/textfile_unix_line_endings.txt", s);
			testAssert(s == "a\nb");
		}
		{
			// Test with empty file
			std::string s;
			readEntireFile(TestUtils::getIndigoTestReposDir() + "/testfiles/empty_file", s);
			testAssert(s == "");
		}

		//============ Test readEntireFileWithRetries() ====================
		{
			// CRLF should not be converted to LF (as this is a binary read)
			std::vector<unsigned char> s;
			readEntireFileWithRetries(TestUtils::getIndigoTestReposDir() + "/testfiles/textfile_windows_line_endings.txt", /*max retry period=*/1.0, s);
			testAssert(std::string((const char*)&*s.begin(), (const char*)&*s.begin() + s.size()) == "a\r\nb");
		}
		{
			std::vector<unsigned char> s;
			readEntireFileWithRetries(TestUtils::getIndigoTestReposDir() + "/testfiles/textfile_unix_line_endings.txt", /*max retry period=*/1.0, s);
			testAssert(std::string((const char*)&*s.begin(), (const char*)&*s.begin() + s.size()) == "a\nb");
		}
		{
			// Test with empty file
			std::vector<unsigned char> s;
			readEntireFileWithRetries(TestUtils::getIndigoTestReposDir() + "/testfiles/empty_file", /*max retry period=*/1.0, s);
			testAssert(std::string((const char*)&*s.begin(), (const char*)&*s.begin() + s.size()) == "");
		}

		//============ Test readEntireFileTextMode() ====================
		{
			// CRLF should be converted to LF
			const std::string s = readEntireFileTextMode(TestUtils::getIndigoTestReposDir() + "/testfiles/textfile_windows_line_endings.txt");
			testAssert(s == "a\nb");
		}
		{
			const std::string s = readEntireFileTextMode(TestUtils::getIndigoTestReposDir() + "/testfiles/textfile_unix_line_endings.txt");
			testAssert(s == "a\nb");
		}
		{
			// Test with empty file
			const std::string s = readEntireFileTextMode(TestUtils::getIndigoTestReposDir() + "/testfiles/empty_file");
			testAssert(s == "");
		}
	}
	catch(FileUtilsExcep& e)
	{
		conPrint(e.what());
		testAssert(!"FileUtilsExcep");
	}

	//createDirsForPath("c:/temp/test/a");

	conPrint("FileUtils::doUnitTests() Done.");
}


} // end namespace FileUtils


#endif // BUILD_TESTS
