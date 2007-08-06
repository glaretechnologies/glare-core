/*=====================================================================
substream.h
-----------
File created by ClassTemplate on Sat Sep 25 05:06:01 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __SUBSTREAM_H_666_
#define __SUBSTREAM_H_666_





/*=====================================================================
SubStream
---------

=====================================================================*/
class SubStream
{
public:
	/*=====================================================================
	SubStream
	---------
	
	=====================================================================*/
	SubStream(StreamSys& stream_sys, StreamListener& event_handler, int remote_port);

	~SubStream();


	//server side stuff:
	void bindAndListen(int local_port);

	bool isIncomingConnection();

	void acceptConnection(SubStream& worker_substream);

private:
	int remote_port;

};



#endif //__SUBSTREAM_H_666_




