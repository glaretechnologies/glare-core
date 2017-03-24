/*===================================================================
Copyright Glare Technologies Limited 2012 -
File created by ClassTemplate on Thu Mar 19 14:06:32 2009
====================================================================*/
#include "License.h"


#include "PlatformUtils.h"
#include "FileUtils.h"
#include "StringUtils.h"
#include "Clock.h"
#include "SystemInfo.h"
#include "../indigo/TestUtils.h"
#include "Exception.h"
#include "Timer.h"
#include "Checksum.h"
#include "../indigo/globals.h"
#include "MyThread.h"
#include "Transmungify.h"
#include "Mutex.h"
#include "OpenSSL.h"

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/objects.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/crypto.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <limits>


//static const std::string PUBLIC_CERTIFICATE_DATA = "-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCg6Xnvoa8vsGURrDzW9stKxi9U\nuKXf4aUqFFrcxO6So9XKpygV4oN3nwBip3rGhIg4jbNbQrhAeicQhfyvATYenj6W\nBLh4X3GbUD/LTYqLNY4qQGsdt/BpO0smp4DPIVpvAPSOeY6424+en4RRnUrsNPJu\nuShWNvQTd0XRYlj4ywIDAQAB\n-----END PUBLIC KEY-----\n";


// We encrypt the public key instead of storing it in plain text, so that it's harder to locate in the executable,
//  so that it's not so easy to overwrite with an attacker's public key.
// Code for encryption is to be found in the tests.
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


static uint32 encrypted_online_public_key_size = 114;
static uint32 encrypted_online_public_key[] = {
	2830347586u,	2159265384u,	2778190321u,	2307638886u,	2325586010u,	2827464002u,	1270066530u,	2207762790u,
	2272837983u,	3564760841u,	2121656662u,	2204741726u,	2140128606u,	2191512414u,	2224011362u,	2207762780u,
	2189021022u,	2224014614u,	3935823877u,	3531518819u,	2307586553u,	2190735435u,	2292906919u,	2493433956u,
	3699244546u,	3749762039u,	3581335130u,	3902074703u,	3568031478u,	2863461389u,	2375071228u,	1869871615u,
	3498099728u,	3765220191u,	2627269894u,	3986473199u,	3985094751u,	2138435578u,	3800616715u,	1893250812u,
	3501177335u,	2192952400u,	3530548972u,	2121132812u,	3501583885u,	3483294448u,	2073881066u,	1890448997u,
	2157248004u,	2324948729u,	3967612185u,	1937372772u,	3463099920u,	2256458757u,	1888999449u,	3532230138u,
	2140663823u,	3920292336u,	2440095324u,	2225982798u,	2427513871u,	3482891773u,	1957481213u,	3481919754u,
	2507927628u,	2475744080u,	3865564767u,	1873262669u,	2309017366u,	3531397469u,	2207627274u,	1265810011u,
	2226304592u,	2628713290u,	1853940741u,	3748318223u,	3802658307u,	3868182270u,	1872214250u,	3467759369u,
	3581929978u,	2171323643u,	3583227407u,	2125583112u,	3701198105u,	3514356061u,	2393370092u,	3969175207u,
	3734226198u,	2125313638u,	3816745572u,	2323116299u,	3698249725u,	2307577079u,	2273901145u,	3549153003u,
	2205538907u,	3798914832u,	2624172795u,	2074916086u,	2226174544u,	3767388406u,	2645537801u,	3934976511u,
	3470324326u,	1956561246u,	2829160002u,	2830347610u,	2777990129u,	2307638886u,	2325586010u,	2827464002u,
	1270066498u,	3854933807u
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


// throws Indigo::Exception on failure
static INDIGO_STRONG_INLINE const std::string unTransmungifyPublicKey()
{
	std::string s;
	Transmungify::decrypt(
		encrypted_public_key,
		encrypted_public_key_size,
		s
	);
	return s;
}


// throws Indigo::Exception on failure
static INDIGO_STRONG_INLINE const std::string unTransmungifyOnlinePublicKey()
{
	std::string s;
	Transmungify::decrypt(
		encrypted_online_public_key,
		encrypted_online_public_key_size,
		s
	);
	return s;
}


/*
Calls verifyKey with the public key for fixed licenses. Needed because unTransmungifyPublicKey() is only available
in License.cpp and NetworkManagers LicenseChecker.cpp needs this fuction to check network floating licenses.
*/
bool License::verifyKey(const std::string& key, std::string& hash)
{
	return verifyKey(key, hash, unTransmungifyPublicKey());
}


/*
Verify that the hash is a public signature of key.

https://wiki.openssl.org/index.php/EVP_Signing_and_Verifying#Public_Key
*/
bool License::verifyKey(const std::string& key, std::string& hash, const std::string& public_key_str)
{
	assert(OpenSSL::isInitialised());

	if(hash.size() > std::numeric_limits<unsigned int>::max())
		throw License::LicenseExcep("Data too big");

	// Load the public key
	BIO* public_key_mbio = BIO_new_mem_buf((void*)public_key_str.c_str(), (int)public_key_str.size()); //(void *)PUBLIC_CERTIFICATE_DATA.c_str(), (int)PUBLIC_CERTIFICATE_DATA.size());
	EVP_PKEY* public_key = PEM_read_bio_PUBKEY(public_key_mbio, NULL, NULL, NULL);

	assert(public_key);

	// Initialize OpenSSL and pass in the data
	// Create the Message Digest Context
	EVP_MD_CTX *mdctx = EVP_MD_CTX_create();

	if(!mdctx)
		throw LicenseExcep("Internal Verification failure 3.");

	if(EVP_DigestVerifyInit(mdctx, NULL, EVP_sha1(), NULL, public_key) != 1)
		throw LicenseExcep("Internal Verification failure 4.");

	if(EVP_DigestVerifyUpdate(mdctx, key.data(), key.size()) != 1)
		throw LicenseExcep("Internal Verification failure 5.");

	// Call the actual verify function and get result
	const int result = EVP_DigestVerifyFinal(mdctx, (unsigned char*)hash.data(), hash.size());

	// Free context
	EVP_MD_CTX_destroy(mdctx);

	// Free the public key
	EVP_PKEY_free(public_key);
	BIO_free_all(public_key_mbio);

	//	ERR_clear_error();
	ERR_remove_state(0);

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


/*
Verify the licence located on disk at appdata_path/licence.sig against the current machine's hardware ID.
Checks the public signature of the licence key hash.
Sets licence_type_out and user_id_out based on the results of the verification.

Also checks for the presence of the Green Button certificate, as well as a network floating licence licence key cached on disk.
*/
void License::verifyLicense(const std::string& appdata_path, LicenceType& licence_type_out, std::string& user_id_out,
							LicenceErrorCode& local_err_code_out, LicenceErrorCode& network_lic_err_code_out)
{
	assert(OpenSSL::isInitialised());

	licence_type_out = UNLICENSED; // License type out is unlicensed, unless proven otherwise.
	user_id_out = "";
	local_err_code_out = LicenceErrorCode_NoError;
	network_lic_err_code_out = LicenceErrorCode_NoError;

	// Try and verifiy network licence first.
	const bool have_net_floating_licence = tryVerifyNetworkLicence(appdata_path, licence_type_out, user_id_out, network_lic_err_code_out);
	const bool have_online_licence = tryVerifyOnlineLicence(appdata_path, licence_type_out, user_id_out, network_lic_err_code_out);
	if(have_net_floating_licence || have_online_licence)
		return;
	else
	{
		licence_type_out = UNLICENSED; // License type out is unlicensed, unless proven otherwise.
		user_id_out = "";
	}

	// Get the hardware IDs for the current machine.
	const std::vector<std::string> hardware_ids = getHardwareIdentifiers();

	try
	{
		// Load the signature from disk and decode from base64
		const std::string licence_sig_path = "licence.sig";

		const std::string licence_sig_full_path = FileUtils::join(appdata_path, licence_sig_path);
		if(!FileUtils::fileExists(licence_sig_full_path))
		{
			local_err_code_out = LicenceErrorCode_LicenceSigFileNotFound;
			return;
		}

		std::string sigfile_contents;
		FileUtils::readEntireFile(licence_sig_full_path, sigfile_contents);

		verifyLicenceString(sigfile_contents, hardware_ids, licence_type_out, user_id_out, local_err_code_out);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		local_err_code_out = LicenceErrorCode_MiscFileExcep;
		throw LicenseExcep(e.what());
	}
}


/*
Verifies that the licence key is correctly signed with Glare's private key.  Also check that the signature matches that for at least one of the hardware IDs passsed in.

*/
void License::verifyLicenceString(const std::string& licence_string, const std::vector<std::string>& hardware_ids, LicenceType& licence_type_out, std::string& user_id_out, LicenceErrorCode& error_code_out)
{
	assert(OpenSSL::isInitialised());

	licence_type_out = UNLICENSED; // License type out is unlicensed, unless proven otherwise.
	user_id_out = "";
	error_code_out = LicenceErrorCode_NoError;

	if(licence_string.empty())
	{
		error_code_out = LicenceErrorCode_FileEmpty;
		return;
	}

	// Split the license key up into (User Id, license type, sig)
	const std::vector<std::string> components = ::split(licence_string, ';');

	if(components.size() != 3)
	{
		error_code_out = LicenceErrorCode_WrongNumComponents;
		return;
	}

	LicenceType desired_licence_type = UNLICENSED;

#ifdef INDIGO_RT
	if(components[1] == "indigo-rt-3.x")
		desired_licence_type = RT_3_X;
	else if(components[1] == "indigo-full-3.x") // The user may run Indigo RT with an Indigo Full licence, without restrictions.
		desired_licence_type = FULL_3_X;
	else if(components[1] == "indigo-full-lifetime")
		desired_licence_type = FULL_LIFETIME;
	else if(components[1] == "indigo-rt-iclone")
		desired_licence_type = INDIGO_RT_ICLONE;

	else if(components[1] == "indigo-rt-4.x")
		desired_licence_type = RT_4_X;
	else if(components[1] == "indigo-full-4.x")
		desired_licence_type = FULL_4_X;
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

	else if(components[1] == "indigo-full-4.x")
		desired_licence_type = FULL_4_X;
	else if(components[1] == "indigo-node-4.x")
		desired_licence_type = NODE_4_X;
	else if(components[1] == "indigo-revit-4.x")
		desired_licence_type = REVIT_4_X;
	else if(components[1] == "indigo-rt-4.x")
		desired_licence_type = RT_4_X;

	else if(components[1] == "indigo-rt-iclone")
		desired_licence_type = INDIGO_RT_ICLONE;
#endif
	else
	{
		error_code_out = LicenceErrorCode_BadLicenceType;
		return;
	}


#ifdef INDIGO_RT
	// The user may run Indigo RT with an Indigo Full licence, without restrictions.
#else
	// If this is Indigo full, and the user only has an Indigo RT licence, show them an appropriate error message:
	if(desired_licence_type == RT_3_X || desired_licence_type == INDIGO_RT_ICLONE || desired_licence_type == RT_4_X)
	{
		error_code_out = LicenceErrorCode_IndigoRTUsedForIndigoRenderer;
		return;
	}
#endif

	std::string hash = decodeBase64(components[2]);
	if(hash.empty())
	{
		error_code_out = LicenceErrorCode_EmptyHash;
		return;
	}

	const std::string public_key_str = unTransmungifyPublicKey();

	// Go through each hardware id, and see if it can be verified against the signature.
	for(size_t i=0; i<hardware_ids.size(); ++i)
	{
		// Try with the unmodified hardware ID.
		{
			const std::string hardware_id = hardware_ids[i];
			const std::string constructed_key = components[0] + ";" + components[1] + ";" + hardware_id; // = "User ID;Licence Type;Hardware Key"
			if(verifyKey(constructed_key, hash, public_key_str))
			{
				// Key verified!
				user_id_out = components[0]; // The user id is the first component.
				licence_type_out = desired_licence_type;
				error_code_out = LicenceErrorCode_NoError;
				return; // We're done here, return
			}
			else
			{
				assert(licence_type_out == UNLICENSED);
			}
		}


		// Try with the hardware ID with the leading whitespace stripped off
		{
			const std::string hardware_id = ::stripHeadWhitespace(hardware_ids[i]);
			const std::string constructed_key = components[0] + ";" + components[1] + ";" + hardware_id; // = "User ID;Licence Type;Hardware Key"
			if(verifyKey(constructed_key, hash, public_key_str))
			{
				// Key verified!
				user_id_out = components[0]; // The user id is the first component.
				licence_type_out = desired_licence_type;
				error_code_out = LicenceErrorCode_NoError;
				return; // We're done here, return
			}
			else
			{
				assert(licence_type_out == UNLICENSED);
			}
		}

		// Try with the hardware ID without whitespace stripping, but with lower-cased MAC address.
		{
			const std::string hardware_id = getLowerCaseMACAddrHardwareID(hardware_ids[i]);
			const std::string constructed_key = components[0] + ";" + components[1] + ";" + hardware_id; // = "User ID;Licence Type;Hardware Key"
			if(verifyKey(constructed_key, hash, public_key_str))
			{
				// Key verified!
				user_id_out = components[0]; // The user id is the first component.
				licence_type_out = desired_licence_type;
				error_code_out = LicenceErrorCode_NoError;
				return; // We're done here, return
			}
			else
			{
				assert(licence_type_out == UNLICENSED);
			}
		}


		// Try with the hardware ID with the leading whitespace stripped off, and with lower-cased MAC address
		{
			const std::string hardware_id = ::stripHeadWhitespace(getLowerCaseMACAddrHardwareID(hardware_ids[i]));
			const std::string constructed_key = components[0] + ";" + components[1] + ";" + hardware_id; // = "User ID;Licence Type;Hardware Key"
			if(verifyKey(constructed_key, hash, public_key_str))
			{
				// Key verified!
				user_id_out = components[0]; // The user id is the first component.
				licence_type_out = desired_licence_type;
				error_code_out = LicenceErrorCode_NoError;
				return; // We're done here, return
			}
			else
			{
				assert(licence_type_out == UNLICENSED);
			}
		}
	}

	error_code_out = LicenceErrorCode_NoHashMatch;
}


/*
Get the hardware IDs for the current machine.
There wil be a hardware ID for each different MAC address.

*/
const std::vector<std::string> License::getHardwareIdentifiers()
{
	assert(OpenSSL::isInitialised());

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
	assert(OpenSSL::isInitialised());

	const std::vector<std::string> ids = getHardwareIdentifiers();
	if(ids.empty())
		throw LicenseExcep("ids.empty()");
	return ids[0];
}


// Licence key will look like
// Intel(R) Core(TM) i7 CPU         920  @ 2.67GHz:00-24-1D-C9-51-98
const std::string License::getLowerCaseMACAddrHardwareID(const std::string& hardware_id)
{
	// Find location of ':':
	const size_t dotpos = hardware_id.find_last_of(':');
	if(dotpos == std::string::npos)
		return hardware_id;
	
	std::string newstr = hardware_id;
	
	for(size_t i=dotpos; i<hardware_id.size(); ++i)
		newstr[i] = ::toLowerCase(newstr[i]);
	
	return newstr;
}


// The MAC address used to be lower-case on OS X.  For backwards compatibility, this method exists to return the old lower-case MAC address primary hardware ID.
const std::string License::getOldPrimaryHardwareId()
{
#if defined(_WIN32)
	return getPrimaryHardwareIdentifier();
#elif defined(OSX)
	return getLowerCaseMACAddrHardwareID(getPrimaryHardwareIdentifier());
#else
	// Linux
	return getPrimaryHardwareIdentifier();
#endif
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
	else if(t == RT_3_X)
		return "Indigo RT 3.x";

	else if(t == FULL_4_X)
		return "Indigo 4.x Full";
	else if(t == NODE_4_X)
		return "Indigo 4.x Node";
	else if(t == REVIT_4_X)
		return "Indigo for Revit 4.x Full";
	else if(t == RT_4_X)
		return "Indigo RT 4.x";

	else if(t == NETWORK_FLOATING_FULL)
		return "Network Floating Full";
	else if(t == NETWORK_FLOATING_NODE)
		return "Network Floating Node";

	else if(t == GREENBUTTON_CLOUD)
		return "Indigo Cloud Node";
	else if(t == INDIGO_RT_ICLONE)
		return "Indigo RT for iClone";
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
	if(t == FULL_2_X || t == NODE_2_X || t == FULL_3_X || t == NODE_3_X || t == RT_3_X || t == REVIT_3_X)
		return true; // we're at 4.0 now.

	return false;
}


bool License::shouldApplyWatermark(LicenceType t)
{
#if BENCHMARK_BUILD
	return false;
#else

	if(t == UNLICENSED)
		return true;
	else if(t == FULL_2_X)
		return true; // we're at 4.0 now.
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
		return true; // we're at 4.0 now.
	else if(t == NODE_3_X)
		return true; // Nodes just send stuff over the network, so if used as a standalone, will apply watermarks.
	else if(t == REVIT_3_X)
		return true; // we're at 4.0 now.
	else if(t == RT_3_X)
		return true; // we're at 4.0 now.

	else if(t == FULL_4_X)
		return false;
	else if(t == NODE_4_X)
		return true; // Nodes just send stuff over the network, so if used as a standalone, will apply watermarks.
	else if(t == REVIT_4_X)
		return false;
	else if(t == RT_4_X)
#ifdef INDIGO_RT
		return false;
#else
		return true;
#endif

	else if(t == GREENBUTTON_CLOUD)
		return false;
	else if(t == INDIGO_RT_ICLONE)
#ifdef INDIGO_RT
		return false; // Although we return false here, additional checking (that the scene is exported from iClone) is done in MainWindow and IndigoDriver.
#else
		return true;
#endif
	else
	{
		assert(0);
		return true;
	}

#endif // !BENCHMARK_BUILD
}


bool License::shouldApplyResolutionLimits(LicenceType t)
{
#if BENCHMARK_BUILD
	return false;
#else

	if(t == UNLICENSED)
		return true;
	else if(t == FULL_LIFETIME)
		return false;
	else if(t == FULL_2_X)
		return true; // we're at 4.0 now, so apply res limits.
	else if(t == NODE_2_X)
		return true; // we're at 4.0 now, so apply res limits.
	else if(t == BETA_2_X)
	{
		return !isCurrentDayInBetaPeriod(); // Apply res limits if not in beta period.
	}
	else if(t == SDK_2_X)
		return true; // we're at 4.0 now, so apply res limits.
	else if(t == NETWORK_FLOATING_FULL)
		return false;
	else if(t == NETWORK_FLOATING_NODE)
		return false;

	else if(t == FULL_3_X)
		return true; // we're at 4.0 now, so apply res limits.
	else if(t == NODE_3_X)
		return true; // we're at 4.0 now, so apply res limits.
	else if(t == REVIT_3_X)
		return true; // we're at 4.0 now, so apply res limits.
	else if(t == RT_3_X)
		return true; // we're at 4.0 now, so apply res limits.

	else if(t == FULL_4_X)
		return false;
	else if(t == NODE_4_X)
		return false;
	else if(t == REVIT_4_X)
		return false;
	else if(t == RT_4_X)
#ifdef INDIGO_RT
		return false;
#else
		return true;
#endif

	else if(t == GREENBUTTON_CLOUD)
		return false;
	else if(t == INDIGO_RT_ICLONE)
#ifdef INDIGO_RT
		return false; // Although we return false here, additional checking (that the scene is exported from iClone) is done in MainWindow and IndigoDriver.
#else
		return true;
#endif
	else
	{
		assert(0);
		return true;
	}

#endif // !BENCHMARK_BUILD
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
	License::LicenceErrorCode local_error_code, network_error_code;
	try
	{
		License::verifyLicense(appdata_path, licence_type, licence_user_id, local_error_code, network_error_code);
	}
	catch(License::LicenseExcep&)
	{}

	if(licence_type != License::UNLICENSED)
		return "Licence verified.  \nLicence type: " + License::licenseTypeToString(licence_type) + ".  \nLicensed to '" + licence_user_id + "'";
	else
		return "Licence not verified, running in free mode.";
}


bool License::tryVerifyNetworkLicence(const std::string& appdata_path, LicenceType& license_type_out, std::string& user_id_out, LicenceErrorCode& error_code_out)
{
	const std::vector<std::string> hardware_ids = getHardwareIdentifiers();

	
	try
	{
		// Load the signature from disk.
		const std::string licence_sig_filename = "network_licence.sig";

		const std::string licence_sig_full_path = FileUtils::join(appdata_path, licence_sig_filename);
		if(!FileUtils::fileExists(licence_sig_full_path))
		{
			error_code_out = LicenceErrorCode_NetworkLicenceFileNotFound;
			return false;
		}

		std::string sigfile_contents;
		// NOTE: if we fail to read the file here (e.g. if it is opened in another thread), then we don't want to throw an exception (which can terminate the render),
		// but just return false, which allows for the licence to be re-checked in a couple of seconds.
		try
		{
			FileUtils::readEntireFile(licence_sig_full_path, sigfile_contents);
		}
		catch(FileUtils::FileUtilsExcep& )
		{
			error_code_out = LicenceErrorCode_FileReadFailed;
			return false;
		}

		if(sigfile_contents.empty())
		{
			error_code_out = LicenceErrorCode_FileEmpty;
			return false;
		}

		// Split the license key up into (user_id, type, end_time, sig)
		const std::vector<std::string> components = ::split(sigfile_contents, ';');

		if(components.size() != 4)
		{
			error_code_out = LicenceErrorCode_WrongNumComponents;
			return false;
		}

		user_id_out = components[0];

		LicenceType desired_license_type = UNLICENSED;

		if(components[1] == "network-floating-full")
			desired_license_type = NETWORK_FLOATING_FULL;
		else if(components[1] == "network-floating-node")
			desired_license_type = NETWORK_FLOATING_NODE;
		else
		{
			error_code_out = LicenceErrorCode_BadLicenceType;
			return false;
		}

		const uint64 end_time = ::stringToUInt64(components[2]);

		const std::string hash = components[3]; // decodeBase64(components[3]);
		if(hash.empty())
		{
			error_code_out = LicenceErrorCode_EmptyHash;
			return false;
		}

		// Go through each hardware id, and see if it can be verified against the signature.
		for(size_t i=0; i<hardware_ids.size(); ++i)
		{
			const std::string hardware_id = hardware_ids[i];

			const std::string constructed_key = components[0] + ";" + components[1] + ";" + components[2] + ";" + hardware_id;

			if(networkFloatingHash(constructed_key) == hash)
			{
				// Are we in the licence period?
				if((uint64)Clock::getSecsSince1970() < end_time)
				{
					// Key verified!
					license_type_out = desired_license_type;
					//TEMP:
					//std::cout << "getSecsSince1970():" << getSecsSince1970() << std::endl;
					//std::cout << "end_time:" << end_time << std::endl;
					//std::cout << "Network licence verified, until " << ((int)end_time - (int)::getSecsSince1970()) << " s from now." << std::endl;
					error_code_out = LicenceErrorCode_NoError;
					return true; // We're done here, return
				}
				else
				{
					error_code_out = LicenceErrorCode_LicenceExpired;
					return false; // Licence has expired
				}
			}
			else
			{
				assert(license_type_out == UNLICENSED);
			}
		}

		error_code_out = LicenceErrorCode_NoHashMatch;
		return false;
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		error_code_out = LicenceErrorCode_MiscFileExcep;
		throw LicenseExcep(e.what());
	}
}


bool License::tryVerifyOnlineLicence(const std::string& appdata_path, LicenceType& license_type_out, std::string& user_id_out, LicenceErrorCode& error_code_out)
{
	const std::vector<std::string> hardware_ids = getHardwareIdentifiers();


	try
	{
		// Load the signature from disk.
		const std::string licence_sig_filename = "online_licence.sig";

		const std::string licence_sig_full_path = FileUtils::join(appdata_path, licence_sig_filename);
		if(!FileUtils::fileExists(licence_sig_full_path))
		{
			error_code_out = LicenceErrorCode_OnlineLicenceFileNotFound;
			return false;
		}

		std::string sigfile_contents;
		// NOTE: if we fail to read the file here (e.g. if it is opened in another thread), then we don't want to throw an exception (which can terminate the render),
		// but just return false, which allows for the licence to be re-checked in a couple of seconds.
		try
		{
			FileUtils::readEntireFile(licence_sig_full_path, sigfile_contents);
		}
		catch(FileUtils::FileUtilsExcep&)
		{
			error_code_out = LicenceErrorCode_FileReadFailed;
			return false;
		}

		if(sigfile_contents.empty())
		{
			error_code_out = LicenceErrorCode_FileEmpty;
			return false;
		}

		// Split the license key up into (user_id, type, end_time, sig)
		const std::vector<std::string> components = ::split(sigfile_contents, ';');

		if(components.size() != 5)
		{
			error_code_out = LicenceErrorCode_WrongNumComponents;
			return false;
		}

		user_id_out = components[0];

		LicenceType desired_licence_type = UNLICENSED;

#ifdef INDIGO_RT
		if(components[1] == "indigo-full-lifetime")
			desired_licence_type = FULL_LIFETIME;
		else if(components[1] == "indigo-rt-4.x")
			desired_licence_type = RT_4_X;
		else if(components[1] == "indigo-full-4.x")
			desired_licence_type = FULL_4_X;
#else
		if(components[1] == "indigo-full-lifetime")
			desired_licence_type = FULL_LIFETIME;
		else if(components[1] == "indigo-full-4.x")
			desired_licence_type = FULL_4_X;
		else if(components[1] == "indigo-node-4.x")
			desired_licence_type = NODE_4_X;
		else if(components[1] == "indigo-revit-4.x")
			desired_licence_type = REVIT_4_X;
		else if(components[1] == "indigo-rt-4.x")
			desired_licence_type = RT_4_X;
#endif
		else
		{
			error_code_out = LicenceErrorCode_BadLicenceType;
			return false;
		}

		const uint64 start_time = ::stringToUInt64(components[2]);
		const uint64 end_time = ::stringToUInt64(components[3]);

		std::string hash = decodeBase64(components[4]);
		if(hash.empty())
		{
			error_code_out = LicenceErrorCode_EmptyHash;
			return false;
		}

		const std::string public_key_str = unTransmungifyOnlinePublicKey();

		// Go through each hardware id, and see if it can be verified against the signature.
		for(size_t i = 0; i<hardware_ids.size(); ++i)
		{
			const std::string hardware_id = stripHeadAndTailWhitespace(hardware_ids[i]);
			const std::string constructed_key = components[0] + ";" + components[1] + ";" + components[2] + ";" + components[3] + ";" + hardware_id;
			if(verifyKey(constructed_key, hash, public_key_str))
			{
				// Are we in the licence period?
				if((uint64)Clock::getSecsSince1970() > start_time && (uint64)Clock::getSecsSince1970() < end_time)
				{
					// Key verified!
					license_type_out = desired_licence_type;
					//TEMP:
					//std::cout << "getSecsSince1970():" << getSecsSince1970() << std::endl;
					//std::cout << "end_time:" << end_time << std::endl;
					//std::cout << "Network licence verified, until " << ((int)end_time - (int)::getSecsSince1970()) << " s from now." << std::endl;
					error_code_out = LicenceErrorCode_NoError;
					return true; // We're done here, return
				}
				else
				{
					error_code_out = LicenceErrorCode_LicenceExpired;
					return false; // Licence has expired
				}
			}
			else
			{
				assert(license_type_out == UNLICENSED);
			}
		}

		error_code_out = LicenceErrorCode_NoHashMatch;
		return false;
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		error_code_out = LicenceErrorCode_MiscFileExcep;
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


#if BUILD_TESTS


class LicenseTestThread : public MyThread
{
public:
	LicenseTestThread() {}

	void run()
	{
		const std::string public_key_str = unTransmungifyPublicKey();

		const int N = 1000;
		for(unsigned int i=0; i<N; ++i)
		{
			const std::string encoded_hash = 
				"KFf0oXSpS2IfGa3pl6BCc1XRJr5hMcOf2ETb8xKMbI4yd+ACk7Qjy6bdy876h1ZTAaGjtQ9CWQlSD31uvRW+WO3fcuD90A/9U2JnNYKrMoV4YmCfbLuzduJ6mfRmAyHV3a9rIUELMdws7coDdwnpEQkl0rg8h2atFAXTmpmUm0Q=";

			std::string hash = License::decodeBase64(encoded_hash);

			const std::string key = "someoneawesome@awesome.com<S. Awesome>;indigo-full-lifetime;              Intel(R) Pentium(R) D CPU 3.40GHz:00-25-21-7F-BB-3E";

			testAssert(License::verifyKey(key, hash, public_key_str));
		}
	}
};


void License::warmup()
{
	{
		const std::string encoded_hash =
			"KFf0oXSpS2IfGa3pl6BCc1XRJr5hMcOf2ETb8xKMbI4yd+ACk7Qjy6bdy876h1ZTAaGjtQ9CWQlSD31uvRW+WO3fcuD90A/9U2JnNYKrMoV4YmCfbLuzduJ6mfRmAyHV3a9rIUELMdws7coDdwnpEQkl0rg8h2atFAXTmpmUm0Q=";

		std::string hash = License::decodeBase64(encoded_hash);

		const std::string key = "someoneawesome@awesome.com<S. Awesome>;indigo-full-lifetime;              Intel(R) Pentium(R) D CPU 3.40GHz:00-25-21-7F-BB-3E";

		const std::string public_key_str = unTransmungifyPublicKey();

		testAssert(License::verifyKey(key, hash, public_key_str));
	}
}


#include <iostream>


void License::test()
{
	conPrint("License::test()");

	// Test using OpenSSL functions from multiple threads concurrently.  This is important to test because this may be the case in in real-world usage of Indigo.
	{
		// Create the threads
		std::vector<Reference<LicenseTestThread> > threads(8);
		for(size_t i=0; i<threads.size(); ++i)
		{
			threads[i] = new LicenseTestThread();
			threads[i]->launch();
		}

		// Wait for threads to terminate
		for(size_t i=0; i<threads.size(); ++i)
			threads[i]->join();
	}


	const std::string PUBLIC_CERTIFICATE_DATA = "-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCg6Xnvoa8vsGURrDzW9stKxi9U\nuKXf4aUqFFrcxO6So9XKpygV4oN3nwBip3rGhIg4jbNbQrhAeicQhfyvATYenj6W\nBLh4X3GbUD/LTYqLNY4qQGsdt/BpO0smp4DPIVpvAPSOeY6424+en4RRnUrsNPJu\nuShWNvQTd0XRYlj4ywIDAQAB\n-----END PUBLIC KEY-----\n";
	/*const bool print_encrypted_public_key = false;
	if(print_encrypted_public_key)
	{
		std::vector<uint32> dst_dwords;
		const bool result = Transmungify::encrypt(PUBLIC_CERTIFICATE_DATA, dst_dwords);
		testAssert(result);

		//std::cout << "static uint32 encrypted_public_key_size = " + toString((uint64)dst_dwords.size()) + ";\n";
		//std::cout << "static uint32 encrypted_public_key[] = {\n";
		for(size_t i=0; i<dst_dwords.size(); ++i)
		{
			//std::cout << toString(dst_dwords[i]) << "u";
			//if(i + 1 < dst_dwords.size())
			//	std::cout << ",";
			//std::cout << "\n";
		}
		//std::cout << "};\n";
	}*/


	// Make sure our un-transmungified public key is correct.
	const std::string public_key_str = unTransmungifyPublicKey();
	testAssert(public_key_str == PUBLIC_CERTIFICATE_DATA);


	const std::string PUBLIC_KEY_ONLINE = "-----BEGIN PUBLIC KEY-----\nMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyIhkld/sNXvLXMxC68wM\nDH1KczymbTfZlRpENkm25SqY6t+tR2HcmvVbbDuov/eBJu9iFAhRY8hBkpFeZKcv\n3Uc16uZ+LC/qcsSsgGs+jutDWvOFfZU3rULNzEkMcKXfuixgrRKx2wodTOlIXUxZ\nseIpFr8flOrb4C3MA12l5rJ2vd6GQdWavuep03RW2/yRoB98V4BKLyfYsDK8Bup6\nF02A/4w95bdWlxLfr9rcnQCoaI9VU/KwhafpeuEDfM2pr/OGgEMyxjQtD9j7SNJi\nMEgy32GIdfbKiqKvtPydXULZZvN8UCrVkVBFtScow/9f65ZY26A/SIeY148hXvkb\nnwIDAQAB\n-----END PUBLIC KEY-----\n";
	/*const bool print_encrypted_public_key = false;
	if(print_encrypted_public_key)
	{
		std::vector<uint32> dst_dwords;
		Transmungify::encrypt(PUBLIC_KEY_ONLINE, dst_dwords);

		std::cout << "static uint32 encrypted_online_public_key_size = " + toString((uint64)dst_dwords.size()) + ";\n";
		std::cout << "static uint32 encrypted_online_public_key[] = {\n";
		for(size_t i = 0; i < dst_dwords.size(); ++i)
		{
			std::cout << toString(dst_dwords[i]) << "u";
			if(i + 1 < dst_dwords.size())
				std::cout << ",";
			std::cout << "\n";
		}
		std::cout << "};\n";
	}*/


	// Make sure our un-transmungified public key is correct.
	const std::string decoded_online_pubkey = unTransmungifyOnlinePublicKey();
	testAssert(decoded_online_pubkey == PUBLIC_KEY_ONLINE);


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





	// Test verifyKey.
	Timer timer;

	const int N = 3;
	for(unsigned int i=0; i<N; ++i)
	{
		const std::string encoded_hash = 
			"KFf0oXSpS2IfGa3pl6BCc1XRJr5hMcOf2ETb8xKMbI4yd+ACk7Qjy6bdy876h1ZTAaGjtQ9CWQlSD31uvRW+WO3fcuD90A/9U2JnNYKrMoV4YmCfbLuzduJ6mfRmAyHV3a9rIUELMdws7coDdwnpEQkl0rg8h2atFAXTmpmUm0Q=";

		std::string hash = License::decodeBase64(encoded_hash);

		const std::string key = "someoneawesome@awesome.com<S. Awesome>;indigo-full-lifetime;              Intel(R) Pentium(R) D CPU 3.40GHz:00-25-21-7F-BB-3E";
		
		testAssert(License::verifyKey(key, hash, public_key_str));
	}

	conPrint("License::verifyKey() verification took on average " + toString(timer.elapsed() / N) + " s");

	{
		const std::string encoded_hash = 
			"Lb+20caATeQVHTMhWiFMbi6/VxVZ1QJlprdIJpZ2srLeQkmLSEtuqD0QN4xKj1PX\n" \
			"KWKyRb676fPCi+YEjlFljew5rGQTUCDtVMQ/lPXBTvKJnXRoJB9KRiCaBJgkK14u\n" \
			"B9YLu+uRFpupJ6wMn5Kx9mKIzXud6e4HpsuRPRn0sgk=";

		std::string hash = License::decodeBase64(encoded_hash);

		const std::string key = "Ranch Computing, contact@ranchcomputing.com;indigo-full-2.x;Intel(R) Core(TM)2 Quad CPU    Q6600  @ 2.40GHz:00-1D-60-D8-D2-95";

		testAssert(License::verifyKey(key, hash, public_key_str));
	}

	// Test verifyKey returns false on invalid signature.
	{
		const std::string encoded_hash = 
			"66620caATeQVHTMhWiFMbi6/VxVZ1QJlprdIJpZ2srLeQkmLSEtuqD0QN4xKj1PX\n" \
			"KWKyRb676fPCi+YEjlFljew5rGQTUCDtVMQ/lPXBTvKJnXRoJB9KRiCaBJgkK14u\n" \
			"B9YLu+uRFpupJ6wMn5Kx9mKIzXud6e4HpsuRPRn0sgk=";

		std::string hash = License::decodeBase64(encoded_hash);

		const std::string key = "Ranch Computing, contact@ranchcomputing.com;indigo-full-2.x;Intel(R) Core(TM)2 Quad CPU    Q6600  @ 2.40GHz:00-1D-60-D8-D2-95";

		// NOTE: the line below still leaks memory due to stupid OpenSSL.  (at least with OpenSSL 0.9.8.x)
		testAssert(License::verifyKey(key, hash, public_key_str) == false);
	}




	// Test verifyLicenceString where it should succeed.
	try
	{
		std::vector<std::string> hardware_ids;
		hardware_ids.push_back("              Intel(R) Pentium(R) D CPU 3.40GHz:00-25-21-7F-BB-3E");

		// Original key is "someoneawesome@awesome.com<S. Awesome>;indigo-full-lifetime;              Intel(R) Pentium(R) D CPU 3.40GHz:00-25-21-7F-BB-3E"

		const std::string licence_key = 
			"someoneawesome@awesome.com<S. Awesome>;indigo-full-lifetime;KFf0oXSpS2IfGa3pl6BCc1XRJr5hMcOf2ETb8xKMbI4yd+ACk7Qjy6bdy876h1ZTAaGjtQ9CWQlSD31uvRW+WO3fcuD90A/9U2JnNYKrMoV4YmCfbLuzduJ6mfRmAyHV3a9rIUELMdws7coDdwnpEQkl0rg8h2atFAXTmpmUm0Q=";
	
		LicenceType licence_type = UNLICENSED;
		std::string user_id;
		LicenceErrorCode error_code;
		verifyLicenceString(licence_key, hardware_ids, licence_type, user_id, error_code);

		testAssert(user_id == "someoneawesome@awesome.com<S. Awesome>");
		testAssert(licence_type == FULL_LIFETIME);
		testAssert(error_code == LicenceErrorCode_NoError);
	}
	catch(LicenseExcep& e)
	{
		failTest(e.what());
	}

	// Test verifyLicenceString detects an invalid signature in the licence key.
	try
	{
		std::vector<std::string> hardware_ids;
		hardware_ids.push_back("              Intel(R) Pentium(R) D CPU 3.40GHz:00-25-21-7F-BB-3E");

		// NOTE: changed start of licence key to have a '666'.
		const std::string licence_key = 
			"someoneawesome@awesome.com<S. Awesome>;indigo-full-lifetime;6660oXSpS2IfGa3pl6BCc1XRJr5hMcOf2ETb8xKMbI4yd+ACk7Qjy6bdy876h1ZTAaGjtQ9CWQlSD31uvRW+WO3fcuD90A/9U2JnNYKrMoV4YmCfbLuzduJ6mfRmAyHV3a9rIUELMdws7coDdwnpEQkl0rg8h2atFAXTmpmUm0Q=";
	
		LicenceType licence_type = UNLICENSED;
		std::string user_id;
		LicenceErrorCode error_code;
		verifyLicenceString(licence_key, hardware_ids, licence_type, user_id, error_code);

		testAssert(user_id == "");
		testAssert(licence_type == UNLICENSED);
		testAssert(error_code == LicenceErrorCode_NoHashMatch);
	}
	catch(LicenseExcep& e)
	{
		failTest(e.what());
	}

	// Test verifyLicenceString detects a different licence type (indigo 3 full vs indigo lifetime) than was signed.
	try
	{
		std::vector<std::string> hardware_ids;
		hardware_ids.push_back("              Intel(R) Pentium(R) D CPU 3.40GHz:00-25-21-7F-BB-3E");

		// NOTE: using 'indigo-full-3.x' here instead of 'indigo-full-lifetime'.
		const std::string licence_key = 
			"someoneawesome@awesome.com<S. Awesome>;indigo-full-3.x;KFf0oXSpS2IfGa3pl6BCc1XRJr5hMcOf2ETb8xKMbI4yd+ACk7Qjy6bdy876h1ZTAaGjtQ9CWQlSD31uvRW+WO3fcuD90A/9U2JnNYKrMoV4YmCfbLuzduJ6mfRmAyHV3a9rIUELMdws7coDdwnpEQkl0rg8h2atFAXTmpmUm0Q=";
	
		LicenceType licence_type = UNLICENSED;
		std::string user_id;
		LicenceErrorCode error_code;
		verifyLicenceString(licence_key, hardware_ids, licence_type, user_id, error_code);

		testAssert(user_id == "");
		testAssert(licence_type == UNLICENSED);
		testAssert(error_code == LicenceErrorCode_NoHashMatch);
	}
	catch(LicenseExcep& e)
	{
		failTest(e.what());
	}

	// Test verifyLicenceString detects a different user name than was signed.
	try 
	{
		std::vector<std::string> hardware_ids;
		hardware_ids.push_back("              Intel(R) Pentium(R) D CPU 3.40GHz:00-25-21-7F-BB-3E");

		// NOTE: using 'Mr Bleh' here.
		const std::string licence_key = 
			"Mr Bleh;indigo-full-lifetime;KFf0oXSpS2IfGa3pl6BCc1XRJr5hMcOf2ETb8xKMbI4yd+ACk7Qjy6bdy876h1ZTAaGjtQ9CWQlSD31uvRW+WO3fcuD90A/9U2JnNYKrMoV4YmCfbLuzduJ6mfRmAyHV3a9rIUELMdws7coDdwnpEQkl0rg8h2atFAXTmpmUm0Q=";
	
		LicenceType licence_type = UNLICENSED;
		std::string user_id;
		LicenceErrorCode error_code;
		verifyLicenceString(licence_key, hardware_ids, licence_type, user_id, error_code);

		testAssert(user_id == "");
		testAssert(licence_type == UNLICENSED);
		testAssert(error_code == LicenceErrorCode_NoHashMatch);
	}
	catch(LicenseExcep& e)
	{
		failTest(e.what());
	}

	// Test verifyLicenceString detects a different hardware ID than was signed.
	try 
	{
		std::vector<std::string> hardware_ids;
		// NOTE: changed CPU freq here.
		hardware_ids.push_back("              Intel(R) Pentium(R) D CPU 6.66GHz:00-25-21-7F-BB-3E");

		const std::string licence_key = 
			"someoneawesome@awesome.com<S. Awesome>;indigo-full-lifetime;KFf0oXSpS2IfGa3pl6BCc1XRJr5hMcOf2ETb8xKMbI4yd+ACk7Qjy6bdy876h1ZTAaGjtQ9CWQlSD31uvRW+WO3fcuD90A/9U2JnNYKrMoV4YmCfbLuzduJ6mfRmAyHV3a9rIUELMdws7coDdwnpEQkl0rg8h2atFAXTmpmUm0Q=";
	
		LicenceType licence_type = UNLICENSED;
		std::string user_id;
		LicenceErrorCode error_code;
		verifyLicenceString(licence_key, hardware_ids, licence_type, user_id, error_code);

		testAssert(user_id == "");
		testAssert(licence_type == UNLICENSED);
		testAssert(error_code == LicenceErrorCode_NoHashMatch);
	}
	catch(LicenseExcep& e)
	{
		failTest(e.what());
	}


	// Test verifyLicenceString doesn't crash on missing MAC address part of hardware ID
	try 
	{
		std::vector<std::string> hardware_ids;
		// NOTE: changed CPU freq here.
		hardware_ids.push_back("              Intel(R) Pentium(R) D CPU 6.66GHz");

		const std::string licence_key = 
			"someoneawesome@awesome.com<S. Awesome>;indigo-full-lifetime;KFf0oXSpS2IfGa3pl6BCc1XRJr5hMcOf2ETb8xKMbI4yd+ACk7Qjy6bdy876h1ZTAaGjtQ9CWQlSD31uvRW+WO3fcuD90A/9U2JnNYKrMoV4YmCfbLuzduJ6mfRmAyHV3a9rIUELMdws7coDdwnpEQkl0rg8h2atFAXTmpmUm0Q=";
	
		LicenceType licence_type = UNLICENSED;
		std::string user_id;
		LicenceErrorCode error_code;
		verifyLicenceString(licence_key, hardware_ids, licence_type, user_id, error_code);

		testAssert(user_id == "");
		testAssert(licence_type == UNLICENSED);
		testAssert(error_code == LicenceErrorCode_NoHashMatch);
	}
	catch(LicenseExcep& e)
	{
		failTest(e.what());
	}




	/* 
	Test verifyLicenceString works when the licence key signature has been generated from the hardware ID with the leading whitespace stripped off, e.g. with

	N:\indigo\trunk\signing> ruby .\sign.rb "someoneawesome@awesome.com<S. Awesome>" "Intel(R) Pentium(R) D CPU 3.40GHz:00-25-21-7F-BB-3E" full-lifetime
	*/
	{
		std::vector<std::string> hardware_ids;
		hardware_ids.push_back("              Intel(R) Pentium(R) D CPU 3.40GHz:00-25-21-7F-BB-3E"); // The actual hardware ID on the computer will be unchanged.

		const std::string licence_key = 
			"someoneawesome@awesome.com<S. Awesome>;indigo-full-lifetime;X4HDE8GqCxaiuJmmIFe33JAChHpVyOUhZ/sIaVpvwJpF+VBBSG5lH5RTxInv0UGQHtSQ1WZl+NIN/mT4aQqChHmGtZFil/3EjvEBcNrt81ihD9CclMK1Zj3L5nQ+YDWqg2+SppFRC410d6f49DHAXturu5aMiu08CYOSjkFyiEU=";
	
		LicenceType licence_type = UNLICENSED;
		std::string user_id;

		try
		{
			LicenceErrorCode error_code;
			verifyLicenceString(licence_key, hardware_ids, licence_type, user_id, error_code);

			testAssert(user_id == "someoneawesome@awesome.com<S. Awesome>");
			testAssert(licence_type == FULL_LIFETIME);
			testAssert(error_code == LicenceErrorCode_NoError);
		}
		catch(LicenseExcep& e)
		{
			failTest(e.what());
		}
	}


	/* 
	Test verifyLicenceString works when the licence key signature has been generated from the hardware ID with the MAC address lower-cased.

	N:\indigo\trunk\signing> ruby .\sign.rb "someoneawesome@awesome.com<S. Awesome>" "              Intel(R) Pentium(R) D CPU 3.40GHz:00-25-21-7f-bb-3e" full-lifetime
	*/
	{
		std::vector<std::string> hardware_ids;
		hardware_ids.push_back("              Intel(R) Pentium(R) D CPU 3.40GHz:00-25-21-7F-BB-3E"); // The actual hardware ID on the computer will be unchanged.

		const std::string licence_key = 
			"someoneawesome@awesome.com<S. Awesome>;indigo-full-lifetime;R2qjttT95f0JouUWCW/NiQ6wr/DbBlLE8I1CJ5vA8O5FDuskG36baoBY/0rwHUL3a+1X6GehAat8HBNuwMgLnGanQxL33wAYjsOH0wd1LrUTmrl3HY0vRlsU914Olfa/bI82i1I1yGE9Qi7yRbVsYGTPZebrY91y08hFy+FLXFw=";
	
		LicenceType licence_type = UNLICENSED;
		std::string user_id;

		try
		{
			LicenceErrorCode error_code;
			verifyLicenceString(licence_key, hardware_ids, licence_type, user_id, error_code);

			testAssert(user_id == "someoneawesome@awesome.com<S. Awesome>");
			testAssert(licence_type == FULL_LIFETIME);
			testAssert(error_code == LicenceErrorCode_NoError);
		}
		catch(LicenseExcep& e)
		{
			failTest(e.what());
		}
	}


	{
		testAssert(getLowerCaseMACAddrHardwareID("Intel(R) Core(TM) i7 CPU         920  @ 2.67GHz:00-24-1D-C9-51-98") == "Intel(R) Core(TM) i7 CPU         920  @ 2.67GHz:00-24-1d-c9-51-98");
	}
}


#endif // BUILD_TESTS
