/*=====================================================================
mysocket.h
----------
File created by ClassTemplate on Wed Apr 17 14:43:14 2002
Code By Nicholas Chapman.

Code Copyright Nicholas Chapman 2005.

=====================================================================*/
#ifndef __MYSOCKET_H_666_
#define __MYSOCKET_H_666_


#if defined(WIN32) || defined(WIN64)
// Stop windows.h from defining the min() and max() macros
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock.h>
#else
#include <sys/select.h>
#endif

#include "ipaddress.h"
//#include "mystream.h"
#include "../utils/platform.h"
#include <string>
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
TCP socket class.
Blocking.
Does both client and server sockets.
=====================================================================*/
class MySocket
{
public:
	MySocket(const std::string& hostname, int port); // Client connect via DNS lookup
	MySocket(const IPAddress& ipaddress, int port); // Client connect
	MySocket(); // For server socket

	~MySocket();

	void bindAndListen(int port); // throw (MySocketExcep);


	void acceptConnection(MySocket& new_socket, SocketShouldAbortCallback* should_abort_callback); // throw (MySocketExcep);

	void close();

	const IPAddress& getOtherEndIPAddress() const{ return otherend_ipaddr; }
	int getThisEndPort() const { return thisend_port; }
	int getOtherEndPort() const { return otherend_port; }


	void writeInt32(int32 x, SocketShouldAbortCallback* should_abort_callback);
	void writeUInt32(uint32 x, SocketShouldAbortCallback* should_abort_callback);
	void writeUInt64(uint64 x, SocketShouldAbortCallback* should_abort_callback);
	void writeString(const std::string& s, SocketShouldAbortCallback* should_abort_callback); // Write null-terminated string.

	//-----------------------------------------------------------------
	//if you use this directly you must do host->network and vice versa byte reordering yourself
	//-----------------------------------------------------------------
	void write(const void* data, size_t numbytes, SocketShouldAbortCallback* should_abort_callback);
	void write(const void* data, size_t numbytes, FractionListener* frac, SocketShouldAbortCallback* should_abort_callback);


	int32 readInt32(SocketShouldAbortCallback* should_abort_callback);
	uint32 readUInt32(SocketShouldAbortCallback* should_abort_callback);
	uint64 readUInt64(SocketShouldAbortCallback* should_abort_callback);
	const std::string readString(size_t max_string_length, SocketShouldAbortCallback* should_abort_callback); // Read null-terminated string.

	void readTo(void* buffer, size_t numbytes, SocketShouldAbortCallback* should_abort_callback);
	void readTo(void* buffer, size_t numbytes, FractionListener* frac, SocketShouldAbortCallback* should_abort_callback);

	void setNagleAlgEnabled(bool enabled); // On by default.

	bool readable(double timeout_s);

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
};


#endif //__MYSOCKET_H_666_
