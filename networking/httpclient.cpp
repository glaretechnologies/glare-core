/*=====================================================================
httpclient.cpp
--------------
File created by ClassTemplate on Mon Jul 29 15:14:55 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "httpclient.h"

#include "../networking/mysocket.h"
#include "../networking/url.h"
#include "../utils/stringutils.h"
//#include "globals.h"

HTTPClient::HTTPClient()
{
	
}


HTTPClient::~HTTPClient()
{
	
}



void HTTPClient::getFile(const std::string& urlstring, std::string& file_out, 
								 FractionListener* frac)
{
	URL file_url(urlstring);

	try
	{
	MySocket stream(file_url.getHost(), 80);

	const std::string CRLF("\r\n");


	//std::string noslashfilename = file_url.getFile().substr(1, (int)file_url.getFile().size() - 1); 

	const std::string message = "GET " + file_url.getFile() + " " + "HTTP/1.0" + CRLF +
								"Host: " + file_url.getHost() + CRLF +
								"Connection: close" + CRLF + CRLF;

	stream.write(message.c_str(), message.length());


	//-----------------------------------------------------------------
	//read in entire http reply
	//-----------------------------------------------------------------
	std::string reply;
	//stream.readMessage(reply, frac);

	const std::string DOUBLE_CRLF = CRLF + CRLF;

	while(1)
	{
		//NOTE: break this while loop after a certain time?
		char c;
		stream.readTo(c);
		::concatWithChar(reply, c);


		if(::hasSuffix(reply, DOUBLE_CRLF))
		{
			break;
		}
	}




	//-----------------------------------------------------------------
	//search string for content length string
	//-----------------------------------------------------------------
	const std::string CONTENT_LENGTH = "Content-Length: ";


	const int pos = reply.find(CONTENT_LENGTH);
		
	if(pos == std::string::npos)
	{
		//content length is not actually required, so guess it from reply length

		file_out = "";
		try
		{
			while(1)
			{
				char c;
				stream.readTo(c);
				::concatWithChar(file_out, c);
			}
		}
		catch(MySocketExcep&)
		{
		}


		//throw HTTPClientExcep("could not find 'Content-Length: ' in http reply", reply);
	}
	else
	{

		//------------------------------------------------------------------------
		//parse the content length
		//------------------------------------------------------------------------
		int content_length =  -1;
		int lenstring_size = 0;

		for(int i=0; i<100; ++i)//assuming max string size = 100 chars
		{
			const int charindex = pos + CONTENT_LENGTH.size() + i;

			if(charindex >= reply.size())//check in bounds of reply
			{
				assert(0);
				throw HTTPClientExcep(" ");
			}

			const char c = reply[charindex];
			
			if(!::isNumeric(c))
			{
				if(lenstring_size == 0)
				{
					assert(0);
					throw HTTPClientExcep("");
				}

				const std::string lengthstring = reply.substr(pos + CONTENT_LENGTH.size(), lenstring_size);
				content_length = ::stringToInt(lengthstring);
				break;
			}

			++lenstring_size;
		}
	

		if(content_length == -1)
		{
			//if content length not found..
			throw HTTPClientExcep(" ");
		}
	
		assert(content_length > 0);
		if(content_length < 0)
			throw HTTPClientExcep("invalid content_length");

		//-----------------------------------------------------------------
		//go to where file starts: find double CRLF
		//-----------------------------------------------------------------
		int crlfpos = reply.find(CRLF + CRLF);

		if(crlfpos == std::string::npos)
			throw HTTPClientExcep(" no 'CRLF+CRLF' found in http reply");

		crlfpos += 4;//skip past the crlfs

		const int replysize = reply.size();
		if(crlfpos + content_length > replysize)
		{
			const int extra_needed = crlfpos + content_length - replysize;
			assert(extra_needed > 0);

			//-----------------------------------------------------------------
			//try and read more of the reply
			//-----------------------------------------------------------------
			std::string morereply;
			stream.readTo(morereply, extra_needed, frac);

			reply += morereply;

			//assert(0);
			//throw HTTPClientExcep("invalid content_length");
		}

		assert(reply.size() >= crlfpos + content_length);

		file_out = reply.substr(crlfpos, content_length);
	}

	/*const std::string CONTENT_LENGTH = "Content-Length";
	int content_length =  -1;
	std::string reply;

	while(1)
	{
		char c;

		try
		{
		stream >> c;
		}
		catch(MySocketExcep& e)
		{
			break;//NOTE: kinda dodgy
		}

		char st[2];
		st[0] = c;
		st[1] = 0;
		reply += std::string(st);

		if(::hasPostfix(reply, CONTENT_LENGTH))
		{
			//char seperator;
			//stream >> seperator;

			//::debugPrint("FOUND CONTENT LENGTH!");

	//		std::string tempstring;
	//		for(int z=0; z<100; ++z)
	//		{
	//			char st[2];
	//			stream >> st[0];
	//			st[1] = 0;
	//			tempstring += std::string(st);
	//		}

	//		::debugPrint("next 100 chars: " + tempstring);



			stream >> c;//read in possible ':'
			if(c != ':')
				::debugPrint("Warning, next char is not ':'");

			stream >> c;//read in space after ':'
			if(c != ' ')
				::debugPrint("Warning, next char is not ' '");


	//		while(isWhitespace(c))
	//		{
	//			stream >> c;
	//		}

			stream >> c;//read in first numeral
			if(!isNumeric(c))
				::debugPrint("Warning, next char is not numeric");

			std::string lengthstring;

			while(isNumeric(c))
			{
				char st[2];
				st[0] = c;
				st[1] = 0;
				lengthstring += std::string(st);

				stream >> c;
			}
			
			//::debugPrint("CONTENT LENGTH: " + lengthstring);
			
			if(lengthstring.size() >= 1)
				content_length = ::stringToInt(lengthstring);
		}
		else if(::hasPostfix(reply, CRLF + CRLF))
		{
			//-----------------------------------------------------------------
			//start reading in attached file
			//-----------------------------------------------------------------
			if(content_length != -1)
			{
				::debugPrint("reading in http attachment file of " + toString(content_length) + " Bytes...");

				file_out.resize(content_length);
				stream.readTo(file_out.begin(), content_length, frac);
			}
		}
	}

*/

	//stream.readMessage(reply);


	//TEMP
/*	::debugPrint("----------START OF REPLY---------");
	::debugPrint(reply);
	::debugPrint("----------END OF REPLY---------");

	//-----------------------------------------------------------------
	//go to where file starts: double CRLF
	//-----------------------------------------------------------------
	int pos = reply.find(CRLF + CRLF);

	if(pos == std::string::npos)
		throw HTTPClientExcep();

	pos += 4;//skip past it

	if(pos >= reply.length())
		return;

	file_out = reply.substr(pos, reply.length() - pos);*/

	}
	catch(MySocketExcep& excep)
	{
		throw HTTPClientExcep("Socket exception while getting file: " + excep.what());
	}
}


/*
void HTTPClient::getFile(const std::string& urlstring, std::string& file_out, 
								 FractionListener* frac)
{
	URL file_url(urlstring);

	try
	{
	MySocket stream(file_url.getHost(), 80);

	const std::string CRLF("\r\n");


	//std::string noslashfilename = file_url.getFile().substr(1, (int)file_url.getFile().size() - 1); 

	const std::string message = "GET " + file_url.getFile() + " " + "HTTP/1.0" + CRLF +
								"Host: " + file_url.getHost() + CRLF +
								"Connection: close" + CRLF + CRLF;

	stream.write(message.c_str(), message.length());


	const std::string CONTENT_LENGTH = "Content-Length";
	std::string reply;

	while(1)
	{
		char c;
		stream >> c;
		reply.push_back(c);

		if(reply.substr((int)reply.size() - 14


	stream.readMessage(reply);


	//TEMP
	//::debugPrint(reply);

	//-----------------------------------------------------------------
	//go to where file starts: double CRLF
	//-----------------------------------------------------------------
	int pos = reply.find(CRLF + CRLF);

	if(pos == std::string::npos)
		throw HTTPClientExcep();

	pos += 4;//skip past it

	if(pos >= reply.length())
		return;

	file_out = reply.substr(pos, reply.length() - pos);

	}
	catch(MySocketExcep& excep)
	{
		throw HTTPClientExcep();
	}
}



*/
