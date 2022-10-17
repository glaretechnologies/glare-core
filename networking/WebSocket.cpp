/*=====================================================================
WebSocket.cpp
-------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "WebSocket.h"


#include "MySocket.h"
#include "../maths/mathstypes.h"
#include "../utils/StringUtils.h"
#include "../utils/MyThread.h"
#include "../utils/PlatformUtils.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"
#include <string.h>


// Kinda like assert(), but checks when NDEBUG enabled, and throws glare::Exception on failure.
static void _doRuntimeCheck(bool b, const char* message)
{
	assert(b);
	if(!b)
		throw glare::Exception(std::string(message));
}

#define doRuntimeCheck(v) _doRuntimeCheck((v), (#v))


WebSocket::WebSocket(SocketInterfaceRef underlying_socket_)
{
	underlying_socket = underlying_socket_;

	need_header_read = true;
}


WebSocket::~WebSocket()
{
}


void WebSocket::write(const void* data, size_t datalen)
{
	// Append data to our output buffer.  Output data will be written to the underlying socket in the flush() method.
	buffer_out.writeData(data, datalen);
}


void WebSocket::write(const void* data, size_t datalen, FractionListener* frac)
{
	// Append data to our output buffer.  Output data will be written to the underlying socket in the flush() method.
	buffer_out.writeData(data, datalen);
}


size_t WebSocket::readSomeBytes(void* buffer, size_t max_num_bytes)
{
	assert(0);
	throw MySocketExcep("WebSocket::readSomeBytes not implemented");
}


void WebSocket::setNoDelayEnabled(bool enabled) // NoDelay option is off by default.
{
	underlying_socket->setNoDelayEnabled(enabled);
}


void WebSocket::enableTCPKeepAlive(float period)
{
	underlying_socket->enableTCPKeepAlive(period);
}


void WebSocket::setAddressReuseEnabled(bool enabled)
{
	underlying_socket->setAddressReuseEnabled(enabled);
}


void WebSocket::readTo(void* buffer, size_t readlen)
{
	readTo(buffer, readlen, NULL);
}


/*
Lets say the first message is

| header |          payload                   |
0        2                                    32

If we have a readlen of 10:

| header |          payload                   |
0        2          12                         32
         |----------|
		  read data  

In this case we want to store the remaining payload size as initial payload_len - 10.

if readlen is greater than the payload length, then we want to keep reading frames until we have read readlen bytes, for example:


| header |          payload                  | header |          payload                  |
0        2          12                       34
         |-----------------------------------|  ....  |-----------------------------------|
              buf read                                       buf read (continued)

*/
void WebSocket::readTo(void* buffer, size_t readlen, FractionListener* frac)
{
	size_t amount_still_to_read = readlen;
	while(amount_still_to_read > 0)
	{
		if(need_header_read)
		{
			// Read first 2 bytes of header
			temp_buffer.resize(2);
			underlying_socket->readData(temp_buffer.data(), 2);

			this->header_opcode = temp_buffer[0] & 0xF; // Opcode.  4 bits
			const uint32 mask = temp_buffer[1] & 0x80; // Mask bit.  Defines whether the "Payload data" is masked.
			this->payload_len = temp_buffer[1] & 0x7F; // Payload length.  7 bits.
			size_t header_size = mask != 0 ? 6 : 2; // If mask is present, it adds 4 bytes to the header size.  Header size may be further increased below.

			if(payload_len <= 125) // "if 0-125, that is the payload length"
			{
				// Read rest of header (will have extra data when mask is present)
				if(header_size > 2)
				{
					temp_buffer.resize(header_size);
					underlying_socket->readData(temp_buffer.data() + 2, header_size - 2);
				}
			}
			else if(payload_len == 126) // "If 126, the following 2 bytes interpreted as a 16-bit unsigned integer are the payload length" - https://tools.ietf.org/html/rfc6455
			{
				// Read rest of header
				header_size += 2;
				temp_buffer.resize(header_size);
				underlying_socket->readData(temp_buffer.data() + 2, header_size - 2);

				doRuntimeCheck(temp_buffer.size() >= 4);
				payload_len = (temp_buffer[2] << 8) | temp_buffer[3];
			}
			else // "If 127, the following 8 bytes interpreted as a 64-bit unsigned integer (the most significant bit MUST be 0) are the payload length"
			{
				// Read rest of header
				header_size += 8;
				temp_buffer.resize(header_size);
				underlying_socket->readData(temp_buffer.data() + 2, header_size - 2);

				doRuntimeCheck(temp_buffer.size() >= 10);

				payload_len = 0;
				for(int i = 0; i < 8; ++i)
					payload_len |= (uint64)temp_buffer[2 + i] << (8 * (7 - i));
			}

			// Read masking key
			if(mask != 0)
			{
				doRuntimeCheck(temp_buffer.size() == header_size);
				doRuntimeCheck(header_size >= 4);
				const size_t mask_offset = header_size - 4;

				masking_key[0] = temp_buffer[mask_offset + 0];
				masking_key[1] = temp_buffer[mask_offset + 1];
				masking_key[2] = temp_buffer[mask_offset + 2];
				masking_key[3] = temp_buffer[mask_offset + 3];
			}
			else
				masking_key[0] = masking_key[1] = masking_key[2] = masking_key[3] = 0;

			this->payload_remaining = payload_len;

			need_header_read = false;
		}
		else
		{
			assert(!need_header_read);

			// TODO: handle continuation frames, which have an opcode of zero.

			if(header_opcode == 0x1 || header_opcode == 0x2) // Text or binary frame:
			{
				// The calling code still desires amount_still_to_read bytes of data.
				// Read that much, or the remaining payload size, whichever is less.
				const size_t payload_len_to_read = myMin(payload_remaining, amount_still_to_read);

				// Read payload_len_to_read bytes from the underlying sock, unmask, and write to buffer.
				// Do this in chunks of max length 2048 bytes.
				size_t offset = 0;
				while(offset < payload_len_to_read)
				{
					const size_t chunk_size = myMin(2048ull, payload_len_to_read - offset);
					temp_buffer.resizeNoCopy(chunk_size);
					underlying_socket->readData(temp_buffer.data(), chunk_size);

					for(size_t z=0; z<chunk_size; ++z)
						((uint8*)buffer)[offset + z] = temp_buffer[z] ^ masking_key[z % 4]; // Copy unmasked data from temp_buffer to the out-buffer.

					offset += chunk_size;
				}

				payload_remaining -= payload_len_to_read;
				buffer = (void*)(((uint8*)buffer) + payload_len_to_read); // Advance buffer pointer.
				amount_still_to_read -= payload_len_to_read;

				if(payload_remaining == 0)
					need_header_read = true;
			}
			else if(header_opcode == 0x8) // Close frame:
			{
				// "If an endpoint receives a Close frame and did not previously send a Close frame, the endpoint MUST send a Close frame in response."
				writeDataInFrame(/*opcode=*/0x8, /*data=*/NULL, /*datalen=*/0);

				throw MySocketExcep("Connection Closed.", MySocketExcep::ExcepType_ConnectionClosedGracefully);
			}
			else if(header_opcode == 0x9) // Ping
			{
				if(payload_len > 2048)
					throw MySocketExcep("Ping payload too long");

				// "Upon receipt of a Ping frame, an endpoint MUST send a Pong frame in response, unless it already received a Close frame"
				temp_buffer.resize(payload_len);
				underlying_socket->readData(temp_buffer.data(), payload_len);

				// Send Pong frame back.
				// "A Pong frame sent in response to a Ping frame must have identical "Application data" as found in the message body of the Ping frame being replied to."
				writeDataInFrame(/*opcode (PONG)=*/0xA, temp_buffer.data(), payload_len);

				need_header_read = true;
			}
			else
			{
				throw MySocketExcep("Got unknown websocket opcode: " + toString(header_opcode));
			}
		}
	}
}


void WebSocket::ungracefulShutdown()
{
	underlying_socket->ungracefulShutdown();
}


void WebSocket::waitForGracefulDisconnect()
{
	underlying_socket->waitForGracefulDisconnect();
}


void WebSocket::startGracefulShutdown()
{
	underlying_socket->startGracefulShutdown();
}


void WebSocket::writeInt32(int32 x)
{
	write(&x, sizeof(int32));
}


void WebSocket::writeUInt32(uint32 x)
{
	write(&x, sizeof(uint32));
}


int WebSocket::readInt32()
{
	int32 i;
	readTo(&i, sizeof(int32));

	return i;
}


uint32 WebSocket::readUInt32()
{
	uint32 x;
	readTo(&x, sizeof(uint32));

	return x;
}


bool WebSocket::readable(double timeout_s)
{
	return underlying_socket->readable(timeout_s);
}


// Block until either the socket is readable or the event_fd is signalled (becomes readable). 
// Returns true if the socket was readable, false if the event_fd was signalled.
bool WebSocket::readable(EventFD& event_fd)
{	
	return underlying_socket->readable(event_fd);
}


void WebSocket::readData(void* buf, size_t num_bytes)
{
	readTo(buf, num_bytes, 
		NULL // fraction listener
	);
}


bool WebSocket::endOfStream()
{
	return false;
}


void WebSocket::writeData(const void* data, size_t num_bytes)
{
	write(data, num_bytes, 
		NULL // fraction listener
	);
}


// Needs to support datalen = 0 for sending close opcodes etc.
void WebSocket::writeDataInFrame(uint8 opcode, const uint8* data, size_t datalen)
{
	uint8 frame_prefix[10];
	frame_prefix[0] = /*fin=*/0x80 | opcode;

	// Work out payload len and write header to the socket.
	if(datalen <= 125)
	{
		frame_prefix[1] = (uint8)datalen;

		underlying_socket->writeData(frame_prefix, 2);
	}
	else if(datalen <= 65536)
	{
		frame_prefix[1] = 126;
		frame_prefix[2] = (uint8)(datalen >> 8);
		frame_prefix[3] = (uint8)(datalen & 0xFF);

		underlying_socket->writeData(frame_prefix, 4);
	}
	else
	{
		frame_prefix[1] = 127;
		for(int i=0; i<8; ++i)
			frame_prefix[2 + i] = (uint8)((datalen >> (8*(7 - i))) & 0xFF);

		underlying_socket->writeData(frame_prefix, 10);
	}

	// Write payload data to the socket
	if(datalen > 0)
		underlying_socket->writeData(data, datalen);
}


// Write all unflushed data written to this socket to the underlying socket.
void WebSocket::flush()
{
	if(buffer_out.buf.size() > 0)
	{
		writeDataInFrame(/*opcode (binary frame)=*/0x2, buffer_out.buf.data(), buffer_out.buf.size());

		buffer_out.buf.resize(0);
	}
}
