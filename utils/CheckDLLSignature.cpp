/*=====================================================================
CheckDLLSignature.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed May 26 14:52:09 +1200 2010
=====================================================================*/
#include "CheckDLLSignature.h"

#include <iostream>
#include "stringutils.h"

#ifdef _MSC_VER

#include <windows.h>
#include <Softpub.h>
#include <Wincrypt.h>
#include <tchar.h>
#include <stdlib.h>

// Include required libraries
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Wintrust.lib")

#endif

CheckDLLSignature::CheckDLLSignature()
{

}


CheckDLLSignature::~CheckDLLSignature()
{

}

// Derived from vertrust.cpp in the Microsoft Platform SDK
bool CheckDLLSignature::verifyDLL(std::string dll_path, std::string expected_sig)
{
#ifdef _MSC_VER

	GUID guidAction = WINTRUST_ACTION_GENERIC_VERIFY_V2;
	WINTRUST_FILE_INFO sWintrustFileInfo;
	WINTRUST_DATA      sWintrustData;
	HRESULT            hr;

	memset((void*)&sWintrustFileInfo, 0x00, sizeof(WINTRUST_FILE_INFO));
	memset((void*)&sWintrustData, 0x00, sizeof(WINTRUST_DATA));

	sWintrustFileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
	sWintrustFileInfo.pcwszFilePath = StringUtils::UTF8ToPlatformUnicodeEncoding(dll_path).c_str();
	sWintrustFileInfo.hFile = NULL;

	sWintrustData.cbStruct            = sizeof(WINTRUST_DATA);
	sWintrustData.dwUIChoice          = WTD_UI_NONE;
	sWintrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
	sWintrustData.dwUnionChoice       = WTD_CHOICE_FILE;
	sWintrustData.pFile               = &sWintrustFileInfo;
	sWintrustData.dwStateAction       = WTD_STATEACTION_VERIFY;

	hr = WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &guidAction, &sWintrustData);

	if (TRUST_E_NOSIGNATURE == hr)
	{
		//_tprintf(_T("No signature found on the file.\n"));
		return false;
	}
	else if (TRUST_E_BAD_DIGEST == hr)
	{
		//_tprintf(_T("The signature of the file is invalid\n"));
		return false;
	}
	else if (TRUST_E_PROVIDER_UNKNOWN == hr)
	{
		//_tprintf(_T("No trust provider on this machine can verify this type of files.\n"));
		return false;
	}
	else if (S_OK != hr)
	{
		//_tprintf(_T("WinVerifyTrust failed with error 0x%.8X\n"), hr);
		return false;
	}
	else
	{
		//_tprintf(_T("File signature is OK.\n"));

		// retreive the signer certificate and display its information
		CRYPT_PROVIDER_DATA const *psProvData     = NULL;
		CRYPT_PROVIDER_SGNR       *psProvSigner   = NULL;
		CRYPT_PROVIDER_CERT       *psProvCert     = NULL;

		psProvData = WTHelperProvDataFromStateData(sWintrustData.hWVTStateData);
		if (psProvData)
		{
			psProvSigner = WTHelperGetProvSignerFromChain((PCRYPT_PROVIDER_DATA)psProvData, 0 , FALSE, 0);
			if (psProvSigner)
			{
				psProvCert = WTHelperGetProvCertFromChain(psProvSigner, 0);
				if (psProvCert)
				{
					DWORD dwStrType = CERT_X500_NAME_STR;
					DWORD dwCount = CertGetNameString(psProvCert->pCert,
						CERT_NAME_RDN_TYPE,
						0,
						&dwStrType,
						NULL,
						0);
					if (dwCount)
					{
						LPTSTR szSubjectRDN = (LPTSTR) LocalAlloc(0, dwCount * sizeof(TCHAR));
						dwCount = CertGetNameString(psProvCert->pCert,
							CERT_NAME_RDN_TYPE,
							0,
							&dwStrType,
							szSubjectRDN,
							dwCount);
						if (dwCount)
						{
							//_tprintf(_T("File signer = %s\n"), szSubjectRDN);
							std::cout << "file signature: " << szSubjectRDN << std::endl;
						}

						LocalFree(szSubjectRDN);
					}
				}
			}
		}
	}

	sWintrustData.dwUIChoice = WTD_UI_NONE;
	sWintrustData.dwStateAction = WTD_STATEACTION_CLOSE;
	WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &guidAction, &sWintrustData);

	return true;

#else
	assert(false);
	return false;
#endif
}
