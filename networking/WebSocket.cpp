/*=====================================================================
WebSocket.cpp
-------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "WebSocket.h"


#include "Networking.h"
#include "MySocket.h"
#include "FractionListener.h"
#include "../maths/mathstypes.h"
#include "../utils/StringUtils.h"
#include "../utils/MyThread.h"
#include "../utils/PlatformUtils.h"
#include "../utils/Timer.h"
#include "../utils/EventFD.h"
#include "../utils/ConPrint.h"
#include "../utils/BitUtils.h"
#include "../utils/OpenSSL.h"
#include <vector>
#include <string.h>
#include <algorithm>



WebSocket::WebSocket(SocketInterfaceRef plain_socket_)
{
	plain_socket = plain_socket_;
	request_start_index = 0;
}


WebSocket::~WebSocket()
{
}

void WebSocket::write(const void* data, size_t datalen)
{
	buffer_out.writeData(data, datalen);
}


void WebSocket::write(const void* data, size_t datalen, FractionListener* frac)
{
	buffer_out.writeData(data, datalen);
}


size_t WebSocket::readSomeBytes(void* buffer, size_t max_num_bytes)
{
	assert(0);
	throw MySocketExcep("WebSocket::readSomeBytes not implemented");
}


void WebSocket::setNoDelayEnabled(bool enabled) // NoDelay option is off by default.
{
	plain_socket->setNoDelayEnabled(enabled);
}


void WebSocket::enableTCPKeepAlive(float period)
{
	plain_socket->enableTCPKeepAlive(period);
}


void WebSocket::setAddressReuseEnabled(bool enabled)
{
	plain_socket->setAddressReuseEnabled(enabled);
}


void WebSocket::readTo(void* buffer, size_t readlen)
{
	readTo(buffer, readlen, NULL);
}


// Move bytes [request_start, socket_buffer.size()) to [0, socket_buffer.size() - request_start),
// then resize buffer to socket_buffer.size() - request_start.
static void moveToFrontOfBuffer(std::vector<uint8>& socket_buffer, size_t request_start)
{
	assert(request_start < socket_buffer.size());
	if(request_start < socket_buffer.size())
	{
		const size_t len = socket_buffer.size() - request_start; // num bytes to copy
		for(size_t i=0; i<len; ++i)
			socket_buffer[i] = socket_buffer[request_start + i];

		socket_buffer.resize(len);
	}
}


void WebSocket::readTo(void* buffer, size_t readlen, FractionListener* frac)
{
	// Read from buffered frame data.  If we have read all of that, start reading a frame from the underlying socket.
	while(readlen > buffer_in.buf.size())
	{
		const size_t old_socket_buffer_size = socket_buffer.size();
		const size_t read_chunk_size = 2048;
		socket_buffer.resize(old_socket_buffer_size + read_chunk_size);
		const size_t num_bytes_read = plain_socket->readSomeBytes(&socket_buffer[old_socket_buffer_size], read_chunk_size);
		if(num_bytes_read == 0) // if connection was closed gracefully
			throw MySocketExcep("Connection Closed.", MySocketExcep::ExcepType_ConnectionClosedGracefully);

		socket_buffer.resize(old_socket_buffer_size + num_bytes_read); // Trim the buffer down so it only extends to what we actually read.


		// Process any complete frames we have in socket_buffer.
		// Initial frame header is 2 bytes
		bool keep_processing_frames = true;
		while(keep_processing_frames)
		{
			//if(VERBOSE) conPrint("WorkerThread: Processing frame...");

			keep_processing_frames = false; // Break the loop, unless we process a full frame below

			const size_t socket_buf_size = socket_buffer.size();
			if(request_start_index + 2 <= socket_buf_size) // If there are 2 bytes in the buffer:
			{
				const uint8* msg = &socket_buffer[request_start_index];

				bool got_header = false; // Have we read the full frame header?

				const uint32 fin = msg[0] & 0x80; // Fin bit. Indicates that this is the final fragment in a message.
				const uint32 opcode = msg[0] & 0xF; // Opcode.  4 bits
				const uint32 mask = msg[1] & 0x80; // Mask bit.  Defines whether the "Payload data" is masked.
				uint32 payload_len = msg[1] & 0x7F; // Payload length.  7 bits.
				uint32 header_size = mask != 0 ? 6 : 2; // If mask is present, it adds 4 bytes to the header size.
				uint32 cur_offset = 2;

				
				if(payload_len <= 125) // "if 0-125, that is the payload length"
				{
					got_header = true;
				}
				else if(payload_len == 126) // "If 126, the following 2 bytes interpreted as a 16-bit unsigned integer are the payload length" - https://tools.ietf.org/html/rfc6455
				{
					if(request_start_index + 4 <= socket_buf_size) // If there are 4 bytes in the buffer:
					{
						payload_len = (msg[2] << 8) | msg[3];
						header_size += 2;
						cur_offset += 2;
						got_header = true;
					}
				}
				else // "If 127, the following 8 bytes interpreted as a 64-bit unsigned integer (the most significant bit MUST be 0) are the payload length"
				{
					assert(payload_len == 127);
					if(request_start_index + 10 <= socket_buf_size) // If there are 10 bytes in the buffer:
					{
						uint64 len64 = 0;
						for(int i=0; i<8; ++i)
							len64 |= (uint64)msg[2 + i] << (8*(7 - i));

						if(len64 > (uint64)std::numeric_limits<uint32>::max()) // We won't handle messages with length >= than 2^32 bytes for now.
							throw MySocketExcep("Message too long");
						payload_len = (uint32)len64;
						header_size += 8;
						cur_offset += 8;
						got_header = true;
					}
				}

				if(got_header)
				{
					// See if we have already read the full frame body:
					if(request_start_index + header_size + payload_len <= socket_buf_size)
					{
						// Read masking key
						unsigned char masking_key[4];
						if(mask != 0)
						{
							masking_key[0] = msg[cur_offset++];
							masking_key[1] = msg[cur_offset++];
							masking_key[2] = msg[cur_offset++];
							masking_key[3] = msg[cur_offset++];
						}
						else
							masking_key[0] = masking_key[1] = masking_key[2] = masking_key[3] = 0;

						// We should have read the entire header (including masking key) now.
						assert(cur_offset == header_size);

						if(payload_len > 0)
						{
							if(opcode == 0x1 && fin != 0) // Text frame:
							{ 
								// Append to the back of buffer_in
								const size_t write_i = buffer_in.buf.size();
								buffer_in.buf.resize(write_i + payload_len);

								for(uint32 i=0; i<payload_len; ++i)
								{
									assert(request_start_index + header_size + i < socket_buffer.size());
									buffer_in.buf[write_i + i] = msg[header_size + i] ^ masking_key[i % 4];
								}
							}
							if(opcode == 0x2 && fin != 0) // Binary frame:
							{ 
								// Append to the back of buffer_in
								const size_t write_i = buffer_in.buf.size();
								buffer_in.buf.resize(write_i + payload_len);

								for(uint32 i=0; i<payload_len; ++i)
								{
									assert(request_start_index + header_size + i < socket_buffer.size());
									buffer_in.buf[write_i + i] = msg[header_size + i] ^ masking_key[i % 4];
								}
							}
							else if(opcode == 0x8) // Close frame:
							{
								throw MySocketExcep("Connection Closed.", MySocketExcep::ExcepType_ConnectionClosedGracefully);
							}
							else if(opcode == 0x9) // Ping
							{
								//conPrint("PING");
								// TODO: Send back pong frame
								//ResponseUtils::writeWebsocketTextMessage(reply_info, unmasked_payload_str);
							}
							else
							{
								//conPrint("Got unknown websocket opcode: " + toString(opcode));
								throw MySocketExcep("Got unknown websocket opcode: " + toString(opcode));
							}
						}

						// We have finished processing the websocket frame.  Advance request_start_index
						request_start_index = request_start_index + header_size + payload_len;
						keep_processing_frames = true;

						// Move the remaining data in the buffer to the start of the buffer, and resize the buffer down.
						// We do this so as to not exhaust memory when a connection is open for a long time and receiving lots of frames.
						if(request_start_index < socket_buf_size)
						{
							moveToFrontOfBuffer(socket_buffer, request_start_index);
							request_start_index = 0;
						}
					}
				}
			}
		}
	}

	
	buffer_in.readData(buffer, readlen);
	if(buffer_in.endOfStream())
	{
		// Clear buffer_in
		buffer_in.buf.resize(0);
		buffer_in.read_index = 0;
	}
}


void WebSocket::ungracefulShutdown()
{
	plain_socket->ungracefulShutdown();
}


void WebSocket::waitForGracefulDisconnect()
{
	plain_socket->waitForGracefulDisconnect();
}


void WebSocket::writeInt32(int32 x)
{
	//if(plain_socket->getUseNetworkByteOrder())
	//	x = bitCast<int32>(htonl(bitCast<uint32>(x)));

	write(&x, sizeof(int32));
}


void WebSocket::writeUInt32(uint32 x)
{
	//if(plain_socket->getUseNetworkByteOrder())
	//	x = htonl(x); // Convert to network byte ordering.

	write(&x, sizeof(uint32));
}


int WebSocket::readInt32()
{
	int32 i;
	readTo(&i, sizeof(int32));

	//if(plain_socket->getUseNetworkByteOrder())
	//	i = bitCast<int32>(ntohl(bitCast<uint32>(i)));

	return i;
}


uint32 WebSocket::readUInt32()
{
	uint32 x;
	readTo(&x, sizeof(uint32));

	//if(plain_socket->getUseNetworkByteOrder())
	//	x = ntohl(x);

	return x;
}


bool WebSocket::readable(double timeout_s)
{
	if(!buffer_in.endOfStream())
		return true;
	return plain_socket->readable(timeout_s);
}


// Block until either the socket is readable or the event_fd is signalled (becomes readable). 
// Returns true if the socket was readable, false if the event_fd was signalled.
bool WebSocket::readable(EventFD& event_fd)
{	
	if(!buffer_in.endOfStream())
		return true;
	return plain_socket->readable(event_fd);
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


// Write all unflushed data written to this socket to the underlying socket.
void WebSocket::flush()
{
	const uint32 fin = 0x80;
	const uint32 opcode = 0x2;

	uint8 frame_prefix[10];
	frame_prefix[0] = fin | opcode;

	const size_t buf_size = buffer_out.buf.size();
	// Write payload len
	if(buf_size <= 125)
	{
		frame_prefix[1] = (uint8)buffer_out.buf.size();

		plain_socket->writeData(frame_prefix, 2);
	}
	else if(buf_size <= 65536)
	{
		frame_prefix[1] = 126;
		frame_prefix[2] = (uint8)(buf_size >> 8);
		frame_prefix[3] = (uint8)(buf_size & 0xFF);

		plain_socket->writeData(frame_prefix, 4);
	}
	else
	{
		// TODO: check this
		frame_prefix[1] = 127;
		for(int i=0; i<8; ++i)
			frame_prefix[2 + i] = (uint8)((buf_size >> (8*(7 - i))) & 0xFF);

		plain_socket->writeData(frame_prefix, 10);
	}

	// Write data to the socket
	plain_socket->writeData(buffer_out.buf.data(), buffer_out.buf.size());

	buffer_out.buf.resize(0);
}
