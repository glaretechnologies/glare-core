/*=====================================================================
SystemInfo.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Mon Mar 01 14:37:00 +1300 2010
=====================================================================*/
#include "SystemInfo.h"


#if defined(_WIN32) || defined(_WIN64)
// Stop windows.h from defining the min() and max() macros
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Iphlpapi.h>

#if !defined(__MINGW32__)
#include <intrin.h>
#endif 

#include <shlobj.h>
#else
#include <time.h>
#include <unistd.h>
#include <string.h> /* for strncpy */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#endif


#include <cassert>
#include "../utils/StringUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../indigo/globals.h"
#include <cstdlib>
#include <algorithm>

#if defined(OSX)
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif


#if defined(_WIN32) || defined(_WIN64)
class MyAdapterInfo
{
public:
	std::string MAC;
	DWORD index;
};


inline static bool myAdapterInfoComparisonPred(const MyAdapterInfo& a, const MyAdapterInfo& b)
{
	return a.index < b.index;
}
#endif


#if defined(_WIN32) || defined(_WIN64)
#else
#ifndef OSX
// From http://www.creatis.insa-lyon.fr/~malaterre/gdcm/getether/mac_addr_sys.c
long mac_addr_sys ( u_char *addr)
{
    struct ifreq ifr;
    struct ifreq *IFR;
    struct ifconf ifc;
    char buf[1024];
    int s, i;
    int ok = 0;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s==-1) {
        return -1;
    }

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    ioctl(s, SIOCGIFCONF, &ifc);

    IFR = ifc.ifc_req;
    for (i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; IFR++) {

        strcpy(ifr.ifr_name, IFR->ifr_name);
        if (ioctl(s, SIOCGIFFLAGS, &ifr) == 0) {
            if (! (ifr.ifr_flags & IFF_LOOPBACK)) {
                if (ioctl(s, SIOCGIFHWADDR, &ifr) == 0) {
                    ok = 1;
                    break;
                }
            }
        }
    }

    close(s);
    if (ok) {
        bcopy( ifr.ifr_hwaddr.sa_data, addr, 6);
    }
    else {
        return -1;
    }
    return 0;
}
#endif
#endif


void SystemInfo::getMACAddresses(std::vector<std::string>& addresses_out)
{
#if defined(_WIN32) || defined(_WIN64)
	std::vector<IP_ADAPTER_INFO> AdapterInfo(16);

	/*
	GetAdaptersInfo seems to randomly fail with ERROR_NO_DATA or ERROR_NOACCESS on older versions of Windows.
	To try to work around this, we will loop and attempt again if we get this error message.
	For more info see
	http://alax.info/blog/1195
	http://connect.microsoft.com/VisualStudio/feedback/details/665383/getadaptersaddresses-api-incorrectly-returns-no-adapters-for-a-process-with-high-memory-consumption

	NOTE: GetAdaptersInfo() is quite slow, takes ~0.6ms on my Win8 Ivy bridge box. --Nick
	NOTE: We should probably change to use GetAdaptersAddresses().
	*/

	const int MAX_NUM_CALLS = 20;
	int num_calls_done = 0;
	while(1) // Loop until GetAdaptersInfo() succeeds or we exceed the max number of attempts.
	{
		DWORD dwBufLen = AdapterInfo.size() * sizeof(IP_ADAPTER_INFO); // The size of the buffer.
		DWORD dwStatus = GetAdaptersInfo(
			AdapterInfo.data(),	// [out] buffer to receive data
			&dwBufLen		// [in] size of receive data buffer
		);
		num_calls_done++;

		// If we get an ERROR_BUFFER_OVERFLOW it means the buffer was too small.
		// Allocate a buffer that is large enough and call GetAdaptersInfo again.
		if (dwStatus == ERROR_BUFFER_OVERFLOW)
		{
			AdapterInfo.resize(dwBufLen / sizeof(IP_ADAPTER_INFO));

			dwStatus = GetAdaptersInfo(
				AdapterInfo.data(),	// [out] buffer to receive data
				&dwBufLen		// [in] size of receive data buffer
				);
		}

		if(dwStatus == ERROR_SUCCESS)
		{
			break;
		}
		else if(dwStatus == ERROR_NO_DATA || dwStatus == ERROR_NOACCESS)
		{
			// Sleep a while then try again.
			PlatformUtils::Sleep(50);

			if(num_calls_done >= MAX_NUM_CALLS)
				throw Indigo::Exception("GetAdaptersInfo Failed (after " + toString(num_calls_done) + " attempts): " + PlatformUtils::getErrorStringForReturnCode(dwStatus));
		}
		else
		{
			// Some other error occurred.
			throw Indigo::Exception("GetAdaptersInfo Failed: " + PlatformUtils::getErrorStringForReturnCode(dwStatus));
		}
	}

	PIP_ADAPTER_INFO pAdapterInfo = (PIP_ADAPTER_INFO)AdapterInfo.data(); // Contains pointer to current adapter info.
	
	std::vector<MyAdapterInfo> adapters;

	while(pAdapterInfo)
	{
		/*std::cout << "--------Details----------" << std::endl;
		std::cout << "AdapterName: " << pAdapterInfo->AdapterName << std::endl;
		std::cout << "Description: " << pAdapterInfo->Description << std::endl;
		std::cout << "Index: " << pAdapterInfo->Index << std::endl;
		std::cout << "-------------------------" << std::endl;*/

		adapters.push_back(MyAdapterInfo());
		for(UINT i = 0; i < pAdapterInfo->AddressLength; i++)
		{
			adapters.back().MAC = adapters.back().MAC + leftPad(toHexString((unsigned char)pAdapterInfo->Address[i]), '0', 2) + ((i < pAdapterInfo->AddressLength-1) ? "-" : "");
		}
		adapters.back().index = pAdapterInfo->Index;

		pAdapterInfo = pAdapterInfo->Next;    // Progress through linked list
	}

	// Sort by index
	std::sort(adapters.begin(), adapters.end(), myAdapterInfoComparisonPred);

	addresses_out.resize(adapters.size());
	for(size_t i=0; i<addresses_out.size(); ++i)
		addresses_out[i] = adapters[i].MAC;

#elif defined(OSX)

	// See https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man3/sysctl.3.html for info on sysctl()

	int	mib[6];
	mib[0] = CTL_NET;
	mib[1] = AF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_LINK;
	mib[4] = NET_RT_IFLIST;
	if ((mib[5] = if_nametoindex("en1")) == 0 && (mib[5] = if_nametoindex("en0")) == 0) {
		throw Indigo::Exception("if_nametoindex error");
	}

	// Get buffer length needed
	size_t len;
	if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0) {
		throw Indigo::Exception("sysctl 1 error");
	}

	// Allocate buffer
	char* buf;
	if ((buf = (char *)malloc(len)) == NULL) {
		throw Indigo::Exception("malloc error");
	}

	// Get information (will be placed into buf)
	if (sysctl(mib, 6, buf, &len, NULL, 0) < 0) {
		throw Indigo::Exception("sysctl 2 error");
	}

	struct if_msghdr* ifm = (struct if_msghdr*)buf;
	struct sockaddr_dl* sdl = (struct sockaddr_dl*)(ifm + 1);
	unsigned char* ptr = (unsigned char*)LLADDR(sdl);

	// Convert binary MAC address to hexadecimal representation.
	char buffer[100];

	sprintf(buffer, "%02X-%02X-%02X-%02X-%02X-%02X", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5));

	addresses_out.resize(0);
	addresses_out.push_back(std::string(buffer));

	// Free buffer
	free(buf);

#else
	// else Linux
	addresses_out.resize(1);
	unsigned char addr[6];
	if(mac_addr_sys(addr) != 0)
		throw Indigo::Exception("mac_addr_sys failed.");

	for(unsigned i=0; i<6; ++i)
	{
		addresses_out.back() = addresses_out.back() + leftPad(toHexString((unsigned char)addr[i]), '0', 2) + ((i < 5) ? "-" : "");
	}
#endif
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"


void SystemInfo::test()
{
	conPrint("SystemInfo::test()");

	try
	{
		// Timer timer;
		int N = 10;
		std::vector<std::string> addresses;
		for(int i=0; i<N; ++i)
		{
			getMACAddresses(addresses);
		}

		// double per_call_time = timer.elapsed() / N;
		// conPrint("per_call_time: " + toString(per_call_time) + " s");
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


#endif
