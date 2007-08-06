/*=====================================================================
streamsysserver.cpp
-------------------
File created by ClassTemplate on Fri Sep 10 04:33:02 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "streamsysserver.h"


#include "streamsysclientproxy.h"
#include "serverstreamlistener.h"
#include <iostream>
#include <vector>
#include <assert.h>
#include "packet.h"
#include "serverlistener.h"


StreamSysServer::StreamSysServer(int listenport, ServerListener* server_listener_)
:	server_listener(server_listener_)
{
	try
	{
		socket.bindAndListen(listenport);//non-blocking
		socket.acceptConnection();
	}
	catch(PollSocketExcep& e)
	{
		throw StreamSysServerExcep("PollSocketExcep: " + e.what());
	}

	this->accepted_msg = "welcome to the server.";
}


StreamSysServer::~StreamSysServer()
{
	
}



void StreamSysServer::think()//poll for events
{


	//------------------------------------------------------------------------
	//process listen socket for new connections
	//------------------------------------------------------------------------
	try
	{
		socket.think();
	}
	catch(PollSocketExcep&)
	{
		//do something here?
	}


	if(socket.getState() == PollSocket::ACCEPTED)
	{
		PollSocket* workersocket = new PollSocket;
		socket.getWorkerSocket(*workersocket);

		//------------------------------------------------------------------------
		//create new client object
		//------------------------------------------------------------------------
		StreamSysClientProxy* client = new StreamSysClientProxy(workersocket);

		clients.insert(client);

		std::cout << "added client at " << client->getClientIPAddr().toString()
				<< ":" << client->getClientPort() << std::endl;


		//------------------------------------------------------------------------
		//transfer all object state to new client as create messages
		//------------------------------------------------------------------------
		/*std::vector<DistObject*> objects;
		listener->getLocalObjects(objects);

		for(int i=0; i<objects.size(); ++i)
		{
			CSMessage message;
			message.msg_id = CSMessage::CREATE_OBJECT;
			objects[i]->serialiseUID(message.object_id_packet);
			objects[i]->serialise(message.object_state);

			try
			{
				Packet p;
				message.serialise(p);
				client->sendPacket(p);
			}
			catch(PollSocketExcep&)
			{}
		}*/


		//------------------------------------------------------------------------
		//inform listeners of new client
		//------------------------------------------------------------------------
		/*if(server_listener)
			server_listener->clientJoined(*client);
		
		for(std::map<int, ServerStreamListener*>::iterator i = listeners.begin(); i != listeners.end(); ++i)
		{
			(*i).second->clientJoined(*client);
		}*/

		socket.acceptConnection();//go back to listening for a new connection

	}

	//------------------------------------------------------------------------
	//let clients think
	//------------------------------------------------------------------------
	std::vector<StreamSysClientProxy*> killist;

	for(std::set<StreamSysClientProxy*>::iterator i = clients.begin(); i != clients.end(); ++i)
	{
		try
		{
			(*i)->think(*this);
		}
		catch(StreamSysClientProxyExcep&)
		{
			std::cout << "removing client at " << (*i)->getClientIPAddr().toString()
				<< ":" << (*i)->getClientPort() << std::endl;

			killist.push_back(*i);

		}
	}

	for(int z=0; z<killist.size(); ++z)
	{
		//------------------------------------------------------------------------
		//inform listeners of client leaving
		//------------------------------------------------------------------------
		if(killist[z]->accepted())//if client was accepted
		{
			if(server_listener)
				server_listener->clientLeft(*killist[z]);

			for(std::map<int, ServerStreamListener*>::iterator i = listeners.begin(); i != listeners.end(); ++i)
				(*i).second->clientLeft(*killist[z]);
		}

		delete killist[z];

		clients.erase(killist[z]);
	}
}

void StreamSysServer::addStreamListener(int stream_uid, ServerStreamListener* listener)
{
	std::map<int, ServerStreamListener*>::iterator result = listeners.find(stream_uid);

	if(result == listeners.end())
	{
		listeners[stream_uid] = listener;
	}
	else
	{
		assert(0);//stream uid already used.
	}
}

void StreamSysServer::removeStreamListener(int stream_uid)
{
	listeners.erase(stream_uid);
}

void StreamSysServer::streamToAllClients(int stream_uid, Packet& packet)
{
	Packet wrapper_packet;
	wrapper_packet << stream_uid;
	packet.writeToStream(wrapper_packet);//write UNSIZED

	for(std::set<StreamSysClientProxy*>::iterator i = clients.begin(); i != clients.end(); ++i)
	{
		if((*i)->accepted())//if client has been accepted
		{
			try
			{
				(*i)->sendPacket(wrapper_packet);
			}
			catch(StreamSysClientProxyExcep&)
			{
			}
		}
	}
}

void StreamSysServer::streamToClient(StreamSysClientProxy& client, int stream_uid, 
									 Packet& packet)
{
	assert(client.accepted());//assert client has been accepted

	Packet wrapper_packet;
	wrapper_packet << stream_uid;
	packet.writeToStream(wrapper_packet);//write UNSIZED

	try
	{
		client.sendPacket(wrapper_packet);
	}
	catch(StreamSysClientProxyExcep&)
	{
	}

}

//called by client proxies:
void StreamSysServer::handlePacketFromClient(Packet& packet, 
											 StreamSysClientProxy& client)
{
	assert(client.accepted());//assert client has been accepted

	//read stream uid
	int stream_uid;
	packet >> stream_uid;

	std::map<int, ServerStreamListener*>::iterator result = listeners.find(stream_uid);

	if(result == listeners.end())
	{
		//no registered hanlder for this stream UID.
		assert(0);
	}
	else
	{
		(*result).second->handlePacket(stream_uid, packet, client);
	}

}


void StreamSysServer::clientAccepted(StreamSysClientProxy& client)
{
	if(server_listener)
		server_listener->clientJoined(client);
		
	for(std::map<int, ServerStreamListener*>::iterator i = listeners.begin(); i != listeners.end(); ++i)
	{
		(*i).second->clientJoined(client);
	}
}

bool StreamSysServer::connectionPermitted(StreamSysClientProxy& client,
								std::string& denied_msg_out)
{
	if(server_listener)
		return server_listener->connectionPermitted(client, denied_msg_out);
	else
		return true;
}


const std::string& StreamSysServer::getAcceptedMessage() const
{
	return accepted_msg;
}
void StreamSysServer::setAcceptedMessage(const std::string& msg)
{
	accepted_msg = msg;
}


