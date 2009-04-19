/*=====================================================================
License.cpp
-----------
File created by ClassTemplate on Thu Mar 19 14:06:32 2009
=====================================================================*/
#include "License.h"


// Compile with:
// 	g++ verify.cc -o verify -lcrypto


#include "platformutils.h"
#include "fileutils.h"
#include "stringutils.h"
#include "clock.h"
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


// From http://www.google.com/codesearch/p?hl=en#Q5tR35FJDOM/libopkele-0.2.1/lib/util.cc&q=decode_base64%20const%20string%20data%20lang:c%2B%2B

static const std::string decodeBase64(const std::string& data) {
	//std::vector<unsigned char> rv;
    BIO *b64 = 0, *bmem = 0;
    //rv.clear();
	
    try {
		bmem = BIO_new_mem_buf((void*)data.c_str(), data.size());
        if(!bmem)
			throw License::LicenseExcep("Failed to allocate in base64 decoder");

        b64 = BIO_new(BIO_f_base64());
        if(!b64)
            throw License::LicenseExcep("Failed to initialize in base64 decoder");

        BIO_push(b64,bmem);
        unsigned char tmp[512];
        size_t rb = 0;
		std::string rv;
        while((rb=BIO_read(b64,tmp,sizeof(tmp)))>0){
            rv.insert(rv.end(), tmp, &tmp[rb]);
		}

        BIO_free_all(b64);

		return rv;
    }
	catch(...)
	{
        if(b64) BIO_free_all(b64);
        throw License::LicenseExcep("base64 decoder error");
    }
}


static EVP_PKEY* get_public_key(){
	std::string public_key = PUBLIC_CERTIFICATE_DATA; //slurp("public_key.pem");
	BIO *mbio = BIO_new_mem_buf((void *)public_key.c_str(), public_key.size());
	return PEM_read_bio_PUBKEY(mbio, NULL, NULL, NULL);
}


void License::verifyLicense(const std::string& indigo_base_path, LicenceType& license_type_out, std::string& user_id_out)
{
	license_type_out = UNLICENSED;

	ERR_load_crypto_strings();

	// Load the public key
	EVP_PKEY* public_key = get_public_key();
	

	const std::string hwinfo = getHardwareIdentifier();

	// Load the signature and decode from base64
	if(!FileUtils::fileExists(FileUtils::join(indigo_base_path, "licence.sig")))
		return;

	//std::vector<unsigned char> signature;
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

	// Initialize openssl and pass in the data
	EVP_MD_CTX ctx;
    if(EVP_VerifyInit(&ctx, EVP_sha1()) != 1)
		throw LicenseExcep("Internal Verification failure 1.");

    if(EVP_VerifyUpdate(&ctx, constructed_key.data(), constructed_key.size()) != 1)
		throw LicenseExcep("Internal Verification failure 2.");

	// Call the actual verify function and get result
	const int result = EVP_VerifyFinal(&ctx, (const unsigned char*)hash.c_str(), hash.size(), public_key);

	ERR_free_strings();

	if(result == 1)
	{
		license_type_out = desired_license_type;
		return;
	}
	else if(result == 0) // incorrect signature
		return;
	else // failure (other error)
		throw LicenseExcep("Verification failed.");
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


const std::string License::licenseTypeToString(LicenceType t)
{
	if(t == UNLICENSED)
		return "Unlicensed";
	else if(t == FULL)
		return "Indigo 2.x Full";
	else if(t == BETA)
		return "Indigo 2.x Beta (Valid until 31st May 2009)";
	else if(t == NODE)
		return "Indigo 2.x Node";
	else
		return "[Unknown]";
}


// Beta period ends 2009/May/31
static bool isCurrentDayInBetaPeriod()
{
	int day, month, year;
	Clock::getCurrentDay(day, month, year);
	// Month is zero based, so May = 4
	return year <= 2009 && month <= 4 && day <= 31;
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
	else
	{
		assert(0);
		return true;
	}
}
