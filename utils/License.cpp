/*=====================================================================
License.cpp
-----------
File created by ClassTemplate on Thu Mar 19 14:06:32 2009
=====================================================================*/
#include "License.h"


#include "platformutils.h"
#include "fileutils.h"
#include "stringutils.h"
#include "clock.h"
#include "../indigo/TestUtils.h"
#include "../utils/timer.h"
#include "../indigo/globals.h"
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>


static const std::string PUBLIC_CERTIFICATE_DATA = "-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCg6Xnvoa8vsGURrDzW9stKxi9U\nuKXf4aUqFFrcxO6So9XKpygV4oN3nwBip3rGhIg4jbNbQrhAeicQhfyvATYenj6W\nBLh4X3GbUD/LTYqLNY4qQGsdt/BpO0smp4DPIVpvAPSOeY6424+en4RRnUrsNPJu\nuShWNvQTd0XRYlj4ywIDAQAB\n-----END PUBLIC KEY-----\n";

/*
Believe it or not, OpenSSL requires newline characters after every 64 characters, or at the end of the input if the total input len is < 64 chars.
So if they were inadvertantly removed, add them back in.
*/
const std::string License::ensureNewLinesPresent(const std::string& data)
{
	if(::getNumMatches(data, '\n') == 0) // If no newlines in input string
	{
		std::string s;
		for(size_t i=0; i<data.length(); i+=64)
			s += data.substr(i, 64) + "\n";
		return s;
	}
	else
		return data;
}


// From http://www.google.com/codesearch/p?hl=en#Q5tR35FJDOM/libopkele-0.2.1/lib/util.cc&q=decode_base64%20const%20string%20data%20lang:c%2B%2B

const std::string License::decodeBase64(const std::string& data_)
{
	const std::string data = ensureNewLinesPresent(data_);

    try {
    	BIO* bmem = BIO_new_mem_buf((void*)data.c_str(), data.size());
        if(!bmem)
			throw License::LicenseExcep("Failed to allocate in base64 decoder");

        BIO* b64 = BIO_new(BIO_f_base64());
        if(!b64)
            throw License::LicenseExcep("Failed to initialize in base64 decoder");

        BIO_push(b64, bmem);
        unsigned char tmp[512];
        size_t rb = 0;
		std::string rv;
        while((rb=BIO_read(b64, tmp, sizeof(tmp)))>0){ // Try and read bytes to buffer
            rv.insert(rv.end(), tmp, &tmp[rb]); // Append read bytes to rv
		}

		BIO_free_all(b64); // This seems to free up all used memory.
		return rv;
    }
	catch(...)
	{
        throw License::LicenseExcep("base64 decoder error");
    }
}


bool License::verifyKey(const std::string& key, const std::string& hash)
{
	ERR_load_crypto_strings();

	// Load the public key
	BIO* public_key_mbio = BIO_new_mem_buf((void *)PUBLIC_CERTIFICATE_DATA.c_str(), PUBLIC_CERTIFICATE_DATA.size());
	EVP_PKEY* public_key = PEM_read_bio_PUBKEY(public_key_mbio, NULL, NULL, NULL);

	// Initialize OpenSSL and pass in the data
	EVP_MD_CTX ctx;
    if(EVP_VerifyInit(&ctx, EVP_sha1()) != 1)
		throw LicenseExcep("Internal Verification failure 1.");

    if(EVP_VerifyUpdate(&ctx, key.data(), key.size()) != 1)
		throw LicenseExcep("Internal Verification failure 2.");

	// Call the actual verify function and get result
	const int result = EVP_VerifyFinal(&ctx, (const unsigned char*)hash.data(), hash.size(), public_key);

	EVP_MD_CTX_cleanup(&ctx);
	
	// Free the public key
	EVP_PKEY_free(public_key);
	BIO_free_all(public_key_mbio);

	ERR_free_strings();

	if(result == 1) // Correct signature
	{
		return true;
	}
	else if(result == 0) // Incorrect signature
	{
		return false;
	}
	else // Failure (other error)
		throw LicenseExcep("Verification failed.");
}


void License::verifyLicense(const std::string& indigo_base_path, LicenceType& license_type_out, std::string& user_id_out)
{
	license_type_out = UNLICENSED;

	const std::string hwinfo = getHardwareIdentifier();

	// Load the signature and decode from base64
	if(!FileUtils::fileExists(FileUtils::join(indigo_base_path, "licence.sig")))
		return;

	std::string hash;
	std::string constructed_key; // = "User ID;Licence Type;Hardware Key"
	LicenceType desired_license_type = UNLICENSED;
	try
	{
		std::string sigfile_contents;
		FileUtils::readEntireFile(FileUtils::join(indigo_base_path, "licence.sig"), sigfile_contents);

		if(sigfile_contents.empty())
			return; //throw LicenseExcep("Signature empty.");

		// Split the license key up into (User Id, license type, sig)
		const std::vector<std::string> components = ::split(sigfile_contents, ';');

		if(components.size() != 3)
			return;

		user_id_out = components[0];

		if(components[1] == "indigo-full-2.x")
			desired_license_type = FULL;
		else if(components[1] == "indigo-beta-2.x")
			desired_license_type = BETA;
		else if(components[1] == "indigo-node-2.x")
			desired_license_type = NODE;
		else if(components[1] == "indigo-full-lifetime")
			desired_license_type = FULL_LIFETIME;
		else
			return;

		hash = decodeBase64(components[2]);

		if(hash.empty())
			return; // throw LicenseExcep("Signature empty.");

		constructed_key = components[0] + ";" + components[1] + ";" + hwinfo;
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw LicenseExcep(e.what());
	}

	if(verifyKey(constructed_key, hash))
	{
		license_type_out = desired_license_type;
	}
	else
	{
		assert(license_type_out == UNLICENSED);
	}
}


const std::string License::getHardwareIdentifier()
{
	try
	{
		std::vector<std::string> MAC_addresses;
		PlatformUtils::getMACAddresses(MAC_addresses);

		if(MAC_addresses.empty())
			throw LicenseExcep("No MAC addresses found.");

		PlatformUtils::CPUInfo cpuinfo;
		PlatformUtils::getCPUInfo(cpuinfo);

		return std::string(cpuinfo.proc_brand) + ":" + MAC_addresses[0];
	}
	catch(PlatformUtils::PlatformUtilsExcep& e)
	{
		throw LicenseExcep(e.what());
	}
}


// Beta period ends 2009/May/31
static bool isCurrentDayInBetaPeriod()
{
	int day, month, year;
	Clock::getCurrentDay(day, month, year);
	// Month is zero based, so May = 4
	return year <= 2009 && month <= 4 && day <= 31;
}


const std::string License::licenseTypeToString(LicenceType t)
{
	if(t == UNLICENSED)
		return "Unlicensed";
	else if(t == FULL)
		return "Indigo 2.x Full";
	else if(t == BETA)
		return std::string("Indigo 2.x Beta ") + (isCurrentDayInBetaPeriod() ? std::string("(Valid until 31st May 2009)") : std::string("(Expired on 1st June 2009)"));
	else if(t == NODE)
		return "Indigo 2.x Node";
	else if(t == FULL_LIFETIME)
		return "Indigo Full Lifetime";
	else
		return "[Unknown]";
}


bool License::shouldApplyWatermark(LicenceType t)
{
	if(t == UNLICENSED)
		return true;
	else if(t == FULL)
		return false;
	else if(t == NODE)
		return true; // Nodes just send stuff over the network, so if used as a standalone, will apply watermarks.
	else if(t == BETA)
	{
		return !isCurrentDayInBetaPeriod(); // Apply watermark if not in beta period.
	}
	else if(t == FULL_LIFETIME)
		return false;
	else
	{
		assert(0);
		return true;
	}
}


bool License::shouldApplyResolutionLimits(LicenceType t)
{
	if(t == UNLICENSED)
		return true;
	else if(t == FULL)
		return false;
	else if(t == NODE)
		return false; // Nodes shouldn't have resolution limits, 'cause they have to do full-res renders
	else if(t == BETA)
	{
		return !isCurrentDayInBetaPeriod(); // Apply res limits if not in beta period.
	}
	else if(t == FULL_LIFETIME)
		return false;
	else
	{
		assert(0);
		return true;
	}
}


const std::string License::currentLicenseSummaryString(const std::string& indigo_base_dir_path)
{
	LicenceType licence_type = License::UNLICENSED;
	std::string licence_user_id;
	try
	{
		License::verifyLicense(indigo_base_dir_path, licence_type, licence_user_id);
	}
	catch(License::LicenseExcep&)
	{}

	if(licence_type != License::UNLICENSED)
		return "Licence verified, licence type: " + License::licenseTypeToString(licence_type) + ", licensed to '" + licence_user_id + "'";
	else
		return "Licence not verified, running in free mode.";
}


void License::test()
{
	// Test long base-64 encoded block, with no embedded newlines
	testAssert(decodeBase64("TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4=") 
		== "Man is distinguished, not only by his reason, but by this singular passion from other animals, which is a lust of the mind, that by a perseverance of delight in the continued and indefatigable generation of knowledge, exceeds the short vehemence of any carnal pleasure."
		);


	// Test long base-64 encoded block, with embedded newlines
	testAssert(decodeBase64("TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlz\nIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2Yg\ndGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGlu\ndWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRo\nZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4=")
		== "Man is distinguished, not only by his reason, but by this singular passion from other animals, which is a lust of the mind, that by a perseverance of delight in the continued and indefatigable generation of knowledge, exceeds the short vehemence of any carnal pleasure."
		);

	testAssert(decodeBase64("bGVhc3VyZS4=") == "leasure.");
	testAssert(decodeBase64("bGVhc3VyZS4=\n") == "leasure.");

	// Test a signed key
	Timer timer;

	const int N = 1000;
	for(unsigned int i=0; i<N; ++i)
	{
		const std::string encoded_hash = 
			"KFf0oXSpS2IfGa3pl6BCc1XRJr5hMcOf2ETb8xKMbI4yd+ACk7Qjy6bdy876h1ZTAaGjtQ9CWQlSD31uvRW+WO3fcuD90A/9U2JnNYKrMoV4YmCfbLuzduJ6mfRmAyHV3a9rIUELMdws7coDdwnpEQkl0rg8h2atFAXTmpmUm0Q=";

		const std::string hash = License::decodeBase64(encoded_hash);

		const std::string key = "someoneawesome@awesome.com<S. Awesome>;indigo-full-lifetime;              Intel(R) Pentium(R) D CPU 3.40GHz:00-25-21-7F-BB-3E";
		
		testAssert(License::verifyKey(key, hash));	
	}

	conPrint("Verification took on average " + toString(timer.elapsed() / N) + " s");
}
