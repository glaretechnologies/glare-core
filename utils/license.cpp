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

static std::vector<unsigned char> decode_base64(const std::string& data) {
	std::vector<unsigned char> rv;
    BIO *b64 = 0, *bmem = 0;
    rv.clear();
	
    try {
        bmem = BIO_new_mem_buf((void*)data.data(),data.size());
        if(!bmem)
            throw "failed to allocate in base64 decoder";
        b64 = BIO_new(BIO_f_base64());
        if(!b64)
            throw "failed to initialize in base64 decoder";
        BIO_push(b64,bmem);
        unsigned char tmp[512];
        size_t rb = 0;
        while((rb=BIO_read(b64,tmp,sizeof(tmp)))>0){
            rv.insert(rv.end(),tmp,&tmp[rb]);
		}
        BIO_free_all(b64);
    }catch(...) {
        if(b64) BIO_free_all(b64);
        throw;
    }
	
	return rv;
}


static EVP_PKEY* get_public_key(){
	std::string public_key = PUBLIC_CERTIFICATE_DATA; //slurp("public_key.pem");
	BIO *mbio = BIO_new_mem_buf((void *)public_key.c_str(), public_key.size());
	return PEM_read_bio_PUBKEY(mbio, NULL, NULL, NULL);
}


bool License::verifyLicense(const std::string& indigo_base_path)
{
	ERR_load_crypto_strings();

	// Load the public key
	EVP_PKEY* public_key = get_public_key();
	
	// Get the hardware info file
	/*std::string hwinfo;
	std::cout << "Loading hardware info.. ";
	try{
		hwinfo = slurp("license.txt");
	}catch(std::string err){
		std::cout << "failure (" << err << ")\n";
		return false;
	}
	std::cout << "ok\n";*/

	const std::string hwinfo = getHardwareIdentifier();

	// Load the signature and decode from base64
	if(!FileUtils::fileExists(FileUtils::join(indigo_base_path, "license.sig")))
		return false;

	std::vector<unsigned char> signature;
	try
	{
		FileUtils::readEntireFile(FileUtils::join(indigo_base_path, "license.sig"), signature);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw LicenseExcep(e.what());
	}

	if(signature.empty())
		throw LicenseExcep("Signature empty.");


	// Initialize openssl and pass in the data
	EVP_MD_CTX ctx;
    if(EVP_VerifyInit(&ctx, EVP_sha1()) != 1)
		throw LicenseExcep("Internal Verification failure.");

    if(EVP_VerifyUpdate(&ctx, hwinfo.data(), hwinfo.size()) != 1)
		throw LicenseExcep("Internal Verification failure.");

	// Call the actual verify function and get result
	const int result = EVP_VerifyFinal(&ctx, &signature[0], signature.size(), public_key);

    if(result == 1)
		return true;
	else if (result == 0) // incorrect signature
		return false;
	else // failure (other error)
		throw LicenseExcep("Verification failed.");

	ERR_free_strings();
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
