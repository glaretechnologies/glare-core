/*=====================================================================
SMTPClient.h
------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include <string>


/*=====================================================================
SMTPClient
----------
Sends emails over a TLS connection with SMTP.
=====================================================================*/
class SMTPClient
{
public:
	SMTPClient();
	~SMTPClient();

	struct SendEmailArgs
	{
		std::string servername; // SMTP server name

		std::string username; // SMTP account username
		std::string password; // SMTP account password
		
		std::string from_name;
		std::string from_email_addr;
		
		std::string to_name;
		std::string to_email_addr;
		
		std::string subject; // email subject line
		std::string contents; // email contents
	};

	static void sendEmail(const SendEmailArgs& args); // Throws Indigo::Exception on failure.

	static void test();
};
