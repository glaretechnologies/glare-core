/*=====================================================================
pollsocket.h
------------
File created by ClassTemplate on Sun Sep 05 21:45:54 2004
Code By Nicholas Chapman.

Code Copyright Nicholas Chapman 2005.

=====================================================================*/
#ifndef __POLLSOCKET_H_666_
#define __POLLSOCKET_H_666_



#pragma warning(disable : 4786)//disable long debug name warning

#include <winsock.h>
#include <string>
#include "ipaddress.h"
#include "mystream.h"
#include <deque>



class PollSocketExcep
{
public:
	PollSocketExcep(const std::string& message_) : message(message_) {}
	~PollSocketExcep(){}

	const std::string& what() const { return message; }
private:
	std::string message;
};




/*=====================================================================
PollSocket
----------
non-blocking socket.
=====================================================================*/
class PollSocket : public MyStream
{
public:
	/*=====================================================================
	PollSocket
	----------
	
	=====================================================================*/
	PollSocket();
	PollSocket(const std::string hostname, int port);//client connect via DNS lookup
		//throws PollSocketExcep
	PollSocket(const IPAddress& ipaddress, int port);//client connect
		//throws PollSocketExcep

	~PollSocket();


	void bindAndListen(int port) throw (PollSocketExcep);
		//throws PollSocketExcep


	void acceptConnection();

	//returns true if a connection accepted.

	//getState() must be CONNECTED
	void getWorkerSocket(PollSocket& new_socket) throw (PollSocketExcep);

	//may be NULL
	//PollSocket* getWorkerSocket() throw (PollSocketExcep);


	//TODO: make this non-blocking.
	//void write(const void* data, int numbytes);
		//throws PollSocketExcep if connection lost

	//this needs to be called fairly often.
	void think();//throws PollSocketExcep if connection lost

	int getInBufferSize() const;
	//void readBytes(void* buffer, int numbytes);

	//------------------------------------------------------------------------
	//MyStream interface methods
	//------------------------------------------------------------------------
	virtual void write(float x);
	virtual void write(int x);
	virtual void write(unsigned short x);
	virtual void write(char x);
	virtual void write(unsigned char x);
	//virtual void write(const Vec3& vec);
	virtual void write(const std::string& s);//writes null-terminated string
	virtual void write(const void* data, int numbytes);

	//for all read funcs below, num bytes reading must be <= getInBufferSize().
	virtual void readTo(float& x);
	virtual void readTo(int& x);
	virtual void readTo(unsigned short& x);
	virtual void readTo(char& x);
	virtual void readTo(unsigned char& x);
	//virtual void readTo(Vec3& x);
	virtual void readTo(std::string& x);
	virtual void readTo(void* buffer, int numbytes);
	//------------------------------------------------------------------------
	//
	//------------------------------------------------------------------------

	
	enum State
	{
		//CREATED,
		ACCEPTING,
		ACCEPTED,
		CONNECTING,//in a call to connect()
		CONNECTED,
		CLOSED
	};

	State getState() const { return state; }

	void close();

		
	const IPAddress& getOtherEndIPAddress() const { return otherend_ipaddr; }
	const int getOtherEndPort() const { return otherend_port; }


	//-----------------------------------------------------------------
	//funcs for measuring data rate
	//-----------------------------------------------------------------
	static int getNumBytesSent();
	static int getNumBytesRcvd();

	static void resetNumBytesSent();
	static void resetNumBytesRcvd();


private:
	void doConnect(const IPAddress& ipaddress, int port);

	std::deque<char> in_buffer;

	State state;

	SOCKET sockethandle;

	IPAddress otherend_ipaddr;
	//IPAddress thisend_ipaddr;
	//int thisend_port;
	int otherend_port;


	SOCKET worker_socket;

	static int num_bytes_sent;
	static int num_bytes_rcvd;
	//NOTE: not threadsafe

};



#endif //__POLLSOCKET_H_666_




