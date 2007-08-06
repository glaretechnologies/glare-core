/*=====================================================================
httpclient.h
------------
File created by ClassTemplate on Mon Jul 29 15:14:55 2002
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __HTTPCLIENT_H_666_
#define __HTTPCLIENT_H_666_



#include <string>
class FractionListener;


class HTTPClientExcep
{
public:
	HTTPClientExcep(const std::string& message_) : message(message_) {}
	HTTPClientExcep(const std::string& message_, const std::string reply_) : 
			message(message_), reply(reply_) {}

	~HTTPClientExcep(){}

	const std::string& what() const { return message; }
	const std::string& getReply() const { return reply; }
private:
	std::string message;

	std::string reply;
};


/*=====================================================================
HTTPClient
----------
downloads files from web using http protocol.
=====================================================================*/
class HTTPClient
{
public:
	/*=====================================================================
	HTTPClient
	----------
	
	=====================================================================*/
	HTTPClient();

	~HTTPClient();


	void getFile(const std::string& url, std::string& file_out, FractionListener* frac) throw (HTTPClientExcep);

private:
};



#endif //__HTTPCLIENT_H_666_




