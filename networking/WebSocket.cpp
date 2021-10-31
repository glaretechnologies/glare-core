/*=====================================================================
WebSocket.cpp
-------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "WebSocket.h"


#include "MySocket.h"
#include "../maths/mathstypes.h"
#include "../utils/StringUtils.h"
#include "../utils/MyThread.h"
#include "../utils/PlatformUtils.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"
#include <vector>
#include <string.h>
#include <algorithm>


WebSocket::WebSocket(SocketInterfaceRef underlying_socket_)
{
	underlying_socket = underlying_socket_;
	frame_start_index = 0;
	read_header = false;
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


// Move bytes [request_start, socket_buffer.size()) to [0, socket_buffer.size() - request_start),
// then resize buffer to socket_buffer.size() - request_start.
static void moveToFrontOfBuffer(js::Vector<uint8, 16>& socket_buffer, size_t request_start)
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
	// While we don't have enough data in the input buffer to complete the read from the local input buffer:
	while(readlen > buffer_in.buf.size())
	{
		// Read some data from the socket into socket_buffer.
		const size_t old_socket_buffer_size = socket_buffer.size();
		const size_t read_chunk_size = 2048;
		socket_buffer.resize(old_socket_buffer_size + read_chunk_size);
		const size_t num_bytes_read = underlying_socket->readSomeBytes(&socket_buffer[old_socket_buffer_size], read_chunk_size);
		if(num_bytes_read == 0) // if connection was closed gracefully
			throw MySocketExcep("Connection Closed.", MySocketExcep::ExcepType_ConnectionClosedGracefully);

		socket_buffer.resize(old_socket_buffer_size + num_bytes_read); // Trim the buffer down so it only extends to what we actually read.

		// Process data from the socket read.  If we processed a full frame, keep looping to process any subsequent frames in socket_buffer.
		bool processed_full_frame = true;
		while(processed_full_frame)
		{
			processed_full_frame = false;

			if(!read_header) // If we have not read a complete frame header:
			{
				const size_t socket_buf_size = socket_buffer.size();
				if(frame_start_index + 2 <= socket_buf_size) // If there are >= 2 bytes in the buffer:
				{
					const uint8* msg = &socket_buffer[frame_start_index];

					this->header_opcode = msg[0] & 0xF; // Opcode.  4 bits
					const uint32 mask = msg[1] & 0x80; // Mask bit.  Defines whether the "Payload data" is masked.
					this->payload_len = msg[1] & 0x7F; // Payload length.  7 bits.
					this->header_size = mask != 0 ? 6 : 2; // If mask is present, it adds 4 bytes to the header size.
					size_t cur_offset = 2; // Offset in header

					bool read_header_size = false;
					if(payload_len <= 125) // "if 0-125, that is the payload length"
					{
						read_header_size = true;
					}
					else if(payload_len == 126) // "If 126, the following 2 bytes interpreted as a 16-bit unsigned integer are the payload length" - https://tools.ietf.org/html/rfc6455
					{
						if(frame_start_index + 4 <= socket_buf_size) // If there are 4 bytes in the buffer:
						{
							payload_len = (msg[2] << 8) | msg[3];
							header_size += 2;
							cur_offset += 2;
							read_header_size = true;
						}
					}
					else // "If 127, the following 8 bytes interpreted as a 64-bit unsigned integer (the most significant bit MUST be 0) are the payload length"
					{
						assert(payload_len == 127);
						if(frame_start_index + 10 <= socket_buf_size) // If there are 10 bytes in the buffer:
						{
							payload_len = 0;
							for(int i=0; i<8; ++i)
								payload_len |= (uint64)msg[2 + i] << (8*(7 - i));

							header_size += 8;
							cur_offset += 8;
							read_header_size = true;
						}
					}

					if(read_header_size && (frame_start_index + header_size <= socket_buf_size))
					{
						// Read masking key
						if(mask != 0)
						{
							masking_key[0] = msg[cur_offset++];
							masking_key[1] = msg[cur_offset++];
							masking_key[2] = msg[cur_offset++];
							masking_key[3] = msg[cur_offset++];
						}
						else
							masking_key[0] = masking_key[1] = masking_key[2] = masking_key[3] = 0;

						read_header = true;
					}
				}
			}


			if(read_header) // If we have read a complete frame header:
			{
				// See if we have already read the full frame body:
				if(frame_start_index + header_size + payload_len <= socket_buffer.size())
				{
					// TODO: handle continuation frames, which have an opcode of zero.

					if(header_opcode == 0x1 || header_opcode == 0x2) // Text or binary frame:
					{ 
						// Append to the back of buffer_in
						const size_t write_i = buffer_in.buf.size();
						buffer_in.buf.resize(write_i + payload_len);

						for(size_t i=0; i<payload_len; ++i)
						{
							assert(frame_start_index + header_size + i < socket_buffer.size());
							buffer_in.buf[write_i + i] = socket_buffer[frame_start_index + header_size + i] ^ masking_key[i % 4];
						}
					}
					else if(header_opcode == 0x8) // Close frame:
					{
						// "If an endpoint receives a Close frame and did not previously send a Close frame, the endpoint MUST send a Close frame in response."
						writeDataInFrame(/*opcode=*/0x8, /*data=*/NULL, /*datalen=*/0);

						throw MySocketExcep("Connection Closed.", MySocketExcep::ExcepType_ConnectionClosedGracefully);
					}
					else if(header_opcode == 0x9) // Ping
					{
						// "Upon receipt of a Ping frame, an endpoint MUST send a Pong frame in response, unless it already received a Close frame"
						
						// Send Pong frame back.
						// "A Pong frame sent in response to a Ping frame must have identical "Application data" as found in the message body of the Ping frame being replied to."
						writeDataInFrame(/*opcode (PONG)=*/0xA, &socket_buffer[frame_start_index + header_size], payload_len);
					}
					else
					{
						throw MySocketExcep("Got unknown websocket opcode: " + toString(header_opcode));
					}

					// We have finished processing the websocket frame.  Advance frame_start_index
					frame_start_index = frame_start_index + header_size + payload_len;

					// Move the remaining data in the buffer to the start of the buffer, and resize the buffer down.
					// We do this so as to not exhaust memory when a connection is open for a long time and receiving lots of frames.
					if((frame_start_index >= 4096) && (frame_start_index < socket_buffer.size()))
					{
						moveToFrontOfBuffer(socket_buffer, frame_start_index);
						frame_start_index = 0;
					}

					if(frame_start_index == socket_buffer.size()) // If we have processed the complete socket_buffer:
					{
						socket_buffer.clear();
						frame_start_index = 0;
					}


					// Reset our state to read the next frame header
					read_header = false;
					header_size = 0;
					payload_len = 0;
					header_opcode = 0;

					processed_full_frame = true; // Keep looping

				} // End if(have read full frame)
			} // End if(have read header)
		}
	}

	
	buffer_in.readData(buffer, readlen); // Read from the internal buffer.
	if(buffer_in.endOfStream()) // If we read all the data in the internal buffer, clear it.
		buffer_in.clear();
}


void WebSocket::ungracefulShutdown()
{
	underlying_socket->ungracefulShutdown();
}


void WebSocket::waitForGracefulDisconnect()
{
	underlying_socket->waitForGracefulDisconnect();
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
	return underlying_socket->readable(timeout_s);
}


// Block until either the socket is readable or the event_fd is signalled (becomes readable). 
// Returns true if the socket was readable, false if the event_fd was signalled.
bool WebSocket::readable(EventFD& event_fd)
{	
	if(!buffer_in.endOfStream())
		return true;
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


void WebSocket::writeDataInFrame(uint8 opcode, const uint8* data, size_t datalen)
{
	uint8 frame_prefix[10];
	frame_prefix[0] = /*fin=*/0x80 | opcode;

	// Write payload len
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

	// Write data to the socket
	if(datalen > 0)
		underlying_socket->writeData(data, datalen);
}


// Write all unflushed data written to this socket to the underlying socket.
void WebSocket::flush()
{
	writeDataInFrame(/*opcode (binary frame)=*/0x2, buffer_out.buf.data(), buffer_out.buf.size());

	buffer_out.buf.resize(0);
}
