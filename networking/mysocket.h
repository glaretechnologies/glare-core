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
#include <sys/select.h>
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


class AbortedMySocketExcep : public MySocketExcep
{
public:
	AbortedMySocketExcep() : MySocketExcep("Aborted.") {}
};


class SocketShouldAbortCallback
{
public:
	virtual ~SocketShouldAbortCallback(){}

	virtual bool shouldAbort() = 0;
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
class MySocket// : public MyStream
{
public:
	/*=====================================================================
	MySocket
	--------
	
	=====================================================================*/
	MySocket(const std::string hostname, int port);//client connect via DNS lookup
	MySocket(const IPAddress& ipaddress, int port);//client connect
	MySocket();//for server socket

	virtual ~MySocket();

	void bindAndListen(int port);// throw (MySocketExcep);


	void acceptConnection(MySocket& new_socket, SocketShouldAbortCallback* should_abort_callback);// throw (MySocketExcep);

	void close();

	const IPAddress& getOtherEndIPAddress() const{ return otherend_ipaddr; }
	int getThisEndPort() const { return thisend_port; }
	int getOtherEndPort() const { return otherend_port; }


	virtual void write(float x, SocketShouldAbortCallback* should_abort_callback);
	virtual void write(int x, SocketShouldAbortCallback* should_abort_callback);
	virtual void write(unsigned short x, SocketShouldAbortCallback* should_abort_callback);
	virtual void write(char x, SocketShouldAbortCallback* should_abort_callback);
	virtual void write(unsigned char x, SocketShouldAbortCallback* should_abort_callback);
	//virtual void write(const Vec3& vec);
	virtual void write(const std::string& s, SocketShouldAbortCallback* should_abort_callback); // Writes string

	//-----------------------------------------------------------------
	//if u use this directly u must do host->network and vice versa byte reordering yourself
	//-----------------------------------------------------------------
	virtual void write(const void* data, int numbytes, SocketShouldAbortCallback* should_abort_callback);
	void write(const void* data, int numbytes, FractionListener* frac, SocketShouldAbortCallback* should_abort_callback);

	//virtual float readFloat();
	//virtual int readInt();
	//virtual char readChar();
	//virtual const Vec3 readVec3();

	virtual void readTo(float& x, SocketShouldAbortCallback* should_abort_callback);
	virtual void readTo(int& x, SocketShouldAbortCallback* should_abort_callback);
	virtual void readTo(unsigned short& x, SocketShouldAbortCallback* should_abort_callback);
	virtual void readTo(char& x, SocketShouldAbortCallback* should_abort_callback);
	virtual void readTo(unsigned char& x, SocketShouldAbortCallback* should_abort_callback);
	//virtual void readTo(Vec3& x);
	virtual void readTo(std::string& x, int maxlength, SocketShouldAbortCallback* should_abort_callback); // read string, of up to maxlength chars
	virtual void readTo(void* buffer, int numbytes, SocketShouldAbortCallback* should_abort_callback);
	void readTo(void* buffer, int numbytes, FractionListener* frac, SocketShouldAbortCallback* should_abort_callback);

	void readTo(std::string& x, int numchars, FractionListener* frac, SocketShouldAbortCallback* should_abort_callback);

	//void pollRead(std::string& data_out);//non blocking, returns currently
	//queued incoming data up to some arbitrary limit such as 1024 bytes.

	void setNagleAlgEnabled(bool enabled);//on by default.

	bool readable(double timeout_s);

	//-----------------------------------------------------------------
	//if u use these directly u must do host->network and vice versa byte reordering yourself
	//-----------------------------------------------------------------
	//void read(void* buffer, int readlen);

	//reads data until buffer not entirely filled.  Not guaranteed to return the whole reply
//	void readMessage(std::string& data_out, FractionListener* frac = NULL);

	//-----------------------------------------------------------------
	//funcs for measuring data rate
	//-----------------------------------------------------------------
	//static int getNumBytesSent();
	//static int getNumBytesRcvd();

	//static void resetNumBytesSent();
	//static void resetNumBytesRcvd();

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


	//static int num_bytes_sent;
	//static int num_bytes_rcvd;
	//NOTE: not threadsafe
};





#endif //__MYSOCKET_H_666_




