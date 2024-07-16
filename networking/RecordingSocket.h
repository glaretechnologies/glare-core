/*=====================================================================
RecordingSocket.h
-----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once



#include "MySocket.h"
#include <vector>


/*=====================================================================
RecordingSocket
---------------
A wrapper around an underlying socket, that records data read from it to a buffer.
The buffer can be written do disk.
=====================================================================*/
class RecordingSocket final : public SocketInterface
{
public:
	RecordingSocket(const Reference<SocketInterface> underlying_socket);
	virtual ~RecordingSocket();


	void writeRecordBufToDisk(const std::string& path);
	void clearRecordBuf();



	virtual void ungracefulShutdown() override;
	virtual void waitForGracefulDisconnect() override;
	virtual void startGracefulShutdown() override;

	// Read 1 or more bytes from the socket, up to a maximum of max_num_bytes.  Returns number of bytes read.
	// Returns zero if connection was closed gracefully
	size_t readSomeBytes(void* buffer, size_t max_num_bytes) override;

	virtual void setNoDelayEnabled(bool enabled) override; // NoDelay option is off by default.

	virtual void enableTCPKeepAlive(float period) override;

	virtual void setAddressReuseEnabled(bool enabled) override;

	virtual IPAddress getOtherEndIPAddress() const override { return underlying_socket->getOtherEndIPAddress(); }
	virtual int getOtherEndPort() const override { return underlying_socket->getOtherEndPort(); }

	virtual bool readable(double timeout_s) override;
	virtual bool readable(EventFD& event_fd) override; // Block until either the socket is readable or the event_fd is signalled (becomes readable).
	// Returns true if the socket was readable, false if the event_fd was signalled.

	
	
	void write(const void* data, size_t numbytes);
	void write(const void* data, size_t numbytes, FractionListener* frac);

	void readTo(void* buffer, size_t numbytes);
	void readTo(void* buffer, size_t numbytes, FractionListener* frac);


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
	Reference<SocketInterface> underlying_socket;

	std::vector<uint8> record_buf;
};


typedef Reference<RecordingSocket> RecordingSocketRef;
