/*=====================================================================
csmessage.h
-----------
File created by ClassTemplate on Sat Sep 04 13:33:50 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __CSMESSAGE_H_666_
#define __CSMESSAGE_H_666_



class MyStream;
#include "packet.h"
#include "intobuid.h"



/*=====================================================================
CSMessage
---------
Internal class.
=====================================================================*/
class CSMessage
{
public:
	/*=====================================================================
	CSMessage
	---------
	
	=====================================================================*/
	CSMessage();

	~CSMessage();

	void serialise(MyStream& stream);
	void deserialise(MyStream& stream);

	/*enum MESSAGE_ID
	{
		CHANGE_OBJECT_STATE,
		CREATE_OBJECT,
		DESTROY_OBJECT
	};*/

	typedef unsigned char ID_TYPE;

	static ID_TYPE CHANGE_OBJECT_STATE;// = 0;
	static ID_TYPE CREATE_OBJECT;// = 1;
	static ID_TYPE DESTROY_OBJECT;// = 2;



	//MESSAGE_ID msg_id;
	ID_TYPE msg_id;
	Packet object_id_packet;//NOTE: get rid of this later

	//IntObUID object_id;

	Packet object_state;
};



#endif //__CSMESSAGE_H_666_




