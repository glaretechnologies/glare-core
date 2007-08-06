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


const std::string getFirstNDirs(const std::string& dirname, int n);

void splitDirName(const std::string& dirname, std::string& rootdir_out, 
						std::string& rest_out);

bool createDir(const std::string& dirname);


//Gets the directory of a file from the pathname.
//If the pathname is just the filename, returns ""
//Path will *not* have a trailing slash
const std::string getDirectory(const std::string& pathname);

const std::string getFilename(const std::string& pathname);


//creates the directory the file is in according to pathname
//returns false if fails or directory already exists
bool createDirForPathname(const std::string& pathname);


bool dirExists(const std::string& dirname);
bool fileExists(const std::string& pathname);//NOTE: untested

void getDirs(const std::string& pathname_, std::vector<std::string>& dirs_out);

bool isPathEqualOrLower(const std::string& pathname);

//Returns true if pathname is relative, contains only backslashes as dir separators,
//and specifies a file in a dir not-higher than the current dir.
bool isPathSafe(const std::string& pathname);

void readEntireFile(std::ifstream& file, std::string& filecontents_out);
void readEntireFile(std::ifstream& file, std::vector<unsigned char>& filecontents_out);

//throws FileUtilsExcep
void readEntireFile(const std::string& pathname, std::vector<unsigned char>& filecontents_out);
//throws FileUtilsExcep
void writeEntireFile(const std::string& pathname, const std::vector<unsigned char>& filecontents);

const std::string getCurrentDir();

void copyFile(const std::string& srcpath, const std::string& dstpath);

//atomic filesystem operation (hopefully)
//throws FileUtilsExcep on failure
void moveFile(const std::string& srcpath, const std::string& dstpath);

//fle must be closed 'cause this function opens it
//const std::string getAscTimeFileLastModified(const std::string& filename);

//const Date getFileLastModifiedDate(const std::string& filename);

void doUnitTests();

bool isPathAbsolute(const std::string& p);

} //end namespace FileUtils


#endif //__FILEUTILS_H_666_




