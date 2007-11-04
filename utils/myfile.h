/*=====================================================================
myfile.h
--------
File created by ClassTemplate on Mon Aug 27 21:24:09 2001
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MYFILE_H_666_
#define __MYFILE_H_666_


#include "../networking/mystream.h"
#include <stdio.h>
#include <string>
#include "array.h"
#include <vector>


class FileNotFoundExcep
{};






//just a simple class to wrap C-style file io
class MyFile : public MyStream
{
public:
	MyFile();

	//throws FileNotFoundExcep if fails to open file
	MyFile(const std::string& filename, const char* openmode);// throw(FileNotFoundExcep);
	~MyFile();


	//throws FileNotFoundExcep if fails to open file
	void open(const std::string& filename, const char* openmode);// throw(FileNotFoundExcep);

	void close();

	inline int readChar();
	inline bool endOfFile();

	//-----------------------------------------------------------------
	//returns newly allocated buffer containing file, 
	//sets 'bytes_read_out to number of bytes in file.
	//-----------------------------------------------------------------
	void* readEntireFile(int& bytes_read_out);
	void readEntireFile(Array<char>& data_out);
	void readEntireFile(std::vector<char>& data_out);

	void readFileAsString(std::string& file_out);

	inline bool isOpen() const { return fileopen; }

	static bool areFilesEqual(const std::string& file1name, const std::string& file2name);

	void print(const std::string& text);
	void printLine(const std::string& line);//appends \n char

	const std::string readLine();//WITH \n as last char


	//void write(const std::string& data);

	void flushWrites();


	//returns num bytes read
	int read(void* target_buffer, int num_bytes_to_read);
	//void write(const void* buffer, int num_bytes_to_write);

	float readFloat();
	int readInt();
	short readShortInt();

	void writeInt(int i);
	void writeFloat(float f);

	virtual void write(float x);
	virtual void write(int x);
	virtual void write(unsigned short x);
	virtual void write(char x);
	virtual void write(unsigned char x);
	//virtual void write(const Vec3& vec);
	virtual void write(const std::string& s);//writes null-terminated string
	virtual void write(const void* data, int numbytes);

	virtual void readTo(float& x);
	virtual void readTo(int& x);
	virtual void readTo(unsigned short& x);
	virtual void readTo(char& x);
	//virtual void readTo(Vec3& x);
	virtual void readTo(std::string& x, unsigned int maxlength);
	virtual void readTo(void* buffer, int numbytes);

private:
	FILE* file;
	bool fileopen;
	bool is_endoffile;//NOTE::work this out!!!!!!!!!!!
};


int MyFile::readChar()
{
	int c = getc(file);
	if(c == EOF)
		is_endoffile = true;

	return c;
		
}

bool MyFile::endOfFile()
{
	return is_endoffile || feof(file);
		
	//NOTE: check this
}




#endif //__MYFILE_H_666_




