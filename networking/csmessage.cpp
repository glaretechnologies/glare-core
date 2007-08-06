/*=====================================================================
csmessage.cpp
-------------
File created by ClassTemplate on Sat Sep 04 13:33:50 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "csmessage.h"




CSMessage::CSMessage()
{
	msg_id = 0;
}


CSMessage::~CSMessage()
{
	
}


void CSMessage::serialise(MyStream& stream)
{
	//const int msg_id_int = msg_id;
	//stream << msg_id_int;
	stream << msg_id;

	object_id_packet.writeToStreamSized(stream);
	object_state.writeToStreamSized(stream);
}

void CSMessage::deserialise(MyStream& stream)
{
	//int msg_id_int;
	//stream >> msg_id_int;
	//msg_id = (MESSAGE_ID)msg_id_int;
	stream >> msg_id;

	object_id_packet.readFromStreamSize(stream);
	object_state.readFromStreamSize(stream);
}


CSMessage::ID_TYPE CSMessage::CHANGE_OBJECT_STATE = 0;
CSMessage::ID_TYPE CSMessage::CREATE_OBJECT = 1;
CSMessage::ID_TYPE CSMessage::DESTROY_OBJECT = 2;


