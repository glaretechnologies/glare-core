/*=====================================================================
SystemInfo.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Mon Mar 01 14:37:00 +1300 2010
=====================================================================*/
#include "SystemInfo.h"


#if defined(WIN32) || defined(WIN64)
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
#include "../utils/stringutils.h"
#include "../utils/fileutils.h"
#include "../utils/Exception.h"
#include "../indigo/globals.h"
#include <cstdlib>
#include <iostream>//TEMP
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


#if defined(WIN32) || defined(WIN64)
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


#if defined(WIN32) || defined(WIN64)
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
#if defined(WIN32) || defined(WIN64)
	IP_ADAPTER_INFO AdapterInfo[16];		// Allocate information
											// for up to 16 NICs
	DWORD dwBufLen = sizeof(AdapterInfo);	// Save memory size of buffer

	const DWORD dwStatus = GetAdaptersInfo(      // Call GetAdapterInfo
		AdapterInfo,	// [out] buffer to receive data
		&dwBufLen		// [in] size of receive data buffer
	);

	if(dwStatus != ERROR_SUCCESS)
		throw Indigo::Exception("GetAdaptersInfo Failed.");

	PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo; // Contains pointer to current adapter info.

	
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
	int			mib[6];
	size_t	 	len;
	char			*buf;
	unsigned char		*ptr;
	struct if_msghdr	*ifm;
	struct sockaddr_dl	*sdl;

	mib[0] = CTL_NET;
	mib[1] = AF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_LINK;
	mib[4] = NET_RT_IFLIST;
	if ((mib[5] = if_nametoindex("en1")) == 0 && (mib[5] = if_nametoindex("en0")) == 0) {
		throw Indigo::Exception("if_nametoindex error");
	}

	if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0) {
		throw Indigo::Exception("sysctl 1 error");
	}

	if ((buf = (char *)malloc(len)) == NULL) {
		throw Indigo::Exception("malloc error");
	}

	if (sysctl(mib, 6, buf, &len, NULL, 0) < 0) {
		throw Indigo::Exception("sysctl 2 error");
	}

	ifm = (struct if_msghdr *)buf;
	sdl = (struct sockaddr_dl *)(ifm + 1);
	ptr = (unsigned char *)LLADDR(sdl);

	char buffer[100];

	sprintf(buffer, "%02x-%02x-%02x-%02x-%02x-%02x", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5));

	addresses_out.resize(0);
	addresses_out.push_back(std::string(buffer));

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

