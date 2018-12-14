/*=====================================================================
TestSocket.h
------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "SocketInterface.h"
#include <vector>
#include <list>
#include "../utils/Platform.h"
class FractionListener;


/*=====================================================================
TestSocket
----------

=====================================================================*/
class TestSocket : public SocketInterface
{
public:
	TestSocket();
	
	// Read 1 or more bytes from the socket, up to a maximum of max_num_bytes.  Returns number of bytes read.
	// Returns zero if connection was closed gracefully
	size_t readSomeBytes(void* buffer, size_t max_num_bytes);

	virtual void ungracefulShutdown() {}


	virtual void setNoDelayEnabled(bool enabled) {} // NoDelay option is off by default.
	virtual void enableTCPKeepAlive(float period) {}
	virtual void setAddressReuseEnabled(bool enabled) {}

	// Enable TCP Keep-alive, and set the period between keep-alive messages to 'period' seconds.
	//void enableTCPKeepAlive(float period);

	bool readable(double timeout_s) { return true; }
	bool readable(EventFD& event_fd) { return true; } // Block until either the socket is readable or the event_fd is signalled (becomes readable).  
	// Returns true if the socket was readable, false if the event_fd was signalled.
	


	//------------------------ InStream ---------------------------------
	virtual int32 readInt32();
	virtual uint32 readUInt32();
	virtual void readData(void* buf, size_t num_bytes);
	virtual bool endOfStream();
	//------------------------------------------------------------------

	//------------------------ OutStream --------------------------------
	virtual void writeInt32(int32 x);
	virtual void writeUInt32(uint32 x);
	virtual void writeData(const void* data, size_t num_bytes);
	//------------------------------------------------------------------

	static void test();

	// A queue of buffers
	std::list<std::vector<uint8> > buffers;
	size_t read_i;

private:
	void readTo(void* buffer, size_t numbytes);
	void readTo(void* buffer, size_t numbytes, FractionListener* frac);
};


typedef Reference<TestSocket> TestSocketRef;
