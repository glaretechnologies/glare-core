/*=====================================================================
rectangle.h
-----------
File created by ClassTemplate on Sat Jun 11 18:09:39 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __RECTANGLE_H_666_
#define __RECTANGLE_H_666_



//#include "../networking/mystream.h"


namespace dlf
{




/*=====================================================================
Rectangle
---------

=====================================================================*/
class Rectangle
{
public:
	/*=====================================================================
	Rectangle
	---------
	
	=====================================================================*/
	Rectangle(int x, int y, int width, int height);

	~Rectangle();


	inline int maxX() const { return x + width; }
	inline int maxY() const { return y + height; }

	int x;
	int y;
	int width;
	int height;

};


/*inline MyStream& operator << (MyStream& stream, const Rectangle& rect)
{
	stream << rect.x;
	stream << rect.y;	
	stream << rect.width;
	stream << rect.height;
	return stream;
}

inline MyStream& operator >> (MyStream& stream, Rectangle& rect)
{
	stream >> rect.x;
	stream >> rect.y;	
	stream >> rect.width;
	stream >> rect.height;
	return stream;
}*/

} //end namespace dlf


#endif //__RECTANGLE_H_666_




