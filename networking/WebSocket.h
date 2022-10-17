/*=====================================================================
WebSocket.h
-----------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "SocketInterface.h"
#include "../utils/BufferOutStream.h"
#include "../utils/Vector.h"
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

	virtual ~WebSocket();


	// Calls shutdown on the socket, then closes the socket handle.
	// This will cause the socket to return from any blocking calls.
	virtual void ungracefulShutdown() override;

	// Wait for the other end to 'gracefully disconnect'
	// This allows the use of the 'Client Closes First' method from http://hea-www.harvard.edu/~fine/Tech/addrinuse.html
	// This is good because it allows the server to rebind to the same port without a long 30s wait on e.g. OS X.
	// Before this is called, the protocol should have told the other end to disconnect in some way. (e.g. a disconnect message)
	virtual void waitForGracefulDisconnect() override;


	virtual void startGracefulShutdown() override;


	//-----------------------------------------------------------------
	//if you use this directly you must do host->network and vice versa byte reordering yourself
	//-----------------------------------------------------------------
	void write(const void* data, size_t numbytes);
	void write(const void* data, size_t numbytes, FractionListener* frac);


	// Read 1 or more bytes from the socket, up to a maximum of max_num_bytes.  Returns number of bytes read.
	// Returns zero if connection was closed gracefully
	virtual size_t readSomeBytes(void* buffer, size_t max_num_bytes) override;

	virtual void setNoDelayEnabled(bool enabled) override; // NoDelay option is off by default.

	virtual void enableTCPKeepAlive(float period) override;

	virtual void setAddressReuseEnabled(bool enabled) override;

	virtual IPAddress getOtherEndIPAddress() const override { return underlying_socket->getOtherEndIPAddress(); }
	virtual int getOtherEndPort() const override { return underlying_socket->getOtherEndPort(); }

	virtual void flush() override;


	void readTo(void* buffer, size_t numbytes);
	void readTo(void* buffer, size_t numbytes, FractionListener* frac);


	bool readable(double timeout_s) override;
	bool readable(EventFD& event_fd) override; // Block until either the socket is readable or the event_fd is signalled (becomes readable).
	// Returns true if the socket was readable, false if the event_fd was signalled.



	//------------------------ InStream ---------------------------------
	virtual int32 readInt32() override;
	virtual uint32 readUInt32() override;
	virtual void readData(void* buf, size_t num_bytes) override;
	virtual bool endOfStream() override;
	//------------------------------------------------------------------

	//------------------------ OutStream --------------------------------
	virtual void writeInt32(int32 x) override;
	virtual void writeUInt32(uint32 x) override;
	virtual void writeData(const void* data, size_t num_bytes) override;
	//------------------------------------------------------------------

private:
	WebSocket(const WebSocket& other);
	WebSocket& operator = (const WebSocket& other);

	void writeDataInFrame(uint8 opcode, const uint8* data, size_t datalen);

	BufferOutStream buffer_out;

	js::Vector<uint8, 16> temp_buffer;

	uint8 masking_key[4];
	bool need_header_read;
	size_t payload_len;
	size_t payload_remaining;
	uint32 header_opcode;

	SocketInterfaceRef underlying_socket;
};


typedef Reference<WebSocket> WebSocketRef;
