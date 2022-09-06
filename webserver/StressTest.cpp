/*=====================================================================
StressTest.cpp
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "StressTest.h"


#include "TaskManager.h"
#include "Task.h"
#include "Timer.h"
#include "PlatformUtils.h"
#include "ConPrint.h"
#include "StringUtils.h"
#include "WebsiteExcep.h"
#include "TestUtils.h"
#include "MySocket.h"
#include "Parser.h"
#if TLS_SUPPORT
#include <TLSSocket.h>
#include <tls.h>
#endif


namespace web
{
namespace StressTest
{


static const std::string CRLF = "\r\n";
static const std::string CRLFCRLF = "\r\n\r\n";


void writeWebsocketTextMessage(const Reference<SocketInterface>& sock, const std::string& s)
{
	const uint32 fin = 0x80;
	const uint32 opcode = 0x1;

	// TODO: handle frames > 2^16 bytes.

	const size_t data_size = (s.size() <= 125) ? 2 + s.size() : 4 + s.size();
	std::vector<unsigned char> data(data_size);
	data[0] = fin | opcode;

	// Write payload len
	if(s.size() <= 125)
	{
		data[1] = (unsigned char)s.size();

		std::memcpy(&data[2], s.c_str(), s.size()); // Append s to data
	}
	else
	{
		data[1] = 126;
		data[2] = (unsigned char)(s.size() >> 8);
		data[3] = (unsigned char)(s.size() & 0xFF);

		std::memcpy(&data[4], s.c_str(), s.size()); // Append s to data
	}

	// Write data to the socket
	sock->writeData(&data[0], data_size);
}


const std::string readWebsocketTextMessage(const Reference<SocketInterface>& sock, const std::string& s)
{
	const unsigned char* msg = (const unsigned char*)&s[0];
				
	//bool got_header = false; // Have we read the full frame header?
							
	const uint32 fin = msg[0] & 0x80; // Fin bit. Indicates that this is the final fragment in a message.
	const uint32 opcode = msg[0] & 0xF; // Opcode.  4 bits
	const uint32 mask = msg[1] & 0x80; // Mask bit.  Defines whether the "Payload data" is masked.
	uint32 payload_len = msg[1] & 0x7F; // Payload length.  7 bits.
	uint32 header_size = mask != 0 ? 6 : 2; // If mask is present, it adds 4 bytes to the header size.
	uint32 cur_offset = 2;

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

	if(payload_len > 0)
	{
		std::vector<unsigned char> unmasked_payload(payload_len);
		for(uint32 i=0; i<payload_len; ++i)
		{
			//assert(request_start_index + header_size + i < socket_buffer.size());
			unmasked_payload[i] = msg[header_size + i] ^ masking_key[i % 4];
		}

		const std::string unmasked_payload_str((const char*)&unmasked_payload[0], (const char*)&unmasked_payload[0] + unmasked_payload.size());

		if(opcode == 0x1 && fin != 0) // Text frame:
		{ 
			return unmasked_payload_str;
		}
		else if(opcode == 0x8) // Close frame:
		{
			return "close";
		}
		else if(opcode == 0x9) // Ping
		{
			conPrint("PING");
			// TODO: Send back pong frame
		}
		else
		{
			conPrint("Got unknown websocket opcode: " + toString(opcode));
		}
	}

	return "Error";
}


class PageLoadTask : public glare::Task
{
public:
	PageLoadTask(int listen_port_, int N_) : listen_port(listen_port_), N(N_) {}

	virtual void run(size_t thread_index)
	{
		bool websocket = false;

		// Create TLS configuration
#if TLS_SUPPORT
		tls_config* client_tls_config = tls_config_new();

		tls_config_insecure_noverifycert(client_tls_config);
		tls_config_insecure_noverifyname(client_tls_config);

		//if(tls_config_set_cert_file(client_tls_config, "server.cert"/*"D:\\programming\\OpenSSL\\openssl-1.0.2a-x64-vs2012\\blarg.cert"*/) != 0)
		//	throw WebsiteExcep("tls_config_set_cert_file failed.");

#endif

		// tls_config_set_protocols(client_tls_config, TLS_PROTOCOL_TLSv1_2);


		for(int i=0; i<N; ++i)
		{
			try
			{
				if(i % 100 == 0)
					conPrint("PageLoadTask: doing query " + toString(i) + "...");


				const IPAddress ip_addr("127.0.0.1");
				MySocketRef plain_sock = new MySocket(ip_addr, listen_port);
				//MySocketRef sock = new MySocket("suprsede.com", 80, NULL);
				SocketInterfaceRef sock = plain_sock;
#if TLS_SUPPORT
				if(listen_port == 443)
				{
					TLSSocketRef tls_socket = new TLSSocket(plain_sock, client_tls_config, "localhost");
					sock = tls_socket;

					// Check the peer certificate hash
					const std::string hashstr = tls_peer_cert_hash(tls_socket->getTLSContext());
					conPrint(hashstr); // hashstr = "SHA256:b2f98cc23261c3c1f8380fd6c814e4e899ffb24714a2df2cc0aba2eefd38cb3b"
					if(hashstr != "SHA256:b2f98cc23261c3c1f8380fd6c814e4e899ffb24714a2df2cc0aba2eefd38cb3b")
						throw MySocketExcep("Server certificate had incorrect hash value.");
				}
#endif
				if(websocket)
				{
					const std::string query = "GET / HTTP/1.1" + CRLF + 
						"Sec-WebSocket-Key: bleh" + CRLF + 
						CRLF;
					sock->writeData(query.c_str(), query.size());
				}
				else
				{
					const std::string query = "GET /sdfds HTTP/1.1" + CRLFCRLF;
					sock->writeData(query.c_str(), query.size());
				}


				// Read data until we have the complete response header.
				char buf[100000];
				size_t total_num_bytes_read = 0;
				bool got_header = false;
				int header_len = 0;
				while(!got_header)
				{
					const size_t num_bytes_read = sock->readSomeBytes(buf + total_num_bytes_read, sizeof(buf) - total_num_bytes_read);
					if(num_bytes_read == 0)
						break;
					total_num_bytes_read += num_bytes_read;

					// Look for double CRLF
					for(int z=3; z<(int)total_num_bytes_read; ++z)
						if(buf[z-3] == '\r' && buf[z-2] == '\n' && buf[z-1] == '\r' && buf[z] == '\n') // If found CRLFCRLF
						{
							header_len = z + 1;
							got_header = true;
							break;
						}
				}

				//conPrint("Got header: (header_len: " + toString(header_len) + ")");
				//conPrint(std::string(buf, buf + header_len));


				// Parse header (and get content length)
				Parser parser(buf, header_len);
				parser.advancePastLine(); // Go past HTTP/1.1 200 OK\r\n
					
				string_view header_key;
				uint32 content_length = 0;
				while(1)
				{
					if(parser.currentIsChar('\r'))
					{
						parser.parseChar('\n');
						break;
					}

					parser.parseToChar(':', header_key);
					parser.advance(); // advance past ':'
					parser.parseWhiteSpace();
					if(header_key == "Content-Length")
						parser.parseUnsignedInt(content_length);

					parser.advancePastLine();

					if(parser.eof())
						throw MySocketExcep("EOF while parsing header.");
				}


				const int full_reponse_size = header_len + content_length;

				// Now get content
				const int remaining_data = (int)full_reponse_size - (int)total_num_bytes_read;

				if(remaining_data > 0)
					sock->readData(buf, remaining_data);
					

				//const std::string response_content(buf + header_len, buf + full_reponse_size);
				//conPrint("response_content:");
				//conPrint(response_content);

				// Wait for a while
				//conPrint("Waiting...");
				//PlatformUtils::Sleep(100000);
				//conPrint("Done.");

				if(websocket)
				{
					// Do some sending of websocket frames.
					for(int z=0; z<10; ++z)
					{
						const int num_msgs_at_once = 4;
						for(int q=0; q<num_msgs_at_once; ++q)
							writeWebsocketTextMessage(sock, "test_ping");
						std::vector<char> buf2(11*num_msgs_at_once);
						/*const size_t num_read = sock->readSomeBytes(&buf2[0], buf2.size());
						if(num_read == 0)
							break;*/
						//conPrint("Reading response...");
						sock->readData(&buf2[0], buf2.size());
						const std::string response = std::string(&buf2[0], &buf2[0] + buf2.size());

					
						const std::string text_msg = readWebsocketTextMessage(sock, response);

						//conPrint("text_msg: '" + text_msg + "'.");
						testAssert(text_msg == "test_pong");
					}
				}

			}
			catch(MySocketExcep& e)
			{
				conPrint("MySocketExcep: " + e.what());
			}
		}
	}

	int listen_port;
	int N;
};


void testPageLoadsWithNThreads(int listen_port, int num_threads)
{
	
	const int N = 200;
	conPrint("Running stress test, num queries: " + toString(N) + ", num threads: " + toString(num_threads));

	glare::TaskManager task_manager(num_threads);
	// Try with 1 thread
	Timer timer;

	std::vector<Reference<PageLoadTask> > tasks(num_threads);
	for(int i=0; i<num_threads; ++i)
	{
		tasks[i] = new PageLoadTask(listen_port, N / num_threads);
		task_manager.addTask(tasks[i]);
	}

	task_manager.waitForTasksToComplete();

	const double period = timer.elapsed() / N;
	const double freq = 1 / period;
	conPrint("--------------------------------");
	conPrint("num queries: " + toString(N));
	conPrint("num threads: " + toString(num_threads));
	conPrint("time / query: " + toString(period) + " s (" + toString((float)freq) + " queries/s)");
}


void test(int listen_port)
{
	try
	{
		const int num_threads = 8;
		testPageLoadsWithNThreads(listen_port, num_threads);
	}
	catch(WebsiteExcep& e)
	{
		failTest(e.what());
	}
}

// Relwtihdebinfo
// time / query: 0.00020211800853885505 s (4947.6045 queries/s)
// 4910.855 queries/s
// 5003.9883 queries/s

} // end namespace StressTest
} // end namespace web


// MySocketExcep: write failed: handshake failed: error:140770FC:SSL routines:SSL23_GET_SERVER_HELLO:unknown protocol


/*
With TLS: (23.059916 queries/s)
(22.97711 queries/


Plain HTTP: 7980.2744 queries/s


ECDHE-RSA-CHACHA20-POLY1305

*/