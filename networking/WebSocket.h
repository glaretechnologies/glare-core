/*=====================================================================
WebSocket.h
-----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "SocketInterface.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include "../utils/BufferOutStream.h"
#include "../utils/BufferInStream.h"
class FractionListener;
class EventFD;


/*=====================================================================
WebSocket
---------
Implements reading and writing over a websocket connection.
See https://tools.ietf.org/html/rfc6455 for the websocket specification.

The write methods append data to a local buffer, which is written to the underlying socket in the flush() method.
=====================================================================*/
class WebSocket : public SocketInterface
{
public:
	WebSocket(SocketInterfaceRef underlying_socket);

	~WebSocket();


	// Calls shutdown on the socket, then closes the socket handle.
	// This will cause the socket to return from any blocking calls.
	virtual void ungracefulShutdown();

	// Wait for the other end to 'gracefully disconnect'
	// This allows the use of the 'Client Closes First' method from http://hea-www.harvard.edu/~fine/Tech/addrinuse.html
	// This is good because it allows the server to rebind to the same port without a long 30s wait on e.g. OS X.
	// Before this is called, the protocol should have told the other end to disconnect in some way. (e.g. a disconnect message)
	virtual void waitForGracefulDisconnect();


	//-----------------------------------------------------------------
	//if you use this directly you must do host->network and vice versa byte reordering yourself
	//-----------------------------------------------------------------
	void write(const void* data, size_t numbytes);
	void write(const void* data, size_t numbytes, FractionListener* frac);


	// Read 1 or more bytes from the socket, up to a maximum of max_num_bytes.  Returns number of bytes read.
	// Returns zero if connection was closed gracefully
	size_t readSomeBytes(void* buffer, size_t max_num_bytes);

	virtual void setNoDelayEnabled(bool enabled); // NoDelay option is off by default.

	virtual void enableTCPKeepAlive(float period);

	virtual void setAddressReuseEnabled(bool enabled);

	virtual IPAddress getOtherEndIPAddress() const { return underlying_socket->getOtherEndIPAddress(); }
	virtual int getOtherEndPort() const { return underlying_socket->getOtherEndPort(); }

	virtual void flush();


	void readTo(void* buffer, size_t numbytes);
	void readTo(void* buffer, size_t numbytes, FractionListener* frac);


	bool readable(double timeout_s);
	bool readable(EventFD& event_fd); // Block until either the socket is readable or the event_fd is signalled (becomes readable).
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

private:
	WebSocket(const WebSocket& other);
	WebSocket& operator = (const WebSocket& other);

	void writeDataInFrame(uint8 opcode, const uint8* data, size_t datalen);

	BufferInStream buffer_in;
	BufferOutStream buffer_out;

	js::Vector<uint8, 16> socket_buffer;
	size_t frame_start_index;
	uint8 masking_key[4];
	bool read_header;
	size_t header_size;
	size_t payload_len;
	uint32 header_opcode;

	SocketInterfaceRef underlying_socket;
};


typedef Reference<WebSocket> WebSocketRef;
