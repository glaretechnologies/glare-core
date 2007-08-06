/*=====================================================================
bitmapio.h
----------
File created by ClassTemplate on Fri Dec 10 04:15:26 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __BITMAPIO_H_666_
#define __BITMAPIO_H_666_


#include <string>
class Bitmap;



class BitmapIOExcep
{
public:
	BitmapIOExcep(const std::string& s_) : s(s_) {}
	~BitmapIOExcep(){}

	const std::string& what() const { return s; }
private:
	std::string s;
};



/*=====================================================================
BitmapIO
--------

=====================================================================*/
class BitmapIO
{
public:
	/*=====================================================================
	BitmapIO
	--------
	
	=====================================================================*/
	BitmapIO();

	~BitmapIO();


	//throws BitmapIOExcep
	void saveBitmapToDisk(const Bitmap& bitmap, const std::string& pathname);

	//throws BitmapIOExcep
	void readBitmapFromDisk(const std::string& pathname, Bitmap& bitmap_out);


};



#endif //__BITMAPIO_H_666_




