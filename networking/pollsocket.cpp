/*=====================================================================
pollsocket.cpp
--------------
File created by ClassTemplate on Sun Sep 05 21:45:54 2004
Code By Nicholas Chapman.

Code Copyright Nicholas Chapman 2005.

=====================================================================*/
#include "pollsocket.h"


#include <assert.h>
#include "networking.h"
#include "../maths/mathstypes.h"
//#include <winsock2.h>


PollSocket::PollSocket()
{
	//thisend_port = -1;
	otherend_port = -1;
	state = CLOSED;
	sockethandle = NULL;
	worker_socket = NULL;

	assert(Networking::isInited());
		
	if(!Networking::isInited())
		throw PollSocketExcep("Networking not inited or destroyed.");


}

PollSocket::PollSocket(const std::string hostname, int port)
{
	
	state = CLOSED;
	sockethandle = NULL;

	assert(Networking::isInited());
		
	assert(Networking::isInited());
		
	if(!Networking::isInited())
		throw PollSocketExcep("Networking not inited or destroyed.");




	//-----------------------------------------------------------------
	//do DNS lookup to get server IP
	//TODO: make DNS lookup non-blocking.
	//-----------------------------------------------------------------
	try
	{
		std::vector<IPAddress> serverips = Networking::getInstance().doDNSLookup(hostname);

		assert(!serverips.empty());
		if(serverips.empty())
			throw PollSocketExcep("could not lookup host IP with DNS");

		//-----------------------------------------------------------------
		//do the connect using the looked up ip address
		//-----------------------------------------------------------------
		doConnect(serverips[0], port);

	}
	catch(NetworkingExcep& e)
	{
		throw PollSocketExcep("DNS Lookup failed: " + std::string(e.what()));
	}
	
}

PollSocket::PollSocket(const IPAddress& ipaddress, int port)
{
	state = CLOSED;
	sockethandle = NULL;

	assert(Networking::isInited());
		
	assert(Networking::isInited());
		
	if(!Networking::isInited())
		throw PollSocketExcep("Networking not inited or destroyed.");



	doConnect(ipaddress, port);
}


PollSocket::~PollSocket()
{
	close();
}




void PollSocket::doConnect(const IPAddress& ipaddress, int port)
{
	assert(state == CLOSED);

	otherend_ipaddr = ipaddress;//remember ip of other end
	otherend_port = port;

	assert(port >= 0 && port <= 65536);

	//-----------------------------------------------------------------
	//fill out server address structure
	//-----------------------------------------------------------------
	struct sockaddr_in server_address;

	memset(&server_address, 0, sizeof(server_address));//clear struct
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons((unsigned short)port);
	server_address.sin_addr.s_addr = ipaddress.getAddr();

	//-----------------------------------------------------------------
	//create the socket
	//-----------------------------------------------------------------
	const int DEFAULT_PROTOCOL = 0;			// use default protocol
	sockethandle = socket(PF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);

	if(sockethandle == INVALID_SOCKET)
	{
		throw PollSocketExcep("could not create a socket.  Error code == " + Networking::getError());
	}



	//------------------------------------------------------------------------
	//set to non-blocking mode
	//------------------------------------------------------------------------
	{
	unsigned long non_blocking = 1;
	const int result = ioctlsocket(sockethandle, FIONBIO, &non_blocking);
	assert(result != SOCKET_ERROR); 
	}

	//-----------------------------------------------------------------
	//connect to server
	//-----------------------------------------------------------------
	const int result =  connect(sockethandle, (struct sockaddr*)&server_address, sizeof(server_address));
	
	if(result == SOCKET_ERROR)
	{
		if(::WSAGetLastError() == WSAEWOULDBLOCK)
		{
			//this is expected for a non-blocking connect
		}
		else
			throw PollSocketExcep("could not make a TCP connection with server. Error code == " + Networking::getError());
	}

	//otherend_port = port;


	//-----------------------------------------------------------------
	//get the interface address of this host used for the connection
	//-----------------------------------------------------------------
	/*struct sockaddr_in interface_addr;
	int length = sizeof(interface_addr);

	memset(&interface_addr, 0, length);
	const int result = getsockname(sockethandle, (struct sockaddr*)&interface_addr, &length);

	if(result != SOCKET_ERROR)
	{
		thisend_ipaddr = interface_addr.sin_addr.s_addr; //NEWCODE: removed ntohl
		//::debugPrint("socket thisend ip: " + thisend_ipaddr.toString());

		thisend_port = ntohs(interface_addr.sin_port); 
		//::debugPrint("socket thisend port: " + toString(thisend_port));

		//if(ipaddress != IPAddress("127.0.0.1"))//loopback connection, ignore
		//	Networking::getInstance().setUsedIPAddr(thisend_ipaddr);
	}
	else
	{
#ifdef CYBERSPACE
		::debugPrint("failed to determine ip addr and port used for socket.");
#endif
	}*/

	state = CONNECTING;//CONNECTED;
}


void PollSocket::bindAndListen(int port) throw (PollSocketExcep)
{
	assert(state == CLOSED);

	//-----------------------------------------------------------------
	//create the socket
	//-----------------------------------------------------------------
	const int DEFAULT_PROTOCOL = 0;			// use default protocol
	sockethandle = socket(PF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);

	if(sockethandle == INVALID_SOCKET)
		throw PollSocketExcep("could not create a socket.  Error code == " + Networking::getError());


	//-----------------------------------------------------------------
	//set up address struct for this host, the server.
	//-----------------------------------------------------------------
	struct sockaddr_in server_addr;

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);//accept on any interface
	server_addr.sin_port = htons(port);

	
	if(::bind(sockethandle, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 
															SOCKET_ERROR)
		throw PollSocketExcep("bind failed");


	const int backlog = 100;//NOTE: wtf should this be?

	if(::listen(sockethandle, backlog) == SOCKET_ERROR)
		throw PollSocketExcep("listen failed");

}


void PollSocket::close()
{
	//NOTE: close worker socket if not taken by client?

	if(sockethandle)
	{
		//------------------------------------------------------------------------
		//try shutting down the socket
		//------------------------------------------------------------------------
		int result = shutdown(sockethandle, 2);//2 == SD_BOTH, was 1

		//this seems to give an error on non-connected sockets (which may be the case)
		//so just ignore the error
		//assert(result == 0);
		//if(result)
		//{
			//::printWarning("Error while shutting down TCP socket: " + Networking::getError());
		//}

		//-----------------------------------------------------------------
		//close socket
		//-----------------------------------------------------------------
		result = closesocket(sockethandle);
		
		assert(result == 0);
		if(result)
		{
			//::printWarning("Error while destroying TCP socket: " + Networking::getError());
		}
	}

	sockethandle = NULL;

	state = CLOSED;
}

void PollSocket::acceptConnection()
{
	assert(state == CLOSED);

	state = ACCEPTING;
}

void PollSocket::getWorkerSocket(PollSocket& new_socket) throw (PollSocketExcep)
{
	if(state == ACCEPTING)
	{
		//return false;
	}
	else if(state == ACCEPTED)
	{
		assert(worker_socket);

		new_socket.sockethandle = worker_socket;
		new_socket.state = CONNECTED;
		new_socket.otherend_ipaddr = otherend_ipaddr;
		new_socket.otherend_port = otherend_port;



		state = CLOSED;

		//return true;
	}
	else
	{
		//error
		assert(0);
	}


	//return false;

}

const int USE_BUFFERSIZE = 1024;


void PollSocket::write(const void* data, int numbytes)
{
	assert(state == CONNECTED);

	
	const int totalnumbytestowrite = numbytes;
	
	while(numbytes > 0)//while still bytes to write
	{
		const int numbytestowrite = myMin(USE_BUFFERSIZE, numbytes);

		int numbyteswritten = send(sockethandle, (const char*)data, numbytestowrite, 0);
		
		if(numbyteswritten == SOCKET_ERROR)
		{
			if(::WSAGetLastError() == WSAEWOULDBLOCK)
			{
				//this is expected for a non-blocking connect
				assert("send gave error WSAEWOULDBLOCK" == 0);
				assert(0);

				//TEMP HACK: just try and send it again
				numbyteswritten = 0;
			}
			else
				throw PollSocketExcep("write failed.  Error code == " + Networking::getError());
		}

		numbytes -= numbyteswritten;
		data = (void*)((char*)data + numbyteswritten);//advance data pointer


		num_bytes_sent += numbyteswritten;
	}
}




void PollSocket::think()
{
	if(state == CLOSED)
	{
		//NOTE: throw exception here?
	}
	else if(state == CONNECTING)//waiting for connect() to complete
	{
		//------------------------------------------------------------------------
		//check to see if connect has completed...
		//------------------------------------------------------------------------
		timeval wait_period;
		wait_period.tv_sec = 0;
		wait_period.tv_usec = 0;

		FD_SET fd_set;
		fd_set.fd_count = 1;
		fd_set.fd_array[0] = sockethandle;

		//connect() counts as a write.
		const int num_ready = select(1, NULL, &fd_set, NULL, &wait_period);

		if(num_ready > 0)
		{
			//connection has succeded!
			state = CONNECTED;


			//------------------------------------------------------------------------
			//set to blocking mode
			//------------------------------------------------------------------------
			{
			unsigned long non_blocking = 0;
			const int result = ioctlsocket(sockethandle, FIONBIO, &non_blocking);
			assert(result != SOCKET_ERROR); 
			}

		}
		else
		{
			//check for connection failure:

			//make sure reset
			wait_period.tv_sec = 0;
			wait_period.tv_usec = 0;

			fd_set.fd_count = 1;
			fd_set.fd_array[0] = sockethandle;

			//check for exceptions
			const int num_ready = select(1, NULL, NULL, &fd_set, &wait_period);

			if(num_ready > 0)
			{
				//connect has failed
				state = CLOSED;

				throw PollSocketExcep("connect failed.");
			}
		}

		//else connect has not completed yet..
	}
	else if(state == ACCEPTING)
	{		
		//------------------------------------------------------------------------
		//check for incoming connections
		//------------------------------------------------------------------------
		timeval wait_period;
		wait_period.tv_sec = 0;
		wait_period.tv_usec = 0;

		FD_SET fd_set;
		fd_set.fd_count = 1;
		fd_set.fd_array[0] = sockethandle;
		
		const int num_ready = select(1, &fd_set, NULL, NULL, &wait_period);

		if(num_ready > 0)
		{
			//now this should accept() immediately ....

			sockaddr_in client_addr;//data struct to get the client IP
			int length = sizeof(client_addr);
			memset(&client_addr, 0, sizeof(client_addr));

			worker_socket = ::accept(sockethandle, (sockaddr*)&client_addr, &length);
			
			//TEMP HACK: store client endpoint
			otherend_ipaddr = IPAddress(client_addr.sin_addr.s_addr);
			otherend_port = ntohs(client_addr.sin_port);



			state = ACCEPTED;
		}
	}
	else if(state == CONNECTED)
	{
		//------------------------------------------------------------------------
		//check for incoming data or a gracefull connection close message.
		//------------------------------------------------------------------------
		bool try_read_data = true;
		while(try_read_data)
		{
			timeval wait_period;
			wait_period.tv_sec = 0;
			wait_period.tv_usec = 0;

			FD_SET fd_set;
			fd_set.fd_count = 1;
			fd_set.fd_array[0] = sockethandle;

			const int num_ready = select(1, &fd_set, NULL, NULL, &wait_period);

			if(num_ready)
			{
				//incoming data ready!
				char buffer[USE_BUFFERSIZE];

				const int numbytesread = 
					recv(sockethandle, buffer, USE_BUFFERSIZE, 0);

				//copy to in buffer
				for(int i=0; i<numbytesread; ++i)
				{
					in_buffer.push_back(buffer[i]);
				}

				if(numbytesread == 0)
				{
					//this means the connection was closed gracefully by the peer 
					//at other end, with all data received.
					state = CLOSED;

					try_read_data = false;//no more data to read on socket
				}	
				else if(numbytesread == SOCKET_ERROR) 
				{
					//state = CLOSED;//not connected anymore kinda

					throw PollSocketExcep("read failed, error code == " + Networking::getError());
				}

				num_bytes_rcvd += numbytesread;
			}
			else
			{
				try_read_data = false;//stop trying to read data for now.
			}
		}//end while(try_read_data)
	}

}

int PollSocket::getInBufferSize() const
{
	return (int)in_buffer.size();
}

void PollSocket::readTo(void* buffer, int numbytes)
{
	assert(numbytes <= getInBufferSize());

	if(numbytes > getInBufferSize())
		throw PollSocketExcep("tried to more data than is ready to read.");

	for(int i=0; i<numbytes; ++i)
	{
		((char*)buffer)[i] = in_buffer.front();
		in_buffer.pop_front();
	}
}

//-----------------------------------------------------------------
//funcs for measuring data rate
//-----------------------------------------------------------------
int PollSocket::getNumBytesSent()
{
	return num_bytes_sent;
}
int PollSocket::getNumBytesRcvd()
{
	return num_bytes_rcvd;
}

void PollSocket::resetNumBytesSent()
{
	num_bytes_sent = 0;
}
void PollSocket::resetNumBytesRcvd()
{
	num_bytes_rcvd = 0;
}







void PollSocket::write(float x)
{
	*((unsigned int*)&x) = htonl(*((unsigned int*)&x));//convert to network byte ordering

	write(&x, sizeof(float));
}

void PollSocket::write(int x)
{
	*((unsigned int*)&x) = htonl(*((unsigned int*)&x));//convert to network byte ordering

	write(&x, sizeof(int));
}

void PollSocket::write(char x)
{
	write(&x, sizeof(char));
}	

void PollSocket::write(unsigned char x)
{
	write(&x, sizeof(unsigned char));
}

void PollSocket::write(const std::string& s)//writes null-terminated string
{
	write(s.c_str(), s.size() + 1);
}


void PollSocket::write(unsigned short x)
{
	x = htons(x);//convert to network byte ordering

	write(&x, sizeof(unsigned short));
}

void PollSocket::readTo(float& x)
{
	readTo(&x, sizeof(float));

	*((unsigned int*)&x) = ntohl(*((unsigned int*)&x));//convert from network to host byte ordering
}

void PollSocket::readTo(int& x)
{
	readTo(&x, sizeof(int));
	*((unsigned int*)&x) = ntohl(*((unsigned int*)&x));//convert from network to host byte ordering
}

void PollSocket::readTo(char& x)
{
	readTo(&x, sizeof(char));
}

void PollSocket::readTo(unsigned char& x)
{
	readTo(&x, sizeof(unsigned char));
}

void PollSocket::readTo(unsigned short& x)
{
	readTo(&x, sizeof(unsigned short));
	x = ntohs(x);//convert from network to host byte ordering
}

void PollSocket::readTo(std::string& s)
{
	//NOTE: dodgy
	std::vector<char> buffer(1000);

	int i = 0;
	while(1)
	{
		buffer.push_back('\0');
		readTo(buffer[i]);

		if(buffer[i] == '\0')
			break;

		//TODO: break after looping to long
		++i;
	}

	s = &(*buffer.begin());
}





int PollSocket::num_bytes_sent = 0;
int PollSocket::num_bytes_rcvd = 0;

