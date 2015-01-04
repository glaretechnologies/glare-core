/*=====================================================================
FileUtils.h
-----------
File created by ClassTemplate on Sat Mar 01 18:05:09 2003
Copyright Glare Technologies Limited 2014 - 
=====================================================================*/
#pragma once


#include "Platform.h"
#include <string>
#include <vector>


namespace FileUtils
{


class FileUtilsExcep
{
public:
	FileUtilsExcep(const std::string& s_) : s(s_) {}
	~FileUtilsExcep(){}

	const std::string& what() const { return s; }
private:
	std::string s;
};


const std::string join(const std::string& dirpath, const std::string& filename);
const std::string toPlatformSlashes(const std::string& pathname);


void createDir(const std::string& dirname);
void createDirsForPath(const std::string& path);

void createDirIfDoesNotExist(const std::string& dirname);


// Gets the directory of a file from the pathname.
// If the pathname is just the filename, returns ""
// Path will *not* have a trailing slash
const std::string getDirectory(const std::string& pathname);

const std::string getFilename(const std::string& pathname); // throws FileUtilsExcep

// Returns only the filenames, not the full paths
const std::vector<std::string> getFilesInDir(const std::string& dir_path); // throws FileUtilsExcep
// Returns the full filenames.  Ignores the pseudo-files '.' and '..'.
const std::vector<std::string> getFilesInDirFullPaths(const std::string& dir_path); // throws FileUtilsExcep

bool fileExists(const std::string& pathname); // Does not throw any exceptions.

// Returns the size of the file in bytes.  Throws FileUtilsExcep on failure, for example if the file does not exist.
uint64 getFileSize(const std::string& pathname);

void getDirectoriesFromPath(const std::string& pathname_, std::vector<std::string>& dirs_out);

// Returns true if pathname is relative, and does not contain any '..' dirs.
bool isDirectory(const std::string& pathname);
bool isPathSafe(const std::string& pathname);

void readEntireFile(const std::string& pathname, std::string& filecontents_out); // throws FileUtilsExcep
void readEntireFile(const std::string& pathname, std::vector<unsigned char>& filecontents_out); // throws FileUtilsExcep

void writeEntireFile(const std::string& pathname, const std::vector<unsigned char>& filecontents); // throws FileUtilsExcep
void writeEntireFile(const std::string& pathname, const std::string& filecontents); // throws FileUtilsExcep
void writeEntireFileTextMode(const std::string& pathname, const std::string& filecontents); // throws FileUtilsExcep
void writeEntireFile(const std::string& pathname, const char* data, size_t data_size); // throws FileUtilsExcep

void readEntireFileTextMode(const std::string& pathname, std::string& s_out); // throws FileUtilsExcep

void copyFile(const std::string& srcpath, const std::string& dstpath);

// Atomic filesystem operation (hopefully)
// throws FileUtilsExcep on failure
void moveFile(const std::string& srcpath, const std::string& dstpath);

void deleteFile(const std::string& path);

void deleteEmptyDirectory(const std::string& path);
void deleteDirectoryRecursive(const std::string& path);


uint64 getFileCreatedTime(const std::string& filename);

bool isPathAbsolute(const std::string& p);

// NOTE: this function call is rather vulnerable to handle leaks.  Prefer to use the FileHandle class instead.
FILE* openFile(const std::string& pathname, const std::string& openmode);

// Remove non alphanumeric characters etc..
const std::string makeOSFriendlyFilename(const std::string& name);

// Changes slashes to platform slashes.  Also tries to guess the correct case by scanning directory and doing case-insensitive matches.
// On Unix (Linux / OS X) will also return the canonical path name.
const std::string getActualOSPath(const std::string& path); // throws FileUtilsExcep

// This is used in TextureServer and ThumbnailCache. We want the key into the textures map to have a 1-1 correspondence with the actual file.
// This is to avoid multiple copies of the same texture being loaded if the queried pathname differs in just e.g. case.
// The best way to do this would be to get the canonical path name.  This doesn't seem possible in Windows XP currently.
// GetFinalPathNameByHandle may work for Vista+ (see http://msdn.microsoft.com/en-nz/library/windows/desktop/aa364962(v=vs.85).aspx)
// Since windows file system is not case sensitive, we will downcase, to do a simple 'canonicalisation'.
const std::string getPathKey(const std::string& pathname); // throws FileUtilsExcep

#if defined(_WIN32) && !defined(__MINGW32__)
const std::wstring convertUTF8ToFStreamPath(const std::string& p);
#else
const std::string convertUTF8ToFStreamPath(const std::string& p);
#endif


void doUnitTests();


} //end namespace FileUtils
