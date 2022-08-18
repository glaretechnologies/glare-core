/*=====================================================================
FileUtils.h
-----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "Exception.h"
#include <string>
#include <vector>


namespace FileUtils
{


class FileUtilsExcep : public glare::Exception
{
public:
	FileUtilsExcep(const std::string& msg) : glare::Exception(msg) {}
	~FileUtilsExcep(){}
};


// Join two paths together using the directory separator.  If filename is an absolute path then just returns it.
const std::string join(const std::string& dirpath, const std::string& filename);

// Converts backslashes to forwards slashes on Linux/mac, and forwards slashes to backslashes on Windows.
const std::string toPlatformSlashes(const std::string& pathname);

void createDir(const std::string& dirname);
void createDirsForPath(const std::string& path);

void createDirIfDoesNotExist(const std::string& dirname);

// Gets the directory of a file from the pathname.
// If the pathname is just the filename, returns ""
// Path will *not* have a trailing slash
const std::string getDirectory(const std::string& pathname);

// Gets the filename of a file from the path.
const std::string getFilename(const std::string& pathname); // throws FileUtilsExcep

// Returns only the filenames, not the full paths.  Ignores the pseudo-files '.' and '..'.
const std::vector<std::string> getFilesInDir(const std::string& dir_path); // throws FileUtilsExcep

// Returns the full filenames.  Ignores the pseudo-files '.' and '..'.
const std::vector<std::string> getFilesInDirFullPaths(const std::string& dir_path); // throws FileUtilsExcep

// Returns file paths in given dir, for files that have the given extension.  Returns the full filenames.  Ignores the pseudo-files '.' and '..'.
const std::vector<std::string> getFilesInDirWithExtensionFullPaths(const std::string& dir_path, const std::string& extension, bool sort_results = false); // throws FileUtilsExcep
const std::vector<std::string> getFilesInDirWithExtensionFullPathsRecursive(const std::string& dir_path, const std::string& extension); // throws FileUtilsExcep
const std::vector<std::string> getSortedFilesInDirWithExtensionFullPathsRecursive(const std::string& dir_path, const std::string& extension); // throws FileUtilsExcep

// Returns true if file exists, false otherwise.  Returns true for directories as well.
bool fileExists(const std::string& pathname); // Does not throw any exceptions.

// Returns the size of the file in bytes.  Throws FileUtilsExcep on failure, for example if the file does not exist.
uint64 getFileSize(const std::string& pathname);

void getDirectoriesFromPath(const std::string& pathname_, std::vector<std::string>& dirs_out);

bool isDirectory(const std::string& pathname);

bool isPathAbsolute(const std::string& p);

// Returns a path to path_b that is relative to dir_path.
// If path_b is already relative, it just returns path_b.
// dir_path is treated as a path to a directory, and path_b as a path to a file.
const std::string getRelativePath(const std::string& dir_path, const std::string& path_b);

// Returns true if pathname is relative, and does not contain any '..' dirs.
bool isPathSafe(const std::string& pathname);

std::string readEntireFile(const std::string& pathname); // Returns file contents directly.  Throws FileUtilsExcep
void readEntireFile(const std::string& pathname, std::string& filecontents_out); // throws FileUtilsExcep
void readEntireFile(const std::string& pathname, std::vector<unsigned char>& filecontents_out); // throws FileUtilsExcep

// Read the contents of a file.  If the file was unable to be opened or read from, retry until it succeeds.
// Will keep trying until success or total_retry_period has elapsed, in which case an exception is thrown.
void readEntireFileWithRetries(const std::string& pathname, double total_retry_period, std::vector<unsigned char>& filecontents_out); // throws FileUtilsExcep

void writeEntireFile(const std::string& pathname, const std::vector<unsigned char>& filecontents); // throws FileUtilsExcep
void writeEntireFile(const std::string& pathname, const std::string& filecontents); // throws FileUtilsExcep
void writeEntireFileTextMode(const std::string& pathname, const std::string& filecontents); // throws FileUtilsExcep
void writeEntireFile(const std::string& pathname, const char* data, size_t data_size); // throws FileUtilsExcep
void writeEntireFileAtomically(const std::string& pathname, const char* data, size_t data_size); // throws FileUtilsExcep
std::string writeEntireFileToTempFile(const char* data, size_t data_size); // Returns path to temp file.  throws FileUtilsExcep

// Reads the entire file, but does some conversion - converts CRLF to LF.
void readEntireFileTextMode(const std::string& pathname, std::string& s_out); // throws FileUtilsExcep
std::string readEntireFileTextMode(const std::string& pathname); // throws FileUtilsExcep

void copyFile(const std::string& srcpath, const std::string& dstpath);

// Atomic filesystem operation (hopefully)
// throws FileUtilsExcep on failure
void moveFile(const std::string& srcpath, const std::string& dstpath);

void deleteFile(const std::string& path);

void deleteEmptyDirectory(const std::string& path);
void deleteDirectoryRecursive(const std::string& path);
void deleteFilesInDir(const std::string& path); // Delete just the files, not directories, in the given dir.

// Only implemented on Windows.
uint64 getFileCreatedTime(const std::string& filename);

// NOTE: this function call is rather vulnerable to handle leaks.  Prefer to use the FileHandle class instead.
// Returns NULL pointer on failure.
FILE* openFile(const std::string& pathname, const std::string& openmode);

// Returns -1 on failure.
int openFileDescriptor(const std::string& pathname, int open_flags);

int openFileDescriptor(const std::string& pathname, int open_flags, int perm_mode);

// Remove non alphanumeric characters etc..
const std::string makeOSFriendlyFilename(const std::string& name);

// Returns the canonical path for the given file.  The file must exist, and path must be a valid path for it.
const std::string getCanonicalPath(const std::string& path);

// Changes slashes to platform slashes.
// Then returns the canonical path name.
const std::string getActualOSPath(const std::string& path); // throws FileUtilsExcep

// Changes slashes to platform slashes.
// If file not found directly, tries to guess the correct case by scanning the directory and doing case-insensitive matches.
// Then returns the canonical path name.
const std::string getActualOSPathWithDirScanning(const std::string& path); // throws FileUtilsExcep

// Returns the platform path separator.
const std::string getPlafromPathSeparator();


#if defined(_WIN32) && !defined(__MINGW32__)
const std::wstring convertUTF8ToFStreamPath(const std::string& p);
#else
const std::string convertUTF8ToFStreamPath(const std::string& p);
#endif


void doUnitTests();


} // end namespace FileUtils
