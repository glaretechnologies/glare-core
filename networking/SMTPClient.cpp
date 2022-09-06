/*=====================================================================
SMTPClient.cpp
--------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "SMTPClient.h"


#if USING_LIBRESSL


#include "MySocket.h"
#include "TLSSocket.h"
#include "../utils/ConPrint.h"
#include "../utils/Parser.h"
#include "../utils/Exception.h"
#include "../utils/Base64.h"
#include "../utils/Clock.h"
#include <tls.h>


SMTPClient::SMTPClient()
{}


SMTPClient::~SMTPClient()
{}


static int parseReply(Parser& parser)
{
	int code = 0;
	bool last_line = false; // Is this the last line of a multi-part reply?
	while(!last_line)
	{
		if(!parser.parseInt(code))
			throw glare::Exception("Failed to parse result code");

		if(parser.eof())
			throw glare::Exception("EOF after code.");

		if(parser.current() == '-')
		{
		}
		else if(parser.current() == ' ')
		{
			last_line = true;
		}
		else
			throw glare::Exception("Failed to parse char after code.");

		// Parse rest of line
		while(!parser.eof())
		{
			if(parser.currentPos() >= 1 && parser.prev() == '\r' && parser.current() == '\n')
			{
				parser.advance(); // advance to character at start of next line
				break;
			}

			parser.advance();
		}
	}
	return code;
}


static void parseCode(Parser& parser, int expected_code)
{
	const int code = parseReply(parser);
	if(code != expected_code)
		throw glare::Exception("Expected code " + toString(expected_code) + " from server, got code " + toString(code));
}


static const std::string base64Encode(const std::string& s)
{
	std::string encoded;
	Base64::encode(s.data(), s.size(), encoded);
	return encoded;
}


// NOTE: not at all sure this is correct.  The RFCs are very confusing.
// We probably want to allow Unicode email addresses, see https://tools.ietf.org/html/rfc6531
static const std::string escapeString(const std::string& s)
{
	std::string res;
	res.reserve(s.size() * 2);
	for(size_t i=0; i<s.size(); ++i)
	{
		const char c = s[i];
		// Is the character c a EBNF nonterminal <c>?
		// See https://tools.ietf.org/html/rfc821 page 30.
		// NOTE: we will let '.' and '@' through for now.
		if(c == '<' || c == '>'
			|| c == '(' || c == ')'
			|| c == '[' || c == ']'
			|| c == '\\' /*|| c == '.'*/ || c == ',' || c == ';' || c == ':' /*|| c == '@'*/ || c == '"'
			|| ((int)c >= 0 && (int)c <= 31) || ((int)c == 127)
			|| c == ' '
			)
		{
			// If it is not, quote the character.
			res.push_back('\\');
			res.push_back(c);
		}
		else
			res.push_back(c);
	}
	return res;
}


static bool isEmailAddressValid(const std::string& addr)
{
	return true;
}


static bool isNameValid(const std::string& name)
{
	return true;
}


void SMTPClient::sendEmail(const SendEmailArgs& args)
{
	// Check validity of email addresses and names.
	if(!isEmailAddressValid(args.from_email_addr))
		throw glare::Exception("From email address '" + args.from_email_addr + "' is not valid.");
	if(!isEmailAddressValid(args.to_email_addr))
		throw glare::Exception("To email address '" + args.to_email_addr + "' is not valid.");

	if(!isNameValid(args.from_name))
		throw glare::Exception("From name  '" + args.from_name + "' is not valid.");
	if(!isNameValid(args.to_name))
		throw glare::Exception("To name  '" + args.to_name + "' is not valid.");

	TLSConfig client_tls_config;

	tls_config_insecure_noverifycert(client_tls_config.config); // TEMP: try and work out how to remove this call.

	MySocketRef plain_sock = new MySocket(args.servername, 465);

	TLSSocketRef sock = new TLSSocket(plain_sock, client_tls_config.config, args.servername);

	// Get formatted date-time string, e.g. Tue, 15 January 2008 16:02:43 -0500
	const std::string cur_datetime_str = Clock::RFC822FormatedString();

	const std::string message = "EHLO mytestserver\r\n"
		"AUTH LOGIN\r\n" +
		base64Encode(args.username) + "\r\n" +
		base64Encode(args.password) + "\r\n"
		"MAIL FROM:<" + escapeString(args.from_email_addr) + ">\r\n" // from 
		"RCPT TO:<" + escapeString(args.to_email_addr) + ">\r\n" // to
		"DATA\r\n"
		"From: \"" + escapeString(args.from_name) + "\" <" + escapeString(args.from_email_addr) + ">\r\n"
		"To: \"" + escapeString(args.to_name) + "\" <" + escapeString(args.to_email_addr) + ">\r\n"
		"Date: " + cur_datetime_str + "\r\n"
		"Subject: " + args.subject + "\r\n"
		"\r\n" +
		args.contents + // TODO: need to escape single lines with "." to ".."
		"\r\n.\r\n"
		"QUIT\r\n";
		
	sock->write(message.data(), message.size());

	// Read reply
	std::string reply;
	reply.reserve(1024);
	while(1)
	{
		size_t write_pos = reply.size();
		const size_t max_read_amount = 512;
		reply.resize(write_pos + max_read_amount);
		size_t num_read = sock->readSomeBytes(&reply[0] + write_pos, max_read_amount);
		reply.resize(write_pos + num_read);
		if(num_read == 0)
			break;
	}

	// conPrint(reply);

	// Parse and process reply
	Parser parser(reply);
	parseCode(parser, 220); // Parse initial 220 hello
	parseCode(parser, 250); // Parse initial 250 response
	parseCode(parser, 334); // Read 334 VXNlcm5hbWU6 response (334 base64('Username'))
	parseCode(parser, 334); // Read 334 UGFzc3dvcmQ6 response (334 base64('Password'))
	parseCode(parser, 235); // Parse authentication successful reply
	parseCode(parser, 250); // Parse mail-from response
	parseCode(parser, 250); // Parse recp-to response
	parseCode(parser, 354); // Parse reply to DATA
	const int code = parseReply(parser);
	if(code == 250) // Server sent back 250 OK before the 221 BYTE
		parseCode(parser, 221); // Parse bye code.
	else if(code == 221) // Server sent byte code
	{
	}
	else
		throw glare::Exception("Received unexpected code " + toString(code));
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"


void SMTPClient::test()
{
	// Test escapeString
	{
		testStringsEqual(escapeString("aaa@aaa.com"), "aaa@aaa.com");
		testStringsEqual(escapeString("aaa@aa<.com"), "aaa@aa\\<.com");
		testStringsEqual(escapeString("aaa@aa>.com"), "aaa@aa\\>.com");
		testStringsEqual(escapeString("aaa@aa .com"), "aaa@aa\\ .com");
		testStringsEqual(escapeString("aaa@aa\\.com"), "aaa@aa\\\\.com");
	}

	// Test sending an email
	try
	{
		SendEmailArgs args;
		args.servername = "";
		args.username = "";
		args.password = "";
		args.from_name = "";
		args.from_email_addr = "";
		args.to_name = "";
		args.to_email_addr = "";
		args.subject = "Test message";
		args.contents = "Test contents.";

		//SMTPClient::sendEmail(args);
	}
	catch(glare::Exception& e)
	{
		conPrint("Test failed: " + e.what());
		assert(0);
	}
}


#endif // BUILD_TESTS


#endif // USING_LIBRESSL
