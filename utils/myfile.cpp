/*=====================================================================
myfile.cpp
----------
File created by ClassTemplate on Mon Aug 27 21:24:09 2001
Code By Nicholas Chapman.
=====================================================================*/
#include "myfile.h"


#include <assert.h>
#include "../maths/vec3.h"
#include <vector>

MyFile::MyFile()
{
	file = NULL;
	fileopen = false;
	is_endoffile = false;
}


MyFile::MyFile(const std::string& filename, const char* openmode)// throw(FileNotFoundExcep)
{
//	endoffile = false;
	is_endoffile = false;

	file = fopen(filename.c_str(), openmode);


	if(!file)
	{
		//assert(0);
		//NOTE: fixme

		fileopen = false;

		throw FileNotFoundExcep();

		//return;
	}

	fileopen = true;
}




MyFile::~MyFile()
{
	if(file)//NOTE: this nes?
	{
		fclose(file);
		file = NULL;
	}
}


void MyFile::open(const std::string& filename, const char* openmode)// throw(FileNotFoundExcep)
{
	assert(file == NULL && fileopen == false);


	file = fopen(filename.c_str(), openmode);

	if(!file)
	{
		//assert(0);
		//NOTE: fixme

		fileopen = false;

		throw FileNotFoundExcep();

		//return false;
	}

	fileopen = true;
}

void MyFile::close()
{
	if(file)//NOTE: this nes?
	{
		fclose(file);
		file = NULL;
	}

	fileopen = false;
}

void* MyFile::readEntireFile(int& bytes_read_out)
{
	//NOTE: checkme

	assert(file && fileopen);
	//int __cdecl fseek(FILE *, long, int);
	//-----------------------------------------------------------------
	//seek to end of file
	//-----------------------------------------------------------------
	fseek(file, 0, SEEK_END);

	int bytes_moved = ftell(file);

	//-----------------------------------------------------------------
	//seek back to beginning
	//-----------------------------------------------------------------
	fseek(file, 0, SEEK_SET);

	//-----------------------------------------------------------------
	//alloc storage for data
	//-----------------------------------------------------------------
	void* buffer = new unsigned char[bytes_moved];

	//-----------------------------------------------------------------
	//read in file to buffer
	//-----------------------------------------------------------------
	fread(buffer, bytes_moved, 1, file);

	//-----------------------------------------------------------------
	//set 'bytes_read_out'
	//-----------------------------------------------------------------
	bytes_read_out = bytes_moved;

	//-----------------------------------------------------------------
	//return data
	//-----------------------------------------------------------------
	return buffer;
}

//void MyFile::readEntireFile(Array<char>& data_out)
//{
//	assert(file && fileopen);
//	//int __cdecl fseek(FILE *, long, int);
//	//-----------------------------------------------------------------
//	//seek to end of file
//	//-----------------------------------------------------------------
//	fseek(file, 0, SEEK_END);
//
//	int bytes_moved = ftell(file);
//
//	//-----------------------------------------------------------------
//	//seek back to beginning
//	//-----------------------------------------------------------------
//	fseek(file, 0, SEEK_SET);
//
//	//-----------------------------------------------------------------
//	//alloc storage for data
//	//-----------------------------------------------------------------
//	data_out.resize(bytes_moved);
//
//	//-----------------------------------------------------------------
//	//read in file to buffer
//	//-----------------------------------------------------------------
//	fread(data_out.begin(), bytes_moved, 1, file);
//}

void MyFile::readEntireFile(std::vector<char>& data_out)
{
	assert(file && fileopen);

	//int __cdecl fseek(FILE *, long, int);
	//-----------------------------------------------------------------
	//seek to end of file
	//-----------------------------------------------------------------
	fseek(file, 0, SEEK_END);

	int bytes_moved = ftell(file);

	//-----------------------------------------------------------------
	//seek back to beginning
	//-----------------------------------------------------------------
	fseek(file, 0, SEEK_SET);

	//-----------------------------------------------------------------
	//alloc storage for data
	//-----------------------------------------------------------------
	data_out.resize(bytes_moved);

	//-----------------------------------------------------------------
	//read in file to buffer
	//-----------------------------------------------------------------
	fread(&(*data_out.begin()), bytes_moved, 1, file);
}



void MyFile::readFileAsString(std::string& file_out)
{
	int bytesize;
	char* data = (char*)readEntireFile(bytesize);

	file_out = data;
}

bool MyFile::areFilesEqual(const std::string& file1name, const std::string& file2name)
{
	MyFile file1(file1name, "rb");
	MyFile file2(file1name, "rb");

	int file1size;
	
	//void* readEntireFile(int& bytes_read_out);
	char* file1data = (char*)file1.readEntireFile(file1size);

	int file2size;
	char* file2data = (char*)file2.readEntireFile(file2size);

	if(file1size != file2size)
	{
		delete[] file1data;
		delete[] file2data;
		return false;
	}

	for(int i=0; i<file1size; i++)
	{
		if(file1data[i] != file1data[i])
		{
			delete[] file1data;
			delete[] file2data;
			return false;
		}
	}

	delete[] file1data;
	delete[] file2data;
	return true;
}



void MyFile::print(const std::string& text)
{
	assert(file && fileopen);

	fprintf(file, text.c_str());
}

void MyFile::printLine(const std::string& line)//appends \n char
{
	print(line + "\n");
}



const std::string MyFile::readLine()//WITH \n as last char
{
	std::string line;

	while(1)
	{
		int c = readChar();

		if(c == EOF)
		{
			return line;
		}
		else if(endOfFile())//these separate?
		{
			return line;//shouldn't get here?
		}

		line += c;

		if(c == '\n' || c == 13)
		{
			return line;
		}
	}
}



void MyFile::flushWrites()
{
	fflush(file);
}
	
int MyFile::read(void* target_buffer, int num_bytes_to_read)
{
	return fread(target_buffer, num_bytes_to_read, 1, file);
}

void MyFile::write(const void* buffer, int num_bytes_to_write)
{
	fwrite(buffer, num_bytes_to_write, 1, file);
}


float MyFile::readFloat()
{
	float f;
	fread(&f, sizeof(float), 1, file);
	
	return f;
}

int MyFile::readInt()
{
	int i;
	fread(&i, sizeof(int), 1, file);
	
	return i;
}


short MyFile::readShortInt()
{
	short s;
	fread(&s, sizeof(short), 1, file);
	
	return s;
}

void MyFile::readTo(void* buffer, int numbytes)
{
	read(buffer, numbytes);
}



void MyFile::writeInt(int i)
{
	fwrite(&i, sizeof(int), 1, file);
}

void MyFile::writeFloat(float f)
{
	fwrite(&f, sizeof(float), 1, file);
}





void MyFile::write(float x)
{
	writeFloat(x);
}

void MyFile::write(int x)
{
	writeInt(x);
}

void MyFile::write(unsigned short x)
{
	fwrite(&x, sizeof(unsigned short), 1, file);
}

void MyFile::write(char x)
{
	fwrite(&x, sizeof(char), 1, file);
}

void MyFile::write(unsigned char x)
{
	fwrite(&x, sizeof(unsigned char), 1, file);
}

/*void MyFile::write(const Vec3& vec)
{
	writeFloat(vec.x);
	writeFloat(vec.y);
	writeFloat(vec.z);
}*/

void MyFile::write(const std::string& s)//writes null-terminated string
{
	fwrite(s.c_str(), s.size() + 1, 1, file);
}
	

void MyFile::readTo(float& x)
{
	x = readFloat();
}

void MyFile::readTo(int& x)
{
	x = readInt();
}

void MyFile::readTo(unsigned short& x)
{
	fread(&x, sizeof(unsigned short), 1, file);
}

void MyFile::readTo(char& x)
{
	fread(&x, sizeof(char), 1, file);
}

/*void MyFile::readTo(Vec3& vec)
{
	vec.x = readFloat();
	vec.y = readFloat();
	vec.z = readFloat();
}*/

void MyFile::readTo(std::string& x, unsigned int maxlength)
{
	/*std::vector<char> buffer(100);

	int i = 0;
	while(1)
	{
		buffer.push_back('\0');
		readTo(buffer[i]);

		if(buffer[i] == '\0')
			break;

		//TODO: break after looping to long
		++i;
	}

	x = &(*buffer.begin());*/

	assert(0);
}
