/*=====================================================================
mysocket.cpp
------------
File created by ClassTemplate on Wed Apr 17 14:43:14 2002
Code By Nicholas Chapman.

Code Copyright Nicholas Chapman 2005.

=====================================================================*/
#include "mysocket.h"

#include "networking.h"
#include "../maths/vec3.h"
#include <vector>
#include "fractionlistener.h"
#include "../utils/stringutils.h"
#include <string.h>
#if defined(WIN32) || defined(WIN64)
#else
#include <netinet/in.h>
#include <unistd.h> // for close()
#include <sys/time.h> // fdset
#include <sys/types.h> // fdset
#include <sys/select.h>
#endif

//#include <iostream> //TEMP


/*inline void zeroHandle(SOCKETHANDLE_TYPE& sockhandle)
{
#ifdef WIN32
	sockhandle = NULL;
#else
	sockhandle = 0;
#endif
}*/

#if defined(WIN32) || defined(WIN64)
typedef int SOCKLEN_TYPE;
#else
typedef socklen_t SOCKLEN_TYPE;

const int SOCKET_ERROR = -1;
#endif




MySocket::MySocket(const std::string hostname, int port)
{
	
	thisend_port = -1;
	otherend_port = -1;
	sockethandle = 0;

	assert(Networking::isInited());
		
	if(!Networking::isInited())
		throw MySocketExcep("Networking not inited or destroyed.");

//#ifdef CYBERSPACE
	//if(Networking::getInstance().shouldSocketsShutDown())
	//	throw MySocketExcep("::Networking::shouldSocketsShutDown() == true");
//#endif



	//-----------------------------------------------------------------
	//do DNS lookup to get server IP
	//-----------------------------------------------------------------
	/*struct hostent* host_info = gethostbyname(hostname.c_str());

	if(!host_info)
	{
		throw MySocketExcep("could not lookup host IP with DNS");
	}

	IPAddress serverip(*((unsigned int*)(host_info->h_addr)));*/
	//NOTE: checkme cast is alright

	try
	{
		std::vector<IPAddress> serverips = Networking::getInstance().doDNSLookup(hostname);

		assert(!serverips.empty());
		if(serverips.empty())
			throw MySocketExcep("could not lookup host IP with DNS");

		//-----------------------------------------------------------------
		//do the connect using the looked up ip address
		//-----------------------------------------------------------------
		doConnect(serverips[0], port);

	}
	catch(NetworkingExcep& e)
	{
		throw MySocketExcep("DNS Lookup failed: " + std::string(e.what()));
	}
	
}


MySocket::MySocket(const IPAddress& ipaddress, int port)
{
	thisend_port = -1;
	otherend_port = -1;
	sockethandle = 0;

	assert(Networking::isInited());


	doConnect(ipaddress, port);
}


MySocket::MySocket()
{
	thisend_port = -1;
	sockethandle = 0;

}

void MySocket::doConnect(const IPAddress& ipaddress, int port)
{

	otherend_ipaddr = ipaddress;//remember ip of other end

	assert(port >= 0 && port <= 65536);

	//-----------------------------------------------------------------
	//fill out server address structure
	//-----------------------------------------------------------------
	struct sockaddr_in server_address;

	memset(&server_address, 0, sizeof(server_address));//clear struct
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons((unsigned short)port);
	server_address.sin_addr.s_addr = ipaddress.getAddr();

//	const std::string temp = ipaddress.toString();


	//-----------------------------------------------------------------
	//create the socket
	//-----------------------------------------------------------------
	const int DEFAULT_PROTOCOL = 0;			// use default protocol
	sockethandle = socket(PF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);

	if(!isSockHandleValid(sockethandle))
	{
		throw MySocketExcep("could not create a socket.  Error code == " + Networking::getError());
	}

	//------------------------------------------------------------------------
	//set timeouts for reading and writing data
	//------------------------------------------------------------------------
	/*{
	const int rcv_timeout = 1000;//ms
	const int result = setsockopt(sockethandle, IPPROTO_TCP, SO_RCVTIMEO, (const char*)&rcv_timeout, sizeof(rcv_timeout));

	assert(result != SOCKET_ERROR);
	if(result == SOCKET_ERROR)
	{
		::error(Networking::getError());
	}

	}
	{
	const int send_timeout = 1000;//ms
	const int result = setsockopt(sockethandle, IPPROTO_TCP, SO_SNDTIMEO, (const char*)&send_timeout, sizeof(send_timeout));

	assert(result != SOCKET_ERROR);
	}*/

	//-----------------------------------------------------------------
	//connect to server
	//NOTE: get rid of this blocking connect.
	//-----------------------------------------------------------------
	if(connect(sockethandle, (struct sockaddr*)&server_address, sizeof(server_address)) == -1)
	{
		throw MySocketExcep("could not make a TCP connection with server. Error code == " + Networking::getError());
	}

	otherend_port = port;


	//-----------------------------------------------------------------
	//get the interface address of this host used for the connection
	//-----------------------------------------------------------------
	struct sockaddr_in interface_addr;
	SOCKLEN_TYPE length = sizeof(interface_addr);

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
	}

}


MySocket::~MySocket()
{
	close();
	/*if(sockethandle)
	{
		//------------------------------------------------------------------------
		//try shutting down the socket
		//------------------------------------------------------------------------
		int result = shutdown(sockethandle, 1);

		assert(result == 0);
		if(result)
		{
			::printWarning("Error while shutting down TCP socket: " + Networking::getError());
		}

		//-----------------------------------------------------------------
		//close socket
		//-----------------------------------------------------------------
		result = closesocket(sockethandle);
		
		assert(result == 0);
		if(result)
		{
			::printWarning("Error while destroying TCP socket: " + Networking::getError());
		}
	}*/

}








void MySocket::bindAndListen(int port) // throw (MySocketExcep)
{

	assert(Networking::isInited());

	//-----------------------------------------------------------------
	//if socket already exists, destroy it
	//-----------------------------------------------------------------
	if(sockethandle)
	{
		assert(0);
		/*const int result = closesocket(sockethandle);
		assert(result != SOCKET_ERROR);
		sockethandle = NULL;*/
		throw MySocketExcep("Socket already created.");
	}

	//-----------------------------------------------------------------
	//set up address struct for this host, the server.
	//-----------------------------------------------------------------
	struct sockaddr_in server_addr;

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);//accept on any interface
	server_addr.sin_port = htons(port);


	//-----------------------------------------------------------------
	//create the socket
	//-----------------------------------------------------------------
	const int DEFAULT_PROTOCOL = 0;			// use default protocol
	sockethandle = socket(PF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);

	if(!isSockHandleValid(sockethandle))
	{
		throw MySocketExcep("could not create a socket.  Error code == " + Networking::getError());
	}


	if(::bind(sockethandle, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 
															SOCKET_ERROR)
		throw MySocketExcep("bind failed");


	const int backlog = 100;//NOTE: wtf should this be?

	if(::listen(sockethandle, backlog) == SOCKET_ERROR)
		throw MySocketExcep("listen failed");

	thisend_port = port;

}



void MySocket::acceptConnection(MySocket& new_socket, SocketShouldAbortCallback* should_abort_callback) // throw (MySocketExcep)
{
	assert(Networking::isInited());

	// Wait until the accept() will succeed.
	while(1)
	{
		const double BLOCK_DURATION = 0.5; // in seconds
		timeval wait_period;
		wait_period.tv_sec = 0; // seconds 
		wait_period.tv_usec = (long)(BLOCK_DURATION * 1000000.0); // and microseconds 

		// Create the file descriptor set
		fd_set sockset;
		initFDSetWithSocket(sockset, sockethandle);
	
		//if(Networking::getInstance().shouldSocketsShutDown())
		//	throw MySocketExcep("::Networking::shouldSocketsShutDown() == true");
		if(should_abort_callback && should_abort_callback->shouldAbort())
			throw AbortedMySocketExcep();

		const int num_ready = select(
			sockethandle + SOCKETHANDLE_TYPE(1), // nfds: range of file descriptors to test
			&sockset, // read fds
			NULL, // write fds
			NULL, // error fds
			&wait_period // timeout
			);

		//std::cout << "MySocket::acceptConnection(): num_ready: " << num_ready << std::endl;

		if(should_abort_callback && should_abort_callback->shouldAbort()) // Networking::getInstance().shouldSocketsShutDown())
			throw AbortedMySocketExcep(); // ::Networking::shouldSocketsShutDown() == true");

		if(num_ready != 0)
			break;
	}

	// Now this should accept() immediately .... :)

	sockaddr_in client_addr;//data struct to get the client IP
	SOCKLEN_TYPE length = sizeof(client_addr);

	memset(&client_addr, 0, length);

	SOCKETHANDLE_TYPE newsockethandle = ::accept(sockethandle, (sockaddr*)&client_addr, &length);

	if(!isSockHandleValid(newsockethandle))
		throw MySocketExcep("accept failed");

/*	if(poll_mode)
	{

		unsigned long NON_BLOCKING = 1;

		const int result = ioctlsocket(socket_handle, FIONBIO, &b);

		assert(result != SOCKET_ERROR); 

		while(1)
		{

			SOCKET newsockethandle = ::accept(sockethandle, (sockaddr*)&client_addr, &length);

			if(newsockethandle == INVALID_SOCKET)
				throw MySocketExcep("accept failed");

*/

	//-----------------------------------------------------------------
	//copy data over to new socket that will do actual communicating
	//-----------------------------------------------------------------
	new_socket.sockethandle = newsockethandle;

	//-----------------------------------------------------------------
	//get other end ip and port
	//-----------------------------------------------------------------
	new_socket.otherend_ipaddr = IPAddress(client_addr.sin_addr.s_addr);
	//NEWCODE removed ntohl

	new_socket.otherend_port = ntohs(client_addr.sin_port);

	//-----------------------------------------------------------------
	//get the interface address of this host used for the connection
	//-----------------------------------------------------------------
	sockaddr_in interface_addr;
	length = sizeof(interface_addr);

	memset(&interface_addr, 0, length);
	const int result = getsockname(sockethandle, (struct sockaddr*)&interface_addr, &length);

	if(result != SOCKET_ERROR)
	{
		thisend_ipaddr = interface_addr.sin_addr.s_addr; 
		//NEWCODE removed ntohl
		//::debugPrint("socket thisend ip: " + thisend_ipaddr.toString());

		thisend_port = ntohs(interface_addr.sin_port);
		//::debugPrint("socket thisend port: " + toString(thisend_port));

		//if(thisend_ipaddr != IPAddress("127.0.0.1"))//loopback connection, ignore
		//	Networking::getInstance().setUsedIPAddr(thisend_ipaddr);

	}
	else
	{
#ifdef CYBERSPACE
		::debugPrint("failed to determine ip addr and port used for socket.");
#endif
	}

}
 



void MySocket::close()
{
	if(sockethandle)
	{
		//------------------------------------------------------------------------
		//try shutting down the socket
		//------------------------------------------------------------------------
		int result = shutdown(sockethandle,  2);//2 == SD_BOTH, was 1

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
#if defined(WIN32) || defined(WIN64)
		result = closesocket(sockethandle);
#else
		result = ::close(sockethandle);
#endif	
		assert(result == 0);
		if(result)
		{
			//::printWarning("Error while destroying TCP socket: " + Networking::getError());
		}
	}
	/*//-----------------------------------------------------------------
	//close socket
	//-----------------------------------------------------------------
	if(closesocket(sockethandle) == SOCKET_ERROR)
	{
		//NOTE: could just do nothing here?
		throw std::exception("error closing socket");
	}*/

	sockethandle = 0;
}






const int USE_BUFFERSIZE = 1024;


void MySocket::write(const void* data, int datalen, SocketShouldAbortCallback* should_abort_callback)
{	
	write(data, datalen, NULL, should_abort_callback);
}

void MySocket::write(const void* data, int datalen, FractionListener* frac, SocketShouldAbortCallback* should_abort_callback)
{
	const int totalnumbytestowrite = datalen;
	
	while(datalen > 0)//while still bytes to write
	{
		const int numbytestowrite = myMin(USE_BUFFERSIZE, datalen);

		//------------------------------------------------------------------------
		//NEWCODE: loop until either the prog is exiting or can write to socket
		//------------------------------------------------------------------------

		while(1)
		{
			const float BLOCK_DURATION = 0.5f;
			timeval wait_period;
			wait_period.tv_sec = 0;
			wait_period.tv_usec = (long)(BLOCK_DURATION * 1000000.0f);

			fd_set sockset;
			initFDSetWithSocket(sockset, sockethandle); //FD_SET(sockethandle, &sockset);
		
			//if(Networking::getInstance().shouldSocketsShutDown())
			//	throw MySocketExcep("::Networking::shouldSocketsShutDown() == true");
			if(should_abort_callback && should_abort_callback->shouldAbort())
				throw AbortedMySocketExcep();

			//get number of handles that are ready to write to
			const int num_ready = select(sockethandle + SOCKETHANDLE_TYPE(1), NULL, &sockset, NULL, &wait_period);

			//if(Networking::getInstance().shouldSocketsShutDown())
			//	throw MySocketExcep("::Networking::shouldSocketsShutDown() == true");
			if(should_abort_callback && should_abort_callback->shouldAbort())
				throw AbortedMySocketExcep();

			if(num_ready != 0)
				break;
		}

		const int numbyteswritten = send(sockethandle, (const char*)data, numbytestowrite, 0);
		
		if(numbyteswritten == SOCKET_ERROR)
			throw MySocketExcep("write failed.  Error code == " + Networking::getError());

		datalen -= numbyteswritten;
		data = (void*)((char*)data + numbyteswritten);//advance data pointer

		MySocket::num_bytes_sent += numbyteswritten;

		if(frac)
			frac->setFraction((float)(totalnumbytestowrite - datalen) / (float)totalnumbytestowrite);
	
		//-----------------------------------------------------------------
		//artificially slow down connection speed
		//-----------------------------------------------------------------
		/*const bool SLOW_CONNECTION = false;

		if(SLOW_CONNECTION && datalen > 0)
		{
			const float max_rate = 10000;//Bytes/sec
			static bool printed_warning = false;

			if(!printed_warning)
			{
				//::debugPrint("WARNING: artificially narrowing TCP socket rate to " +
				//					::toString(max_rate) + "B/s !", Vec3(1,1,0));

				printed_warning = true;
			}
			
			const float sleep_time = 1000.0 * (float)USE_BUFFERSIZE / max_rate;//in ms
			Sleep(sleep_time);
		}*/

		//if(Networking::getInstance().shouldSocketsShutDown())
		//	throw MySocketExcep("::Networking::shouldSocketsShutDown() == true");
	}
}


void MySocket::readTo(void* buffer, int readlen, SocketShouldAbortCallback* should_abort_callback)
{
	readTo(buffer, readlen, NULL, should_abort_callback);
}


void MySocket::readTo(void* buffer, int readlen, FractionListener* frac, SocketShouldAbortCallback* should_abort_callback)
{
	const int totalnumbytestoread = readlen;

	while(readlen > 0)//while still bytes to read
	{		
		const int numbytestoread = myMin(USE_BUFFERSIZE, readlen);

		//------------------------------------------------------------------------
		//NEWCODE: loop until either the prog is exiting or have incoming data
		//------------------------------------------------------------------------

		while(1)
		{
			const float BLOCK_DURATION = 0.5f;
			timeval wait_period;
			wait_period.tv_sec = 0;
			wait_period.tv_usec = (long)(BLOCK_DURATION * 1000000.0f);

			//FD_SET fd_set;
			fd_set sockset;
			initFDSetWithSocket(sockset, sockethandle); //FD_SET(sockethandle, &sockset);
		
			//if(Networking::getInstance().shouldSocketsShutDown())
			//	throw MySocketExcep("::Networking::shouldSocketsShutDown() == true");
			if(should_abort_callback && should_abort_callback->shouldAbort())
				throw AbortedMySocketExcep();
	
			//get number of handles that are ready to read from
			const int num_ready = select(sockethandle + SOCKETHANDLE_TYPE(1), &sockset, NULL, NULL, &wait_period);

			//if(Networking::getInstance().shouldSocketsShutDown())
			//	throw MySocketExcep("::Networking::shouldSocketsShutDown() == true");
			if(should_abort_callback && should_abort_callback->shouldAbort())
				throw AbortedMySocketExcep();

			if(num_ready != 0)
				break;
		}

		//------------------------------------------------------------------------
		//do the read
		//------------------------------------------------------------------------
		const int numbytesread = recv(sockethandle, (char*)buffer, numbytestoread, 0);

		if(numbytesread == SOCKET_ERROR || numbytesread == 0) 
			throw MySocketExcep("read failed, error: " + Networking::getError());

		readlen -= numbytesread;
		buffer = (void*)((char*)buffer + numbytesread);
			
		MySocket::num_bytes_rcvd += numbytesread;
		
		if(frac)
			frac->setFraction((float)(totalnumbytestoread - readlen) / (float)totalnumbytestoread);

		//if(Networking::getInstance().shouldSocketsShutDown())
		//	throw MySocketExcep("::Networking::shouldSocketsShutDown() == true");
		if(should_abort_callback && should_abort_callback->shouldAbort())
			throw AbortedMySocketExcep();
	}
}


/*void MySocket::readMessage(std::string& data_out, FractionListener* frac)
{


	std::vector<char> buf(USE_BUFFERSIZE);
	data_out = "";

	bool done = false;

	while(!done)
	{
	
		//-----------------------------------------------------------------
		//read data to buf
		//-----------------------------------------------------------------
		const int num_chars_received = 
				recv(sockethandle, &(*buf.begin()), USE_BUFFERSIZE, 0);


   		if(num_chars_received == SOCKET_ERROR)
   		{
			throw MySocketExcep("socket error while reading data.  Error code == " + Networking::getError());
		}

		MySocket::num_bytes_rcvd += num_chars_received;

		//-----------------------------------------------------------------
		//append buf to end of string 'data_out'
		//-----------------------------------------------------------------
		const int writeindex = data_out.size();
		data_out.resize(data_out.size() + num_chars_received);
		for(int i=0; i<num_chars_received; ++i)
			data_out[writeindex + i] = buf[i];

   		if(num_chars_received < USE_BUFFERSIZE)
   			done = true;

		if(::threadsShutDown())
			throw MySocketExcep("::threadsShutDown() == true");

   }
}*/




void MySocket::write(float x, SocketShouldAbortCallback* should_abort_callback)
{
	*((unsigned int*)&x) = htonl(*((unsigned int*)&x));//convert to network byte ordering

	write(&x, sizeof(float), should_abort_callback);
}

void MySocket::write(int x, SocketShouldAbortCallback* should_abort_callback)
{
	*((unsigned int*)&x) = htonl(*((unsigned int*)&x));//convert to network byte ordering

	write(&x, sizeof(int), should_abort_callback);
}

void MySocket::write(char x, SocketShouldAbortCallback* should_abort_callback)
{
	write(&x, sizeof(char), should_abort_callback);
}	

void MySocket::write(unsigned char x, SocketShouldAbortCallback* should_abort_callback)
{
	write(&x, sizeof(unsigned char), should_abort_callback);
}

void MySocket::write(const std::string& s, SocketShouldAbortCallback* should_abort_callback) // writes string
{
	//write(s.c_str(), s.size() + 1);
	
	// Write length of string
	write((int)s.length(), should_abort_callback);

	// Write string data
	write(&(*s.begin()), s.length(), should_abort_callback);
}

/*
void MySocket::write(const Vec3& vec)
{
	write(vec.x);
	write(vec.y);
	write(vec.z);
}*/

void MySocket::write(unsigned short x, SocketShouldAbortCallback* should_abort_callback)
{
	x = htons(x);//convert to network byte ordering

	write(&x, sizeof(unsigned short), should_abort_callback);
}


/*float MySocket::readFloat()
{
	float x;
	read(&x, sizeof(float));

	x = ntohl(x);//convert from network to host byte ordering

	return x;
}

int MySocket::readInt()
{
	int x;
	read(&x, sizeof(int));

	x = ntohl(x);//convert from network to host byte ordering

	return x;
}

char MySocket::readChar()
{
	char x;
	read(&x, sizeof(char));

	return x;
}*/


void MySocket::readTo(float& x, SocketShouldAbortCallback* should_abort_callback)
{
	readTo(&x, sizeof(float), should_abort_callback);

	*((unsigned int*)&x) = ntohl(*((unsigned int*)&x));//convert from network to host byte ordering
}

void MySocket::readTo(int& x, SocketShouldAbortCallback* should_abort_callback)
{
	readTo(&x, sizeof(int), should_abort_callback);
	*((unsigned int*)&x) = ntohl(*((unsigned int*)&x));//convert from network to host byte ordering
}

void MySocket::readTo(char& x, SocketShouldAbortCallback* should_abort_callback)
{
	readTo(&x, sizeof(char), should_abort_callback);
}

void MySocket::readTo(unsigned char& x, SocketShouldAbortCallback* should_abort_callback)
{
	readTo(&x, sizeof(unsigned char), should_abort_callback);
}

void MySocket::readTo(unsigned short& x, SocketShouldAbortCallback* should_abort_callback)
{
	readTo(&x, sizeof(unsigned short), should_abort_callback);
	x = ntohs(x);//convert from network to host byte ordering
}

/*
void MySocket::readTo(Vec3& vec)
{
	readTo(vec.x);
	readTo(vec.y);
	readTo(vec.z);
}
*/
void MySocket::readTo(std::string& s, int maxlength, SocketShouldAbortCallback* should_abort_callback)
{
	/*std::vector<char> buffer(1000);

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

	s = &(*buffer.begin());*/

	// Read length
	int length;
	readTo(length, should_abort_callback);
	if(length < 0 || length > maxlength)
		throw MySocketExcep("String was too long.");
	//std::vector<char> buffer(length);
	//readTo(buffer

	s.resize(length);
	readTo(&(*s.begin()), length, NULL, should_abort_callback);
}

void MySocket::readTo(std::string& x, int numchars, FractionListener* frac, SocketShouldAbortCallback* should_abort_callback)
{
//	x.clear();
	assert(numchars >= 0);

	x.resize(numchars);
	readTo(&(*x.begin()), numchars, frac, should_abort_callback);
}

//-----------------------------------------------------------------
//funcs for measuring data rate
//-----------------------------------------------------------------
int MySocket::getNumBytesSent()
{
	return num_bytes_sent;
}
int MySocket::getNumBytesRcvd()
{
	return num_bytes_rcvd;
}

void MySocket::resetNumBytesSent()
{
	num_bytes_sent = 0;
}
void MySocket::resetNumBytesRcvd()
{
	num_bytes_rcvd = 0;
}

void MySocket::pollRead(std::string& data_out)
{
	data_out.resize(0);

	const float BLOCK_DURATION = 0.0f;
	timeval wait_period;
	wait_period.tv_sec = 0;
	wait_period.tv_usec = (long)(BLOCK_DURATION * 1000000.0f);

	//FD_SET fd_set;
	fd_set sockset;
	initFDSetWithSocket(sockset, sockethandle); //FD_SET(sockethandle, &sockset);
		
	//if(Networking::getInstance().shouldSocketsShutDown())
	//	throw MySocketExcep("::Networking::shouldSocketsShutDown() == true");

	//get number of handles that are ready to read from
	const int num_ready = select(sockethandle+1, &sockset, NULL, NULL, &wait_period);

	//if(Networking::getInstance().shouldSocketsShutDown())
	//	throw MySocketExcep("::Networking::shouldSocketsShutDown() == true");

	if(num_ready == 0)
		return;
		

	data_out.resize(USE_BUFFERSIZE);
	const int numbytesread = recv(sockethandle, &(*data_out.begin()), data_out.size(), 0);


	if(numbytesread == SOCKET_ERROR || numbytesread == 0) 
		throw MySocketExcep("read failed, error code == " + Networking::getError());


	data_out.resize(numbytesread);

	/*data_out.resize(1024);//max bytes to read at once

	const int num_pending_bytes = recv(sockethandle, &(*data_out.begin()), data_out.size(), MSG_PEEK);

	//now remove from the input queue
	data_out.resize(num_pending_bytes);

	if(num_pending_bytes != 0)
	{

		const int numread = recv(sockethandle, &(*data_out.begin()), data_out.size(), 0);

		assert(numread == data_out.size());
	}*/
}

bool MySocket::isSockHandleValid(SOCKETHANDLE_TYPE handle)
{
#if defined(WIN32) || defined(WIN64)
	return handle != INVALID_SOCKET;
#else
	return handle >= 0;
#endif
}

void MySocket::setNagleAlgEnabled(bool enabled_)//on by default.
{
#if defined(WIN32) || defined(WIN64)
	BOOL enabled = enabled_;

	::setsockopt(sockethandle, //socket handle
		IPPROTO_TCP, //level
		TCP_NODELAY, //option name
		(const char*)&enabled,//value
		sizeof(BOOL));//size of value buffer
#else
	//int enabled = enabled_;
	//TODO
	assert(0);
#endif
}

void MySocket::initFDSetWithSocket(fd_set& sockset, SOCKETHANDLE_TYPE& sockhandle)
{
	FD_ZERO(&sockset);
	
	//FD_SET doesn´t seem to work when targeting x64 in gcc.
#ifdef COMPILER_GCC
	//sockset.fds_bits[0] = sockhandle;
	FD_SET(sockhandle, &sockset);
#else
	FD_SET(sockhandle, &sockset);
#endif
}


int MySocket::num_bytes_sent = 0;
int MySocket::num_bytes_rcvd = 0;
