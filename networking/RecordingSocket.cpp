/*=====================================================================
RecordingSocket.cpp
-------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "RecordingSocket.h"


#include "../utils/FileUtils.h"
#include "../utils/ConPrint.h"


RecordingSocket::RecordingSocket(Reference<SocketInterface> underlying_socket_)
{
	underlying_socket = underlying_socket_;
}


RecordingSocket::~RecordingSocket()
{
}


void RecordingSocket::writeRecordBufToDisk(const std::string& path)
{
	conPrint("Writing trace to disk at '" + path + "'...");

	FileUtils::writeEntireFile(path, record_buf);
}


void RecordingSocket::clearRecordBuf()
{
	record_buf.clear();
}


void RecordingSocket::write(const void* data, size_t datalen)
{
	write(data, datalen, NULL);
}


void RecordingSocket::write(const void* data, size_t datalen, FractionListener* frac)
{
	underlying_socket->writeData(data, datalen);
}


size_t RecordingSocket::readSomeBytes(void* buffer, size_t max_num_bytes)
{
	const size_t num = underlying_socket->readSomeBytes(buffer, max_num_bytes);

	// Copy read data to record buf
	if(num > 0)
	{
		const size_t sz = record_buf.size();
		record_buf.resize(sz + num);
		std::memcpy(&record_buf[sz], buffer, num);
	}
	return num;
}


void RecordingSocket::setNoDelayEnabled(bool enabled) // NoDelay option is off by default.
{
	underlying_socket->setNoDelayEnabled(enabled);
}


void RecordingSocket::enableTCPKeepAlive(float period)
{
	underlying_socket->enableTCPKeepAlive(period);
}


void RecordingSocket::setAddressReuseEnabled(bool enabled)
{
	underlying_socket->setAddressReuseEnabled(enabled);
}


void RecordingSocket::readTo(void* buffer, size_t readlen)
{
	readTo(buffer, readlen, NULL);
}


void RecordingSocket::readTo(void* buffer, size_t readlen, FractionListener* frac)
{
	underlying_socket->readData(buffer, readlen);

	// Copy read data to record buf
	if(readlen > 0)
	{
		const size_t sz = record_buf.size();
		record_buf.resize(sz + readlen);
		std::memcpy(&record_buf[sz], buffer, readlen);
	}
}


void RecordingSocket::ungracefulShutdown()
{
	underlying_socket->ungracefulShutdown();
}


void RecordingSocket::waitForGracefulDisconnect()
{
	underlying_socket->waitForGracefulDisconnect();
}


void RecordingSocket::startGracefulShutdown()
{
	underlying_socket->startGracefulShutdown();
}


void RecordingSocket::writeInt32(int32 x)
{
	underlying_socket->writeInt32(x);
}


void RecordingSocket::writeUInt32(uint32 x)
{
	underlying_socket->writeUInt32(x);
}


int RecordingSocket::readInt32()
{
	const int x = underlying_socket->readInt32();

	// Copy read data to record buf
	const size_t sz = record_buf.size();
	record_buf.resize(sz + sizeof(x));
	std::memcpy(&record_buf[sz], &x, sizeof(x));

	return x;
}


uint32 RecordingSocket::readUInt32()
{
	const uint32 x = underlying_socket->readUInt32();

	// Copy read data to record buf
	const size_t sz = record_buf.size();
	record_buf.resize(sz + sizeof(x));
	std::memcpy(&record_buf[sz], &x, sizeof(x));

	return x;
}


bool RecordingSocket::readable(double timeout_s)
{
	return underlying_socket->readable(timeout_s);
}


// Block until either the socket is readable or the event_fd is signalled (becomes readable). 
// Returns true if the socket was readable, false if the event_fd was signalled.
bool RecordingSocket::readable(EventFD& event_fd)
{	
	return underlying_socket->readable(event_fd);
}


void RecordingSocket::readData(void* buf, size_t num_bytes)
{
	readTo(buf, num_bytes, 
		NULL // fraction listener
	);
}


bool RecordingSocket::endOfStream()
{
	return underlying_socket->endOfStream();
}


void RecordingSocket::writeData(const void* data, size_t num_bytes)
{
	write(data, num_bytes, 
		NULL // fraction listener
	);
}
