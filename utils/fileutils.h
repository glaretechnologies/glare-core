/*=====================================================================
fileutils.h
-----------
File created by ClassTemplate on Sat Mar 01 18:05:09 2003
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __FILEUTILS_H_666_
#define __FILEUTILS_H_666_

#include "platform.h"
#include <string>
#include <vector>
#include <fstream>
class Date;

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


//const std::string getFirstNDirs(const std::string& dirname, int n);

//void splitDirName(const std::string& dirname, std::string& rootdir_out, 
//						std::string& rest_out);

void createDir(const std::string& dirname);
void createDirsForPath(const std::string& path);

void createDirIfDoesNotExist(const std::string& dirname);


//Gets the directory of a file from the pathname.
//If the pathname is just the filename, returns ""
//Path will *not* have a trailing slash
const std::string getDirectory(const std::string& pathname);

const std::string getFilename(const std::string& pathname); // throws FileUtilsExcep


//creates the directory the file is in according to pathname
//returns false if fails or directory already exists
//bool createDirForPathname(const std::string& pathname);

/*
Returns only the filenames, not the full paths
*/
const std::vector<std::string> getFilesInDir(const std::string& dir_path);


//bool dirExists(const std::string& dirname);
bool fileExists(const std::string& pathname); //NOTE: untested

void getDirectoriesFromPath(const std::string& pathname_, std::vector<std::string>& dirs_out);

//bool isPathEqualOrLower(const std::string& pathname);

//Returns true if pathname is relative, and does not contain any '..' dirs.
bool isDirectory(const std::string& pathname);
bool isPathSafe(const std::string& pathname);

//void readEntireFile(std::ifstream& file, std::string& filecontents_out); // throws FileUtilsExcep
//void readEntireFile(std::ifstream& file, std::vector<unsigned char>& filecontents_out); // throws FileUtilsExcep
void readEntireFile(const std::string& pathname, std::string& filecontents_out); // throws FileUtilsExcep
void readEntireFile(const std::string& pathname, std::vector<unsigned char>& filecontents_out); // throws FileUtilsExcep

void writeEntireFile(const std::string& pathname, const std::vector<unsigned char>& filecontents); // throws FileUtilsExcep
void writeEntireFile(const std::string& pathname, const std::string& filecontents); // throws FileUtilsExcep

const std::string getCurrentDir();

void copyFile(const std::string& srcpath, const std::string& dstpath);

// Atomic filesystem operation (hopefully)
// throws FileUtilsExcep on failure
void moveFile(const std::string& srcpath, const std::string& dstpath);

void deleteFile(const std::string& path);

void deleteEmptyDirectory(const std::string& path);


//fle must be closed 'cause this function opens it
//const std::string getAscTimeFileLastModified(const std::string& filename);

//const Date getFileLastModifiedDate(const std::string& filename);

bool isPathAbsolute(const std::string& p);

//uint32 fileChecksum(const std::string& p); // throws FileUtilsExcep

FILE* openFile(const std::string& pathname, const std::string& openmode);

// remove non alphanumeric characters etc..
const std::string makeOSFriendlyFilename(const std::string& name);

/*
Changed slashes to platform slashes.  Also tries to guess the correct case by scanning directory and doing case-insensitive matches.
*/
const std::string getActualOSPath(const std::string& path);


#if (defined(WIN32) || defined(WIN64)) && !defined(__MINGW32__)
const std::wstring convertUTF8ToFStreamPath(const std::string& p);
#else
const std::string convertUTF8ToFStreamPath(const std::string& p);
#endif

void doUnitTests();

} //end namespace FileUtils


#endif //__FILEUTILS_H_666_




