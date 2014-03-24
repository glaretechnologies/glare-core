/*=====================================================================
packet.h
--------
File created by ClassTemplate on Sun Apr 14 10:14:17 2002
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PACKET_H_666_
#define __PACKET_H_666_


#include <vector>
#include "../utils/Platform.h"
#include <cstring> // for size_t


/*class MyPacketExcep
{public:
	MyPacketExcep(){}
	MyPacketExcep(const std::string& message_) : message(message_) {}
	~MyPacketExcep(){}

	const std::string& what() const { return message; }
private:
	std::string message;
};*/

const int MAX_PACKET_SIZE = 1000000;//in bytes


/*=====================================================================
Packet
------
For UDP data and possibly for TCP data
=====================================================================*/
class Packet //: public MyStream
{
public:
	/*=====================================================================
	Packet
	------
	
	=====================================================================*/
	Packet();

	~Packet();

	void resetReadIndex(){ readindex = 0; }


	//virtual void write(float x);
	virtual void writeInt32(int32 x);
	//virtual void write(unsigned short x);
	//virtual void write(char x);
	//virtual void write(unsigned char x);
	//virtual void write(const Vec3& vec);
	//virtual void write(const std::string& s);//writes null-terminated string
	virtual void write(const void* data, size_t numbytes);
	
	//virtual void readTo(float& x);
	virtual void readInt32To(int32& x);
	//virtual void readTo(unsigned short& x);
	//virtual void readTo(char& x);
	//virtual void readTo(Vec3& x);
	//virtual void readTo(std::string& x, int maxlength);
	//virtual void readTo(void* buffer, int numbytes);

	inline const char* getData() const { return &(*data.begin()); }
	inline char* getData(){ return &(*data.begin()); }
	inline int getPacketSize() const { return (int)data.size(); }

//	void writeToStreamSized(MyStream& stream);//write to stream with an int indicating the size first.
//	void readFromStreamSize(MyStream& stream);

//	void writeToStream(MyStream& stream);

	std::vector<char> data;//temp
	mutable int readindex;

private:
	//inline char* getData(){ return &(*data.begin()); }

	//char* data;
};



#endif //__PACKET_H_666_




