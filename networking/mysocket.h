/*=====================================================================
mysocket.h
----------
File created by ClassTemplate on Wed Apr 17 14:43:14 2002
Code By Nicholas Chapman.

Code Copyright Nicholas Chapman 2005.

=====================================================================*/
#ifndef __MYSOCKET_H_666_
#define __MYSOCKET_H_666_


//#pragma warning(disable : 4786)//disable long debug name warning

#if defined(WIN32) || defined(WIN64)
// Stop windows.h from defining the min() and max() macros
#define NOMINMAX
#include <winsock.h>
#else

#endif

#include <string>
#include "ipaddress.h"
#include "mystream.h"
//class Vec3;
class FractionListener;


class MySocketExcep
{
public:
	MySocketExcep(){}
	MySocketExcep(const std::string& message_) : message(message_) {}
	~MySocketExcep(){}

	const std::string& what() const { return message; }
private:
	std::string message;
};



/*=====================================================================
MySocket
--------
my TCP socket class.
Blocking.
Does both client and server sockets.
NOTE: fix exceptions
NOTE: handle copy semantics
=====================================================================*/
class MySocket : public MyStream
{
public:
	/*=====================================================================
	MySocket
	--------
	
	=====================================================================*/
	MySocket(const std::string hostname, int port);//client connect via DNS lookup
	MySocket(const IPAddress& ipaddress, int port);//client connect
	MySocket();//for server socket

	~MySocket();

	void bindAndListen(int port) throw (MySocketExcep);


	void acceptConnection(MySocket& new_socket) throw (MySocketExcep);

	void close();

	const IPAddress& getOtherEndIPAddress() const{ return otherend_ipaddr; }
	int getThisEndPort() const { return thisend_port; }


	virtual void write(float x);
	virtual void write(int x);
	virtual void write(unsigned short x);
	virtual void write(char x);
	virtual void write(unsigned char x);
	//virtual void write(const Vec3& vec);
	virtual void write(const std::string& s);//writes null-terminated string

	//-----------------------------------------------------------------
	//if u use this directly u must do host->network and vice versa byte reordering yourself
	//-----------------------------------------------------------------
	virtual void write(const void* data, int numbytes);
	void write(const void* data, int numbytes, FractionListener* frac);

	//virtual float readFloat();
	//virtual int readInt();
	//virtual char readChar();
	//virtual const Vec3 readVec3();

	virtual void readTo(float& x);
	virtual void readTo(int& x);
	virtual void readTo(unsigned short& x);
	virtual void readTo(char& x);
	virtual void readTo(unsigned char& x);
	//virtual void readTo(Vec3& x);
	virtual void readTo(std::string& x);
	virtual void readTo(void* buffer, int numbytes);
	void readTo(void* buffer, int numbytes, FractionListener* frac);

	void readTo(std::string& x, int numchars, FractionListener* frac);

	void pollRead(std::string& data_out);//non blocking, returns currently
	//queued incoming data up to some arbitrary limit such as 1024 bytes.

	void setNagleAlgEnabled(bool enabled);//on by default.

	//-----------------------------------------------------------------
	//if u use these directly u must do host->network and vice versa byte reordering yourself
	//-----------------------------------------------------------------
	//void read(void* buffer, int readlen);

	//reads data until buffer not entirely filled.  Not guaranteed to return the whole reply
//	void readMessage(std::string& data_out, FractionListener* frac = NULL);

	//-----------------------------------------------------------------
	//funcs for measuring data rate
	//-----------------------------------------------------------------
	static int getNumBytesSent();
	static int getNumBytesRcvd();

	static void resetNumBytesSent();
	static void resetNumBytesRcvd();

private:
	MySocket(const MySocket& other);
	MySocket& operator = (const MySocket& other);

	void doConnect(const IPAddress& ipaddress, int port);

#if defined(WIN32) || defined(WIN64)
	typedef SOCKET SOCKETHANDLE_TYPE;
	SOCKETHANDLE_TYPE sockethandle;
#else
	typedef int SOCKETHANDLE_TYPE;
	SOCKETHANDLE_TYPE sockethandle;
#endif
	bool isSockHandleValid(SOCKETHANDLE_TYPE handle);
	static void initFDSetWithSocket(fd_set& sockset, SOCKETHANDLE_TYPE& sockhandle);

	IPAddress otherend_ipaddr;
	IPAddress thisend_ipaddr;
	int thisend_port;
	int otherend_port;


	static int num_bytes_sent;
	static int num_bytes_rcvd;
	//NOTE: not threadsafe
};





#endif //__MYSOCKET_H_666_




