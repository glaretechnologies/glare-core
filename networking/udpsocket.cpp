/*=====================================================================
udpsocket.cpp
-------------
File created by ClassTemplate on Sat Apr 13 04:40:34 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "udpsocket.h"

#include "networking.h"
#include "ipaddress.h"
#include "packet.h"
#include <assert.h>
#include "../utils/lock.h"
//#include "../utils/inifile.h"
#include "../utils/stringutils.h"

#if defined(WIN32) || defined(WIN64)

#else
#include <netinet/in.h>
#include <unistd.h>//for close()
#include <sys/time.h>//fdset
#include <sys/types.h>//fdset
#include <sys/select.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#endif


#if defined(WIN32) || defined(WIN64)
static bool isSocketError(int result)
{
	return result == SOCKET_ERROR;
}
#else
static bool isSocketError(ssize_t result)
{
	return result == -1;
}
#endif

#if defined(WIN32) || defined(WIN64)
typedef int SOCKLEN_TYPE;
#else
typedef socklen_t SOCKLEN_TYPE;
#endif



UDPSocket::UDPSocket()//create outgoing socket
{
	thisend_port = -1;
	socket_handle = 0;

	assert(Networking::isInited());

	if(!Networking::isInited())
		throw UDPSocketExcep("Networking not inited or destroyed.");


	//-----------------------------------------------------------------
	//create a UDP socket
	//-----------------------------------------------------------------
	const int DEFAULT_PROTOCOL = 0;			// use default protocol

	socket_handle = socket(PF_INET, SOCK_DGRAM, DEFAULT_PROTOCOL);

	if(!isSockHandleValid(socket_handle))
		throw UDPSocketExcep("could not create a socket");

	//-----------------------------------------------------------------
	//get the interface of this host used for the connection
	//-----------------------------------------------------------------
	struct sockaddr_in interface_addr;
	SOCKLEN_TYPE length = sizeof(interface_addr);

	memset(&interface_addr, 0, length);
	const int result = getsockname(socket_handle, (struct sockaddr*)&interface_addr, &length);

	if(isSockHandleValid(result))//NOTE: this correct? //result != SOCKET_ERROR)
	{
		//thisend_port = ntohs(interface_addr.sin_port);
		//::debugPrint("UDPSocket socket thisend port: " + toString(thisend_port));
		IPAddress ip_used = interface_addr.sin_addr.s_addr; 
	}
	else
	{
		//::debugPrint("failed to determine ip addr and port used for UDP socket.");
	}

}




UDPSocket::~UDPSocket()
{
	//-----------------------------------------------------------------
	//close socket
	//-----------------------------------------------------------------
	/*if(closesocket(socket_handle) == SOCKET_ERROR)
	{
		//NOTE: could just do nothing here?
		//throw std::exception("error closing socket");
	}*/
	
	if(socket_handle)
	{
		//------------------------------------------------------------------------
		//try shutting down the socket
		//------------------------------------------------------------------------
		int result = shutdown(socket_handle, 1);

		
		if(result)
		{
			const std::string e = Networking::getInstance().getError();
			//::printWarning("Error while shutting down UDP socket: " + Networking::getError());
		}
		assert(result == 0);

		//-----------------------------------------------------------------
		//close socket
		//-----------------------------------------------------------------
#if defined(WIN32) || defined(WIN64)
		result = closesocket(socket_handle);
#else
		result = ::close(socket_handle);
#endif
		
		assert(result == 0);
		if(result)
		{
			//::printWarning("Error while destroying UDP socket: " + Networking::getError());
		}
	}
}


void UDPSocket::bindToPort(const Port& port)//listen on a particluar port
{
	//-----------------------------------------------------------------
	//bind the socket to a specific port
	//-----------------------------------------------------------------
	struct sockaddr_in my_addr;    // my address information

	my_addr.sin_family = AF_INET;         
	my_addr.sin_port = htons(port.getPort());     
	my_addr.sin_addr.s_addr = INADDR_ANY; // accept on any network interface
	memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct

	if(bind(socket_handle, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == -1) 
	{
		throw UDPSocketExcep("error binding socket to port " + port.toString() + ": " + Networking::getInstance().getError());
	}       

	thisend_port = port;
	//debugPrint("UDPSocket: listening on port " + port.toString() + "...");


	
	//-----------------------------------------------------------------
	//TEMP: get the interface of this host used for the connection
	//NOTE: just gives 0.0.0.0 for the ip
	//-----------------------------------------------------------------
	/*struct sockaddr_in interface_addr;
	int length = sizeof(interface_addr);

	memset(&interface_addr, 0, length);
	const int result = getsockname(socket_handle, (struct sockaddr*)&interface_addr, &length);

	if(result != SOCKET_ERROR)
	{
		//thisend_port = ntohs(interface_addr.sin_port);
		//::debugPrint("UDPSocket socket thisend port: " + toString(thisend_port));
		IPAddress ip_used = interface_addr.sin_addr.s_addr; 
		::debugPrint("UDPsocket bound on ip " + ip_used.toString());
	}
	else
	{
		//::debugPrint("failed to determine ip addr and port used for UDP socket.");
	}*/
}

void UDPSocket::enableBroadcast()
{
	const int optval = 1;
	const int result = ::setsockopt(
		socket_handle,
		SOL_SOCKET, // level
		SO_BROADCAST, // optname
		(const char*)&optval, // optval
		sizeof(optval) // optlen
		);

	assert(result == 0);
}


void UDPSocket::sendPacket(const Packet& packet, const IPAddress& dest_ip, const Port& destport)
{
	sendPacket(packet.getData(), packet.getPacketSize(), dest_ip, destport);
}



void UDPSocket::sendPacket(const char* data, int datalen, const IPAddress& dest_ip, const Port& destport)
{

	//if(showsentpackets)
	//NOTE: move	debugPrint("UDPSocket: sending packet to host at " + dest_ip.toString());

	//-----------------------------------------------------------------
	//create the destination address structure
	//-----------------------------------------------------------------
	struct sockaddr_in dest_address;

	memset(&dest_address, 0, sizeof(sockaddr_in));//0 it
	dest_address.sin_family = AF_INET;
	dest_address.sin_addr.s_addr = dest_ip.getAddr();//NEWCODE removed htonl
	dest_address.sin_port = htons(destport.getPort());


	//const unsigned long binary_ip = htonl(dest_ip.getAddr());

	//if(binary_ip == -1)
	//{	//error...
	//	throw std::exception("invalid IP address");//: " + dest_ip.toString()));
	//}



	{
	//Lock mutexlock(mutex);

	//-----------------------------------------------------------------
	//send it!
	//-----------------------------------------------------------------
	const int numbytessent = ::sendto(socket_handle, data, datalen, 0, 
							(struct sockaddr*)&dest_address, sizeof(sockaddr));
	

	if(isSocketError(numbytessent)) // numbytessent == SOCKET_ERROR)
	{
		//const int socket_err_num = WSAGetLastError();
		throw UDPSocketExcep("error while sending bytes over socket: " + Networking::getInstance().getError());
	}

	if(numbytessent < datalen)
	{
		//NOTE: FIXME this really an error?
		throw UDPSocketExcep("error: could not get all bytes in one packet.");
	}

	UDPSocket::num_bytes_sent += numbytessent;


	
	//-----------------------------------------------------------------
	//get the interface address of this host used for the connection
	//-----------------------------------------------------------------
	struct sockaddr_in interface_addr;
	SOCKLEN_TYPE length = sizeof(interface_addr);

	memset(&interface_addr, 0, length);
	const int result = getsockname(socket_handle, (struct sockaddr*)&interface_addr, &length);

	if(!isSocketError(result)) // result != SOCKET_ERROR)
	{
		IPAddress ip_used = interface_addr.sin_addr.s_addr; 
		//NEWCODE removed htonl
	}
	else
	{
		//::debugPrint("failed to determine ip addr and port used for socket.");
	}


	}
}

 

//returns num bytes read.  blocking.
int UDPSocket::readPacket(char* buf, int buflen, IPAddress& sender_ip_out, 
						  Port& senderport_out, IPAddress& thisend_ip_out, bool peek)
{

	struct sockaddr_in from_address; // senders's address information

	int flags = 0;
	//if(peek)
	//	flags = MSG_PEEK;

	SOCKLEN_TYPE from_add_size = sizeof(struct sockaddr_in);
	//-----------------------------------------------------------------
	//get one packet.
	//-----------------------------------------------------------------
	const int numbytesrcvd = ::recvfrom(socket_handle, buf, buflen, flags, 
								(struct sockaddr*)&from_address, &from_add_size);


	if(isSocketError(numbytesrcvd)) // numbytesrcvd == SOCKET_ERROR)
	{
#if defined(WIN32) || defined(WIN64)
		if(WSAGetLastError() == WSAEWOULDBLOCK)
		{
			//doesn't matter.

			return 0;//return 0 bytes read
		}
		else
		{
			const std::string errorstring = Networking::getError();
			if(errorstring == "WSAECONNRESET")
			{
				//socket is dead thanks to stupid xp... so reopen it
				//recommended solution is to just use a different socket for sending UDP datagrams.

				//-----------------------------------------------------------------
				//get the send lock as well so as to avoid problems
				//-----------------------------------------------------------------
				//Lock mutexlock(sendmutex);

				//-----------------------------------------------------------------
				//close socket
				//-----------------------------------------------------------------
				closesocket(socket_handle);

				//-----------------------------------------------------------------
				//open socket
				//-----------------------------------------------------------------
				const int DEFAULT_PROTOCOL = 0;			// use default protocol

				socket_handle = socket(PF_INET, SOCK_DGRAM, DEFAULT_PROTOCOL);

				if(socket_handle == INVALID_SOCKET)
				{
					throw UDPSocketExcep("could not RE-create a socket after WSAECONNRESET");
				}

				//-----------------------------------------------------------------
				//bind to port it was on before
				//-----------------------------------------------------------------
				assert(thisend_port.getPort() != 0);

				struct sockaddr_in my_addr;    // my address information

				my_addr.sin_family = AF_INET;         
				my_addr.sin_port = htons(thisend_port.getPort());     
				my_addr.sin_addr.s_addr = INADDR_ANY; // accept on any network interface
				memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct

				if(bind(socket_handle, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == -1) 
				{
 					throw UDPSocketExcep("error RE-binding socket to port " + thisend_port.toString() + " after WSAECONNRESET");
				}  
				
				//::debugPrint("UDPSocket: WSAECONNRESET error, reopened UDP socket.");

				//now just throw an excep and ignore the current packet, UDP is unreliable anyway..
			}


			throw UDPSocketExcep("error while reading from socket: error code == " + errorstring);
		}
#else
		if(errno == EAGAIN || errno == EWOULDBLOCK)
			return 0; // Then socket was marked as non-blocking and there was no data to be read.
		
		throw UDPSocketExcep("error while reading from socket.  Error code == " + Networking::getError());
#endif
	}

	UDPSocket::num_bytes_rcvd += numbytesrcvd;


	//-----------------------------------------------------------------
	//get the sender IP and port
	//-----------------------------------------------------------------
	sender_ip_out = IPAddress(from_address.sin_addr.s_addr);//NEWCODE removed htonl

	senderport_out.setPort( ntohs(from_address.sin_port) );

	
	//-----------------------------------------------------------------
	//get the interface address of this host used for the connection
	//-----------------------------------------------------------------
	struct sockaddr_in interface_addr;
	SOCKLEN_TYPE length = sizeof(interface_addr);

	memset(&interface_addr, 0, length);
	const int result = getsockname(socket_handle, (struct sockaddr*)&interface_addr, &length);

	if(!isSocketError(result)) // result != SOCKET_ERROR)
	{
		thisend_ip_out = interface_addr.sin_addr.s_addr; //NEWCODE removed htonl		
	}
	else
	{
		//::debugPrint("failed to determine ip addr used for UDPSocket socket.");
	}

	

	return numbytesrcvd;
}


void UDPSocket::readPacket(Packet& packet_out, IPAddress& sender_ip_out, Port& senderport_out, 
							IPAddress& thisend_ip_out)
{
	char data[1024];
	const int num_bytes_read = readPacket(data, 1024, sender_ip_out, senderport_out, thisend_ip_out);


	for(int i=0; i<num_bytes_read; ++i)
		packet_out.write(data[i]);	//NOTE: grossly inefficient
}


	//returns true if packet waiting
bool UDPSocket::pollForPacket(Packet& packet_out, IPAddress& sender_ip_out, 
										Port& senderport_out, IPAddress& thisend_ip_out)
{
	char data[1024];
	const int num_bytes_read = readPacket(data, 1024, sender_ip_out, senderport_out, 
															thisend_ip_out, false);

	if(num_bytes_read == 0)
		return false;

	for(int i=0; i<num_bytes_read; ++i)
		packet_out.write(data[i]);	//NOTE: grossly inefficient

	return true;
}

void UDPSocket::setBlocking(bool blocking)
{
#if defined(WIN32) || defined(WIN64)

	unsigned long b = blocking ? 0 : 1;

	const int result = ioctlsocket(socket_handle, FIONBIO, &b);

	assert(result != SOCKET_ERROR);
#else
	const int current_flags = fcntl(socket_handle, F_GETFL);
	int new_flags;
	if(blocking)
		new_flags = current_flags & ~O_NONBLOCK;
	else
		new_flags = current_flags | O_NONBLOCK;

	const int result = fcntl(socket_handle, F_SETFL, new_flags);
	assert(result != -1);
#endif
	if(isSocketError(result))
		throw UDPSocketExcep("setBlocking() failed. Error code: " + Networking::getError());
}


//-----------------------------------------------------------------
//funcs for measuring data rate
//-----------------------------------------------------------------
int UDPSocket::getNumBytesSent()
{
	return num_bytes_sent;
}
int UDPSocket::getNumBytesRcvd()
{
	return num_bytes_rcvd;
}

void UDPSocket::resetNumBytesSent()
{
	num_bytes_sent = 0;
}
void UDPSocket::resetNumBytesRcvd()
{
	num_bytes_rcvd = 0;
}

/*int UDPSocket::getCSPort() const
{
	return ::getIniFile().getIntForKey("udp_port");
}*/

bool UDPSocket::isSockHandleValid(SOCKETHANDLE_TYPE handle)
{
#if defined(WIN32) || defined(WIN64)
	return handle != INVALID_SOCKET;
#else
	return handle >= 0;
#endif
}


int UDPSocket::num_bytes_sent = 0;
int UDPSocket::num_bytes_rcvd = 0;
