/*=====================================================================
StreamUtils.h
-------------
File created by ClassTemplate on Fri May 29 13:14:27 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __STREAMUTILS_H_666_
#define __STREAMUTILS_H_666_


#include <iostream>
#include <vector>
#include "Array4D.h"


/*=====================================================================
StreamUtils
-----------

=====================================================================*/
namespace StreamUtils
{
	class StreamUtilsExcep
	{
	public:
		StreamUtilsExcep(const std::string& s_) : s(s_) {}
		~StreamUtilsExcep(){}
		const std::string& what() const { return s; }
	private:
		std::string s;
	};


	template <class T>
	inline static void writeBasic(std::ostream& stream, const T& t)
	{
		stream.write((const char*)&t, sizeof(T));
	}


	template <class T>
	inline static void readBasic(std::istream& stream, T& t_out)
	{
		stream.read((char*)&t_out, sizeof(T));
	}


	inline static void write(std::ostream& stream, float x)
	{
		writeBasic(stream, x);
	}


	inline static void read(std::istream& stream, float& x)
	{
		readBasic(stream, x);
	}


	inline static void write(std::ostream& stream, unsigned int x)
	{
		writeBasic(stream, x);
	}


	inline static void read(std::istream& stream, unsigned int& x)
	{
		readBasic(stream, x);
	}


	template <class T>
	inline static void writeBasicVector(std::ostream& stream, const std::vector<T>& t)
	{
		const size_t sz = t.size();
		writeBasic(stream, sz);
		//for(size_t i=0; i<t.size(); ++i)
		//	writeBasic(stream, t[i]);
		if(sz > 0)
			stream.write((const char*)&t[0], sizeof(T) * t.size());

		if(stream.bad())
			throw StreamUtilsExcep("Write to stream failed.");
	}


	template <class T>
	inline static void readBasicVector(std::istream& stream, std::vector<T>& t)
	{
		unsigned int sz;
		readBasic(stream, sz);
		t.resize(sz);
		//for(size_t i=0; i<t.size(); ++i)
		//	readBasic(stream, t[i]);

		if(sz > 0)
			stream.read((char*)&t[0], sizeof(T) * t.size());
	}


	template <class T>
	void write(std::ostream& stream, const Array4D<T>& x) // throws StreamUtilsExcep
	{
		const unsigned int dx = x.dX();
		const unsigned int dy = x.dY();
		const unsigned int dz = x.dZ();
		const unsigned int dw = x.dW();
		writeBasic(stream, dx);
		writeBasic(stream, dy);
		writeBasic(stream, dz);
		writeBasic(stream, dw);
		writeBasicVector(stream, x.getData());

		if(stream.bad())
			throw StreamUtilsExcep("Write to stream failed.");
	}


	template <class T>
	void read(std::istream& stream, Array4D<T>& x)
	{
		unsigned int dx, dy, dz, dw;
		readBasic(stream, dx);
		readBasic(stream, dy);
		readBasic(stream, dz);
		readBasic(stream, dw);

		x = Array4D<T>(dx, dy, dz, dw);
		readBasicVector(stream, x.getData());
	}

}


#endif //__STREAMUTILS_H_666_
