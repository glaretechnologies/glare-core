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
#include "SystemInfo.h"
#include "../indigo/TestUtils.h"
#include "Exception.h"
#include "timer.h"
#include "Checksum.h"
#include "../indigo/globals.h"
#include "Transmungify.h"
#include "X509Certificate.h"

#define USE_OPENSSL 1
#if USE_OPENSSL
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#endif 

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <limits>


//#define NO_HARDWARE_ID_SDK_LICENSING 1


//static const std::string PUBLIC_CERTIFICATE_DATA = "-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCg6Xnvoa8vsGURrDzW9stKxi9U\nuKXf4aUqFFrcxO6So9XKpygV4oN3nwBip3rGhIg4jbNbQrhAeicQhfyvATYenj6W\nBLh4X3GbUD/LTYqLNY4qQGsdt/BpO0smp4DPIVpvAPSOeY6424+en4RRnUrsNPJu\nuShWNvQTd0XRYlj4ywIDAQAB\n-----END PUBLIC KEY-----\n";


// We encrypt the public key instead of storing it in plain text, so that it's harder to locate in the executable,
//  so that it's not so easy to overwrite with an attacker's public key.
static uint32 encrypted_public_key_size = 69;
static uint32 encrypted_public_key[] = {
	2830347586u,	2159265384u,	2778190321u,	2307638886u,	2325586010u,	2827464002u,	1270066530u,	3751267170u,
	2124394588u,	2123231468u,	2459428445u,	2207237470u,	2224018782u,	2276379486u,	2208220934u,	2207239944u,
	3733964619u,	3481989648u,	3481253900u,	1939461391u,	1855913558u,	2322461721u,	1886894503u,	2104217339u,
	1890509070u,	3550267900u,	2406142700u,	2104605028u,	3732076779u,	2276389708u,	2205481734u,	3547510632u,
	3732588877u,	2272847359u,	3985634910u,	3799771374u,	3699042827u,	2090849274u,	2406804984u,	2312685065u,
	2460932200u,	2172704080u,	2090259470u,	2090259021u,	2121257228u,	2793337951u,	2507330316u,	2440294493u,
	1871890705u,	1970061548u,	2089936203u,	2444146244u,	2440819183u,	1888547343u,	1970131559u,	3496775148u,
	2272975627u,	3784299601u,	2089473541u,	3698897944u,	2224538606u,	1268762178u,	2830347586u,	2174472769u,
	2207438181u,	2778187620u,	2828785986u,	2830347687u,	3854933760u
};

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


#if USE_OPENSSL
// Modified from http://www.google.com/codesearch/p?hl=en#Q5tR35FJDOM/libopkele-0.2.1/lib/util.cc&q=decode_base64%20const%20string%20data%20lang:c%2B%2B

const std::string License::decodeBase64(const std::string& data_)
{
	const std::string data = ensureNewLinesPresent(data_);

	if(data.size() > (size_t)std::numeric_limits<int>::max())
		throw License::LicenseExcep("Data too big");

    try {
    	BIO* bmem = BIO_new_mem_buf((void*)data.c_str(), (int)data.size());
        if(!bmem)
			throw License::LicenseExcep("Failed to allocate in base64 decoder");

        BIO* b64 = BIO_new(BIO_f_base64());
        if(!b64)
            throw License::LicenseExcep("Failed to initialize in base64 decoder");

		// Append bmem to the chain
        BIO_push(b64, bmem);
        unsigned char tmp[512];
        size_t rb = 0;
		std::string rv;
        while((rb=BIO_read(b64, tmp, sizeof(tmp)))>0){ // Try and read bytes to buffer
            rv.insert(rv.end(), tmp, &tmp[rb]); // Append read bytes to rv
		}

		//BIO_pop(bmem);
		//BIO_free_all(bmem);

		BIO_free_all(b64); // This seems to free up all used memory.
		
		return rv;
    }
	catch(...)
	{
        throw License::LicenseExcep("base64 decoder error");
    }
}
#endif // USE_OPENSSL


// throws Indigo::Exception on failure
static INDIGO_STRONG_INLINE const std::string unTransmunfigyPublicKey()
{
	std::string s;
	const bool result = Transmungify::decrypt(
		encrypted_public_key,
		encrypted_public_key_size,
		s
	);

	assert(result);

	return s;
}


/*
Verify that the hash is a public signature of key.

*/
bool License::verifyKey(const std::string& key, const std::string& hash)
{
	if(hash.size() > std::numeric_limits<unsigned int>::max())
		throw License::LicenseExcep("Data too big");

	const std::string public_key_str = unTransmunfigyPublicKey();

#if USE_OPENSSL
	ERR_load_crypto_strings(); // NOTE: This seems to leak memory over multiple calls, see http://readlist.com/lists/openssl.org/openssl-users/0/394.html

	// Load the public key
	BIO* public_key_mbio = BIO_new_mem_buf((void*)public_key_str.c_str(), (int)public_key_str.size()); //(void *)PUBLIC_CERTIFICATE_DATA.c_str(), (int)PUBLIC_CERTIFICATE_DATA.size());
	EVP_PKEY* public_key = PEM_read_bio_PUBKEY(public_key_mbio, NULL, NULL, NULL);

	assert(public_key);

	// Initialize OpenSSL and pass in the data
	EVP_MD_CTX ctx;
    if(EVP_VerifyInit(&ctx, EVP_sha1()) != 1)
		throw LicenseExcep("Internal Verification failure 1.");

    if(EVP_VerifyUpdate(&ctx, key.data(), key.size()) != 1)
		throw LicenseExcep("Internal Verification failure 2.");

	// Call the actual verify function and get result
	const int result = EVP_VerifyFinal(&ctx, (const unsigned char*)hash.data(), (unsigned int)hash.size(), public_key);

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

#else // #if USE_OPENSSL
	return false;
#endif // #if USE_OPENSSL
}


void License::verifyLicense(const std::string& appdata_path, LicenceType& licence_type_out, std::string& user_id_out)
{
	licence_type_out = UNLICENSED; // License type out is unlicensed, unless proven otherwise.
	user_id_out = "";


	// TEMP check for the test certificate GreenButton sent us
	const std::string greenbutton_cert_subj = "GreenButton for Indigo";
	const std::string greenbutton_cert_pubkey = "3082010a0282010100ba9de4caa44d4d2edaf43716024c07584bfab4403c590a050f4687c56c3884f273b7f94e86746b5bce2b4816d13d4fd4d0644d88f98344c559e4159ecd044b11077f3c75adffddb8811b3ec0bdedd29b9411d84f85febf42c8c6c2ac08ec6187ebdd9bf049090af3395eab8d8fc4aa621cea52200f5996130a22e2eda33879ba8e8f72778125a709079ca84456694e2d792f340009d4d87e9343b4dce4fca72f12aff86964d1eeb090b6e959c2d34ced33aec996a16c7bec2843f4e014c77ce0c40d465e52239eb6d0e231c071c2710c3162d69f54726e02de2b51098ffcf931cfa6f5ee1bbbdf498b81bda54ff8f6a188b3bbe7026670079c04659621a6aa010203010001";

#ifdef _WIN32
	if(X509Certificate::verifyCertificate("Root", greenbutton_cert_subj, greenbutton_cert_pubkey) ||
		X509Certificate::verifyCertificate("My", greenbutton_cert_subj, greenbutton_cert_pubkey))
	{
		licence_type_out = GREENBUTTON_CLOUD;
		user_id_out = "GreenButton";
		return;
	}
	//else
	//	conPrint("Could not find the GreenButton certificate");
#endif


#if USE_OPENSSL

	// Try and verifiy network licence first.
	const bool have_net_floating_licence = tryVerifyNetworkLicence(appdata_path, licence_type_out, user_id_out);
	if(have_net_floating_licence)
		return;
	else
	{
		licence_type_out = UNLICENSED; // License type out is unlicensed, unless proven otherwise.
		user_id_out = "";
	}

#ifdef NO_HARDWARE_ID_SDK_LICENSING
	/*std::string hardware_id;
	try
	{
		std::string licence_id;
		FileUtils::readEntireFile(FileUtils::join(appdata_path, "licence-id.txt"), licence_id);
		hardware_id = licence_id;
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw LicenseExcep(e.what());
	}*/

	//const std::string hardware_id = getHardwareIdentifier();
#else
	const std::vector<std::string> hardware_ids = getHardwareIdentifiers();
#endif

#ifdef NO_HARDWARE_ID_SDK_LICENSING
	const std::string licence_sig_path = "sdk-licence.txt";
#else
	const std::string licence_sig_path = "licence.sig";
#endif

	try
	{
		// Load the signature from disk and decode from base64
		const std::string licence_sig_full_path = FileUtils::join(appdata_path, licence_sig_path);
		if(!FileUtils::fileExists(licence_sig_full_path))
			return;

		std::string sigfile_contents;
		FileUtils::readEntireFile(licence_sig_full_path, sigfile_contents);

		if(sigfile_contents.empty())
			return; // throw LicenseExcep("Signature empty.");

		// Split the license key up into (User Id, license type, sig)
		const std::vector<std::string> components = ::split(sigfile_contents, ';');

		if(components.size() != 3)
			return;

		user_id_out = components[0];

		LicenceType desired_licence_type = UNLICENSED;

#ifdef INDIGO_RT
		if(components[1] == "indigo-rt-3.x")
			desired_licence_type = RT_3_X;
		else if(components[1] == "indigo-full-3.x")
			desired_licence_type = FULL_3_X;
		else if(components[1] == "indigo-full-lifetime")
			desired_licence_type = FULL_LIFETIME;
#else
		if(components[1] == "indigo-full-2.x")
			desired_licence_type = FULL_2_X;
		else if(components[1] == "indigo-beta-2.x")
			desired_licence_type = BETA_2_X;
		else if(components[1] == "indigo-node-2.x")
			desired_licence_type = NODE_2_X;
		else if(components[1] == "indigo-full-lifetime")
			desired_licence_type = FULL_LIFETIME;
		else if(components[1] == "indigo-sdk-2.x")
			desired_licence_type = SDK_2_X;
		else if(components[1] == "indigo-full-3.x")
			desired_licence_type = FULL_3_X;
		else if(components[1] == "indigo-node-3.x")
			desired_licence_type = NODE_3_X;
		else if(components[1] == "indigo-revit-3.x")
			desired_licence_type = REVIT_3_X;
		else if(components[1] == "indigo-rt-3.x")
			desired_licence_type = RT_3_X;
#endif
		else
			return;


#ifdef INDIGO_RT
		// The user may run Indigo RT with an Indigo Full licence, without restrictions.
#else
		// If this is Indigo full, and the user only has an Indigo RT licence, show them an appropriate error message:
		if(desired_licence_type == RT_3_X)
			throw LicenseExcep("The entered licence key is for Indigo RT. Please download and install Indigo RT from http://www.indigorenderer.com/download-indigo-rt");
#endif

		const std::string hash = decodeBase64(components[2]);

		if(hash.empty())
			return; // throw LicenseExcep("Signature empty.");

		// Go through each hardware id, and see if it can be verified against the signature.
		for(size_t i=0; i<hardware_ids.size(); ++i)
		{
			const std::string hardware_id = hardware_ids[i];

			//std::string hash;
			//std::string constructed_key; // = "User ID;Licence Type;Hardware Key"
			// Or, in the case of the SDK DLL,
			// = "Preamble and User ID;License Type"

			#ifdef NO_HARDWARE_ID_SDK_LICENSING
			constructed_key = components[0] + ";" + components[1];
			#else
			const std::string constructed_key = components[0] + ";" + components[1] + ";" + hardware_id;
			#endif

			if(verifyKey(constructed_key, hash))
			{
				// Key verified!
				licence_type_out = desired_licence_type;
				return; // We're done here, return
			}
			else
			{
				assert(licence_type_out == UNLICENSED);
			}
		}
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw LicenseExcep(e.what());
	}

#endif // #if USE_OPENSSL
}


const std::vector<std::string> License::getHardwareIdentifiers()
{
	try
	{	
		std::vector<std::string> MAC_addresses;
		SystemInfo::getMACAddresses(MAC_addresses);

		if(MAC_addresses.empty())
			throw LicenseExcep("No MAC addresses found.");

		PlatformUtils::CPUInfo cpuinfo;
		PlatformUtils::getCPUInfo(cpuinfo);

		std::vector<std::string> ids(MAC_addresses.size());
		for(size_t i=0; i<ids.size(); ++i)
			ids[i] = std::string(cpuinfo.proc_brand) + ":" + MAC_addresses[i];
		
		return ids;
	}
	catch(Indigo::Exception& e)
	{
		throw LicenseExcep(e.what());
	}
	catch(PlatformUtils::PlatformUtilsExcep& e)
	{
		throw LicenseExcep(e.what());
	}
}


const std::string License::getPrimaryHardwareIdentifier()
{
	const std::vector<std::string> ids = getHardwareIdentifiers();
	if(ids.empty())
		throw LicenseExcep("ids.empty()");
	return ids[0];
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
	else if(t == FULL_LIFETIME)
		return "Indigo Full Lifetime";

	else if(t == FULL_2_X)
		return "Indigo 2.x Full";
	else if(t == BETA_2_X)
		return std::string("Indigo 2.x Beta ") + (isCurrentDayInBetaPeriod() ? std::string("(Valid until 31st May 2009)") : std::string("(Expired on 1st June 2009)"));
	else if(t == NODE_2_X)
		return "Indigo 2.x Node";
	else if(t == SDK_2_X)
		return "Indigo 2.x SDK";

	else if(t == FULL_3_X)
		return "Indigo 3.x Full";
	else if(t == NODE_3_X)
		return "Indigo 3.x Node";

	else if(t == REVIT_3_X)
		return "Indigo for Revit 3.x Full";

	else if(t == NETWORK_FLOATING_FULL)
		return "Network Floating Full";
	else if(t == NETWORK_FLOATING_NODE)
		return "Network Floating Node";
	else if(t == RT_3_X)
		return "Indigo RT";
	else if(t == GREENBUTTON_CLOUD)
		return "Indigo Cloud Node";
	else
		return "[Unknown]";
}


const std::string License::licenseTypeToCodeString(LicenceType t)
{
	if(t == NETWORK_FLOATING_FULL)
		return "network-floating-full";
	else if(t == NETWORK_FLOATING_NODE)
		return "network-floating-node";
	else
		return "[Unknown]";
}


bool License::licenceIsForOldVersion(LicenceType t)
{
	if(t == FULL_2_X || t == NODE_2_X)
		return true; // we're at 3.0 now.

	return false;
}


bool License::shouldApplyWatermark(LicenceType t)
{
	if(t == UNLICENSED)
		return true;
	else if(t == FULL_2_X)
		return true; // we're at 3.0 now.
	else if(t == NODE_2_X)
		return true; // Nodes just send stuff over the network, so if used as a standalone, will apply watermarks.
	else if(t == BETA_2_X)
	{
		return !isCurrentDayInBetaPeriod(); // Apply watermark if not in beta period.
	}
	else if(t == FULL_LIFETIME)
		return false;
	else if(t == SDK_2_X)
		return true;
	else if(t == NETWORK_FLOATING_FULL)
		return false;
	else if(t == NETWORK_FLOATING_NODE)
		return false;
	else if(t == FULL_3_X)
		return false;
	else if(t == NODE_3_X)
		return true; // Nodes just send stuff over the network, so if used as a standalone, will apply watermarks.
	else if(t == REVIT_3_X)
		return false;
	else if(t == RT_3_X)
#ifdef INDIGO_RT
		return false;
#else
		return true;
#endif
	else if(t == GREENBUTTON_CLOUD)
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
	else if(t == FULL_LIFETIME)
		return false;
	else if(t == FULL_2_X)
		return true; // we're at 3.0 now, so apply res limits.
	else if(t == NODE_2_X)
		return true; // we're at 3.0 now, so apply res limits.
	else if(t == BETA_2_X)
	{
		return !isCurrentDayInBetaPeriod(); // Apply res limits if not in beta period.
	}
	else if(t == SDK_2_X)
		return true; // we're at 3.0 now, so apply res limits.
	else if(t == NETWORK_FLOATING_FULL)
		return false;
	else if(t == NETWORK_FLOATING_NODE)
		return false;
	else if(t == FULL_3_X)
		return false;
	else if(t == NODE_3_X)
		return false;
	else if(t == REVIT_3_X)
		return false;
	else if(t == RT_3_X)
#ifdef INDIGO_RT
		return false;
#else
		return true;
#endif
	else if(t == GREENBUTTON_CLOUD)
		return false;
	else
	{
		assert(0);
		return true;
	}
}


uint32 License::maxUnlicensedResolution()
{
	return 700000; // 0.7 MP
}
	

bool License::dimensionsExceedLicenceDimensions(LicenceType t, int width, int height)
{
	if(shouldApplyResolutionLimits(t))
		return width * height > (int)maxUnlicensedResolution();
	else
		return false;
}


const std::string License::currentLicenseSummaryString(const std::string& appdata_path)
{
	LicenceType licence_type = License::UNLICENSED;
	std::string licence_user_id;
	try
	{
		License::verifyLicense(appdata_path, licence_type, licence_user_id);
	}
	catch(License::LicenseExcep&)
	{}

	if(licence_type != License::UNLICENSED)
		return "Licence verified.  \nLicence type: " + License::licenseTypeToString(licence_type) + ".  \nLicensed to '" + licence_user_id + "'";
	else
		return "Licence not verified, running in free mode.";
}


bool License::tryVerifyNetworkLicence(const std::string& appdata_path, LicenceType& license_type_out, std::string& user_id_out)
{
	const std::vector<std::string> hardware_ids = getHardwareIdentifiers();

	
	try
	{
		// Load the signature from disk.
		const std::string licence_sig_filename = "network_licence.sig";

		const std::string licence_sig_full_path = FileUtils::join(appdata_path, licence_sig_filename);
		if(!FileUtils::fileExists(licence_sig_full_path))
			return false;

		std::string sigfile_contents;
		FileUtils::readEntireFile(licence_sig_full_path, sigfile_contents);

		if(sigfile_contents.empty())
			return false;

		// Split the license key up into (user_id, type, end_time, sig)
		const std::vector<std::string> components = ::split(sigfile_contents, ';');

		if(components.size() != 4)
			return false;

		user_id_out = components[0];

		LicenceType desired_license_type = UNLICENSED;

		if(components[1] == "network-floating-full")
			desired_license_type = NETWORK_FLOATING_FULL;
		else if(components[1] == "network-floating-node")
			desired_license_type = NETWORK_FLOATING_NODE;
		else
			return false;

		const uint64 end_time = ::stringToUInt64(components[2]);

		const std::string hash = components[3]; // decodeBase64(components[3]);
		if(hash.empty())
			return false;

		// Go through each hardware id, and see if it can be verified against the signature.
		for(size_t i=0; i<hardware_ids.size(); ++i)
		{
			const std::string hardware_id = hardware_ids[i];

			const std::string constructed_key = components[0] + ";" + components[1] + ";" + components[2] + ";" + hardware_id;

			if(networkFloatingHash(constructed_key) == hash)
			{
				// Are we in the licence period?
				if((uint64)::getSecsSince1970() < end_time)
				{
					// Key verified!
					license_type_out = desired_license_type;
					//TEMP:
					//std::cout << "getSecsSince1970():" << getSecsSince1970() << std::endl;
					//std::cout << "end_time:" << end_time << std::endl;
					//std::cout << "Network licence verified, until " << ((int)end_time - (int)::getSecsSince1970()) << " s from now." << std::endl;
					return true; // We're done here, return
				}
				else
					return false; // Licence has expired
			}
			else
			{
				assert(license_type_out == UNLICENSED);
			}
		}

		return false;
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw LicenseExcep(e.what());
	}
}


const std::string License::networkFloatingHash(const std::string& input)
{
	/*std::string out(input.size(), 0);
	for(unsigned int i=0; i<input.size(); ++i)
		out[i] = input[i] + 1;
	return out;*/

	const std::string x = input + "9fssa3tnjh";

	const uint32 crc = Checksum::checksum((void*)x.c_str(), x.size());

	return ::toHexString(crc);
}


void License::cleanup() // Cleans up / frees OpenSSL global state.
{
	CRYPTO_cleanup_all_ex_data();
}


#if (BUILD_TESTS)


void License::test()
{
	static const std::string PUBLIC_CERTIFICATE_DATA = "-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCg6Xnvoa8vsGURrDzW9stKxi9U\nuKXf4aUqFFrcxO6So9XKpygV4oN3nwBip3rGhIg4jbNbQrhAeicQhfyvATYenj6W\nBLh4X3GbUD/LTYqLNY4qQGsdt/BpO0smp4DPIVpvAPSOeY6424+en4RRnUrsNPJu\nuShWNvQTd0XRYlj4ywIDAQAB\n-----END PUBLIC KEY-----\n";

	const bool print_encrypted_public_key = false;
	if(print_encrypted_public_key)
	{
		std::vector<uint32> dst_dwords;
		const bool result = Transmungify::encrypt(PUBLIC_CERTIFICATE_DATA, dst_dwords);

		std::cout << "static uint32 encrypted_public_key_size = " + toString((uint64)dst_dwords.size()) + ";\n";
		std::cout << "static uint32 encrypted_public_key[] = {\n";
		for(size_t i=0; i<dst_dwords.size(); ++i)
		{
			std::cout << toString(dst_dwords[i]) << "u";
			if(i + 1 < dst_dwords.size())
				std::cout << ",";
			std::cout << "\n";
		}
		std::cout << "};\n";
	}


	// Make sure our un-transmungified public key is correct.
	const std::string decoded_pubkey = unTransmunfigyPublicKey();
	testAssert(decoded_pubkey == PUBLIC_CERTIFICATE_DATA);


#if USE_OPENSSL
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

	const int N = 3;
	for(unsigned int i=0; i<N; ++i)
	{
		const std::string encoded_hash = 
			"KFf0oXSpS2IfGa3pl6BCc1XRJr5hMcOf2ETb8xKMbI4yd+ACk7Qjy6bdy876h1ZTAaGjtQ9CWQlSD31uvRW+WO3fcuD90A/9U2JnNYKrMoV4YmCfbLuzduJ6mfRmAyHV3a9rIUELMdws7coDdwnpEQkl0rg8h2atFAXTmpmUm0Q=";

		const std::string hash = License::decodeBase64(encoded_hash);

		const std::string key = "someoneawesome@awesome.com<S. Awesome>;indigo-full-lifetime;              Intel(R) Pentium(R) D CPU 3.40GHz:00-25-21-7F-BB-3E";
		
		testAssert(License::verifyKey(key, hash));	
	}

	conPrint("Verification took on average " + toString(timer.elapsed() / N) + " s");

	{
		const std::string encoded_hash = 
			"Lb+20caATeQVHTMhWiFMbi6/VxVZ1QJlprdIJpZ2srLeQkmLSEtuqD0QN4xKj1PX\n" \
			"KWKyRb676fPCi+YEjlFljew5rGQTUCDtVMQ/lPXBTvKJnXRoJB9KRiCaBJgkK14u\n" \
			"B9YLu+uRFpupJ6wMn5Kx9mKIzXud6e4HpsuRPRn0sgk=";
	
		const std::string hash = License::decodeBase64(encoded_hash);
		
		const std::string key = "Ranch Computing, contact@ranchcomputing.com;indigo-full-2.x;Intel(R) Core(TM)2 Quad CPU    Q6600  @ 2.40GHz:00-1D-60-D8-D2-95";
		
		testAssert(License::verifyKey(key, hash));	
	}
#endif // USE_OPENSSL
}
#endif
