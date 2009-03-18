/*=====================================================================
platformutils.cpp
-----------------
File created by ClassTemplate on Mon Jun 06 00:24:52 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "platformutils.h"

#if defined(WIN32) || defined(WIN64)
// Stop windows.h from defining the min() and max() macros
#define NOMINMAX
#include <windows.h>
#include <Iphlpapi.h>
#include <intrin.h>
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
#include <iostream> //TEMP


//make current thread sleep for x milliseconds
void PlatformUtils::Sleep(int x)
{
#if defined(WIN32) || defined(WIN64)
	::Sleep(x);
#else
	int numseconds = x / 1000;
	int fraction = x % 1000;//fractional seconds in ms
	struct timespec t;
	t.tv_sec = numseconds;   //seconds
	t.tv_nsec = fraction * 1000000; //nanoseconds
	nanosleep(&t, NULL);
#endif
}

/*
unsigned int PlatformUtils::getMinWorkingSetSize()
{
	SIZE_T min, max;
	GetProcessWorkingSetSize(GetCurrentProcess(), &min, &max);

	return (unsigned int)min;
}
unsigned int PlatformUtils::getMaxWorkingSetSize()
{
	SIZE_T min, max;
	GetProcessWorkingSetSize(GetCurrentProcess(), &min, &max);

	return (unsigned int)max;
}
*/

unsigned int PlatformUtils::getNumLogicalProcessors()
{
#if defined(WIN32) || defined(WIN64)
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	return system_info.dwNumberOfProcessors;
#else
	return sysconf(_SC_NPROCESSORS_CONF);
#endif
}



#if defined(WIN32) || defined(WIN64)
#else
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



void PlatformUtils::getMACAddresses(std::vector<std::string>& addresses_out)
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
		throw PlatformUtilsExcep("GetAdaptersInfo Failed.");

	PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo; // Contains pointer to current adapter info.

	addresses_out.resize(0);
	while(pAdapterInfo)
	{
		addresses_out.push_back("");
		for(UINT i = 0; i < pAdapterInfo->AddressLength; i++)
		{
			addresses_out.back() = addresses_out.back() + leftPad(toHexString((unsigned char)pAdapterInfo->Address[i]), '0', 2) + ((i < pAdapterInfo->AddressLength-1) ? "-" : "");
		}

		pAdapterInfo = pAdapterInfo->Next;    // Progress through linked list
	}
#else
	/*int fd = socket(AF_INET, SOCK_DGRAM, 0);

	if(fd == -1)
		throw PlatformUtilsExcep("socket() Failed.");

	struct ifreq ifr;
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1); // Assuming we want eth0

	if(ioctl(fd, SIOCGIFHWADDR, &ifr) == -1) // Retrieve MAC address
		throw PlatformUtilsExcep("ioctl() failed.");

	close(fd);

	addresses_out.resize(1);
	for(unsigned i=0; i<6; ++i)
	{
		addresses_out.back() = addresses_out.back() + leftPad(toHexString((unsigned char)ifr.ifr_hwaddr.sa_data[i]), '0', 2) + ((i < 5) ? "-" : "");
	}*/
	addresses_out.resize(1);
	unsigned char addr[6];
	if(mac_addr_sys(addr) != 0)
		throw PlatformUtilsExcep("mac_addr_sys failed.");

	for(unsigned i=0; i<6; ++i)
	{
		addresses_out.back() = addresses_out.back() + leftPad(toHexString((unsigned char)addr[i]), '0', 2) + ((i < 5) ? "-" : "");
	}

#endif
}


static void doCPUID(unsigned int infotype, unsigned int* out)
{


#if defined(WIN32) || defined(WIN64)
	int CPUInfo[4];
	__cpuid(
		CPUInfo,
		infotype // infotype
		);
	memcpy(out, CPUInfo, 16);
#else
	// ebx saving is necessary for PIC
	__asm__ volatile(
			"mov %%ebx, %%esi\n\t"
			"cpuid\n\t"
			"xchg %%ebx, %%esi"
            : "=a" (out[0]),
			"=S" (out[1]),
            "=c" (out[2]),
            "=d" (out[3])
            : "0" (infotype)
     );
#endif
}


void PlatformUtils::getCPUInfo(CPUInfo& info_out)
{
	const int MMX_FLAG = 1 << 23;
	const int SSE_FLAG = 1 << 25;
	const int SSE2_FLAG = 1 << 26;
	const int SSE3_FLAG = 1;

	unsigned int CPUInfo[4];
	doCPUID(0, CPUInfo);

	const int highest_param = CPUInfo[0];
	assert(highest_param >= 1);

	memcpy(info_out.vendor, &CPUInfo[1], 4);
	memcpy(info_out.vendor + 4, &CPUInfo[3], 4);
	memcpy(info_out.vendor + 8, &CPUInfo[2], 4);
	info_out.vendor[12] = 0;

	doCPUID(1, CPUInfo);

	info_out.mmx = (CPUInfo[3] & MMX_FLAG ) != 0;
	info_out.sse1 = (CPUInfo[3] & SSE_FLAG ) != 0;
	info_out.sse2 = (CPUInfo[3] & SSE2_FLAG ) != 0;
	info_out.sse3 = (CPUInfo[2] & SSE3_FLAG ) != 0;
	info_out.stepping = CPUInfo[0] & 0xF;
	info_out.model = (CPUInfo[0] >> 4) & 0xF;
	info_out.family = (CPUInfo[0] >> 8) & 0xF;

	doCPUID(0x80000000, CPUInfo);

	unsigned int highest_extended_param;
	memcpy(&highest_extended_param, &CPUInfo[0], 4);
	assert(highest_extended_param >= 0x80000004);

	doCPUID(0x80000002, CPUInfo);
	memcpy(info_out.proc_brand + 0, CPUInfo + 0, 4);
	memcpy(info_out.proc_brand + 4, CPUInfo + 1, 4);
	memcpy(info_out.proc_brand + 8, CPUInfo + 2, 4);
	memcpy(info_out.proc_brand + 12, CPUInfo + 3, 4);
	doCPUID(0x80000003, CPUInfo);
	memcpy(info_out.proc_brand + 16, CPUInfo + 0, 4);
	memcpy(info_out.proc_brand + 20, CPUInfo + 1, 4);
	memcpy(info_out.proc_brand + 24, CPUInfo + 2, 4);
	memcpy(info_out.proc_brand + 28, CPUInfo + 3, 4);
	doCPUID(0x80000004, CPUInfo);
	memcpy(info_out.proc_brand + 32, CPUInfo + 0, 4);
	memcpy(info_out.proc_brand + 36, CPUInfo + 1, 4);
	memcpy(info_out.proc_brand + 40, CPUInfo + 2, 4);
	memcpy(info_out.proc_brand + 44, CPUInfo + 3, 4);
/*
#else
	unsigned int a, b, c, d;
	cpuid(0, a, b, c, d);

	const unsigned int highest_param = a;
	assert(highest_param >= 1);

	memcpy(info_out.vendor, &b, 4);
	memcpy(info_out.vendor + 4, &d, 4);
	memcpy(info_out.vendor + 8, &c, 4);
	info_out.vendor[12] = 0;

	cpuid(1, a, b, c, d);

	info_out.mmx = (d & MMX_FLAG) != 0;
	info_out.sse1 = (d & SSE_FLAG ) != 0;
	info_out.sse2 = (d & SSE2_FLAG ) != 0;
	info_out.sse3 = (c & SSE3_FLAG ) != 0;
	info_out.stepping = a & 0xF;
	info_out.model = (a >> 4) & 0xF;
	info_out.family = (a >> 8) & 0xF;

	cpuid(0x80000000, a, b, c, d);
	const unsigned int highest_extended_param = a;
	assert(highest_extended_param >= 0x80000004);

	cpuid(0x80000002, a, b, c, d);
	memcpy(info_out.proc_brand + 0, &a, 4);
	memcpy(info_out.proc_brand + 4, &b, 4);
	memcpy(info_out.proc_brand + 8, &c, 4);
	memcpy(info_out.proc_brand + 12, &d, 4);

	cpuid(0x80000003, a, b, c, d);
	memcpy(info_out.proc_brand + 16, &a, 4);
	memcpy(info_out.proc_brand + 20, &b, 4);
	memcpy(info_out.proc_brand + 24, &c, 4);
	memcpy(info_out.proc_brand + 28, &d, 4);

	cpuid(0x80000004, a, b, c, d);
	memcpy(info_out.proc_brand + 32, &a, 4);
	memcpy(info_out.proc_brand + 36, &b, 4);
	memcpy(info_out.proc_brand + 40, &c, 4);
	memcpy(info_out.proc_brand + 44, &d, 4);
#endif*/
}
