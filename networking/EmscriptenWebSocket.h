/*=====================================================================
EmscriptenWebSocket.h
---------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#if 1


#include "SocketInterface.h"
#include "../utils/CircularBuffer.h"
#include "../utils/Platform.h"
#include "../utils/Mutex.h"
#include "../utils/Condition.h"
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>


/*=====================================================================
EmscriptenWebSocket
-------------------
For use in a program compiled by Emscripten.
=====================================================================*/
class EmscriptenWebSocket : public SocketInterface
{
public:
	EmscriptenWebSocket();

	// protocol should be "ws" or "wss"
	void connect(const std::string& protocol, const std::string& hostname, int port);

	// Read 1 or more bytes from the socket, up to a maximum of max_num_bytes.  Returns number of bytes read.
	// Returns zero if connection was closed gracefully
	size_t readSomeBytes(void* buffer, size_t max_num_bytes) override;

	virtual void ungracefulShutdown() override {}

	virtual void waitForGracefulDisconnect() override {}

	virtual void startGracefulShutdown() override {}


	virtual void setNoDelayEnabled(bool enabled) override {} // NoDelay option is off by default.
	virtual void enableTCPKeepAlive(float period) override {}
	virtual void setAddressReuseEnabled(bool enabled) override {}

	virtual IPAddress getOtherEndIPAddress() const override { return IPAddress(); }
	virtual int getOtherEndPort() const override { return 0; }

	
	bool readable(double timeout_s) override { return read_buffer.nonEmpty(); }
	bool readable(EventFD& event_fd) override { return read_buffer.nonEmpty(); } // Block until either the socket is readable or the event_fd is signalled (becomes readable).
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

	static void test();

private:
	void readTo(void* buffer, size_t numbytes);

public:
	mutable Mutex mutex;
	CircularBuffer<uint8> read_buffer;
	Condition new_data_condition;
private:
	EMSCRIPTEN_WEBSOCKET_T socket;
};


typedef Reference<EmscriptenWebSocket> EmscriptenWebSocketRef;


#endif
