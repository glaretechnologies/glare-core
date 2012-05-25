/*=====================================================================
CheckDLLSignature.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed May 26 14:52:09 +1200 2010
=====================================================================*/
#include "CheckDLLSignature.h"

#include <iostream>
#include "stringutils.h"
#include <assert.h>

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


    LONG lStatus;
    DWORD dwLastError;

    // Initialize the WINTRUST_FILE_INFO structure.

    WINTRUST_FILE_INFO FileData;
    memset(&FileData, 0, sizeof(FileData));
    FileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
    FileData.pcwszFilePath = StringUtils::UTF8ToPlatformUnicodeEncoding(dll_path).c_str();
    FileData.hFile = NULL;
    FileData.pgKnownSubject = NULL;

    /*
    WVTPolicyGUID specifies the policy to apply on the file
    WINTRUST_ACTION_GENERIC_VERIFY_V2 policy checks:
    
    1) The certificate used to sign the file chains up to a root 
    certificate located in the trusted root certificate store. This 
    implies that the identity of the publisher has been verified by 
    a certification authority.
    
    2) In cases where user interface is displayed (which this example
    does not do), WinVerifyTrust will check for whether the  
    end entity certificate is stored in the trusted publisher store,  
    implying that the user trusts content from this publisher.
    
    3) The end entity certificate has sufficient permission to sign 
    code, as indicated by the presence of a code signing EKU or no 
    EKU.
    */

    GUID WVTPolicyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA WinTrustData;

    // Initialize the WinVerifyTrust input data structure.

    // Default all fields to 0.
    memset(&WinTrustData, 0, sizeof(WinTrustData));

    WinTrustData.cbStruct = sizeof(WinTrustData);
    
    // Use default code signing EKU.
    WinTrustData.pPolicyCallbackData = NULL;

    // No data to pass to SIP.
    WinTrustData.pSIPClientData = NULL;

    // Disable WVT UI.
    WinTrustData.dwUIChoice = WTD_UI_NONE;

    // No revocation checking.
    WinTrustData.fdwRevocationChecks = WTD_REVOKE_NONE; 

    // Verify an embedded signature on a file.
    WinTrustData.dwUnionChoice = WTD_CHOICE_FILE;

    // Default verification.
    WinTrustData.dwStateAction = 0;

    // Not applicable for default verification of embedded signature.
    WinTrustData.hWVTStateData = NULL;

    // Not used.
    WinTrustData.pwszURLReference = NULL;

    // Default.
    WinTrustData.dwProvFlags = WTD_SAFER_FLAG;

    // This is not applicable if there is no UI because it changes 
    // the UI to accommodate running applications instead of 
    // installing applications.
    WinTrustData.dwUIContext = 0;

    // Set pFile.
    WinTrustData.pFile = &FileData;

    // WinVerifyTrust verifies signatures as specified by the GUID 
    // and Wintrust_Data.
    lStatus = WinVerifyTrust(
        NULL,
        &WVTPolicyGUID,
        &WinTrustData);

    switch (lStatus) 
    {
        case ERROR_SUCCESS:
            /*
            Signed file:
                - Hash that represents the subject is trusted.

                - Trusted publisher without any verification errors.

                - UI was disabled in dwUIChoice. No publisher or 
                    time stamp chain errors.

                - UI was enabled in dwUIChoice and the user clicked 
                    "Yes" when asked to install and run the signed 
                    subject.
            */
            //wprintf_s(L"The file \"%s\" is signed and the signature "
            //    L"was verified.\n",
            //    pwszSourceFile);
			return true;
            break;
        
        case TRUST_E_NOSIGNATURE:
            // The file was not signed or had a signature 
            // that was not valid.

            // Get the reason for no signature.
            dwLastError = GetLastError();
            if (TRUST_E_NOSIGNATURE == dwLastError ||
                    TRUST_E_SUBJECT_FORM_UNKNOWN == dwLastError ||
                    TRUST_E_PROVIDER_UNKNOWN == dwLastError) 
            {
                // The file was not signed.
                //wprintf_s(L"The file \"%s\" is not signed.\n",
                //    pwszSourceFile);
				int notsigned = 3;
            } 
            else 
            {
                // The signature was not valid or there was an error 
                // opening the file.
                //wprintf_s(L"An unknown error occurred trying to "
                //    L"verify the signature of the \"%s\" file.\n",
                //    pwszSourceFile);

				int notvalid_error_opening_file = 3;
			}

            break;

        case TRUST_E_EXPLICIT_DISTRUST:
            // The hash that represents the subject or the publisher 
            // is not allowed by the admin or user.
            //wprintf_s(L"The signature is present, but specifically "
            //    L"disallowed.\n");
            break;

        case TRUST_E_SUBJECT_NOT_TRUSTED:
            // The user clicked "No" when asked to install and run.
            //wprintf_s(L"The signature is present, but not "
            //    L"trusted.\n");
            break;

        case CRYPT_E_SECURITY_SETTINGS:
            /*
            The hash that represents the subject or the publisher 
            was not explicitly trusted by the admin and the 
            admin policy has disabled user trust. No signature, 
            publisher or time stamp errors.
            */
            //wprintf_s(L"CRYPT_E_SECURITY_SETTINGS - The hash "
            //    L"representing the subject or the publisher wasn't "
            //    L"explicitly trusted by the admin and admin policy "
            //    L"has disabled user trust. No signature, publisher "
            //    L"or timestamp errors.\n");
            break;

        default:
            // The UI was disabled in dwUIChoice or the admin policy 
            // has disabled user trust. lStatus contains the 
            // publisher or time stamp chain error.
            //wprintf_s(L"Error is: 0x%x.\n", lStatus);
            break;
    }

    return false;









	//GUID guidAction = WINTRUST_ACTION_GENERIC_VERIFY_V2;
	//WINTRUST_FILE_INFO sWintrustFileInfo;
	//WINTRUST_DATA      sWintrustData;
	//HRESULT            hr;

	//memset((void*)&sWintrustFileInfo, 0x00, sizeof(WINTRUST_FILE_INFO));
	//memset((void*)&sWintrustData, 0x00, sizeof(WINTRUST_DATA));

	//sWintrustFileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
	//sWintrustFileInfo.pcwszFilePath = StringUtils::UTF8ToPlatformUnicodeEncoding(dll_path).c_str();
	//sWintrustFileInfo.hFile = NULL;

	//sWintrustData.cbStruct            = sizeof(WINTRUST_DATA);
	//sWintrustData.dwUIChoice          = WTD_UI_NONE;
	//sWintrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
	//sWintrustData.dwUnionChoice       = WTD_CHOICE_FILE;
	//sWintrustData.pFile               = &sWintrustFileInfo;
	//sWintrustData.dwStateAction       = WTD_STATEACTION_VERIFY;

	//hr = WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &guidAction, &sWintrustData);

	//if (TRUST_E_NOSIGNATURE == hr)
	//{
	//	//_tprintf(_T("No signature found on the file.\n"));
	//	return false;
	//}
	//else if (TRUST_E_BAD_DIGEST == hr)
	//{
	//	//_tprintf(_T("The signature of the file is invalid\n"));
	//	return false;
	//}
	//else if (TRUST_E_PROVIDER_UNKNOWN == hr)
	//{
	//	//_tprintf(_T("No trust provider on this machine can verify this type of files.\n"));
	//	return false;
	//}
	//else if (S_OK != hr)
	//{
	//	//_tprintf(_T("WinVerifyTrust failed with error 0x%.8X\n"), hr);
	//	return false;
	//}
	//else
	//{
	//	//_tprintf(_T("File signature is OK.\n"));

	//	// retreive the signer certificate and display its information
	//	CRYPT_PROVIDER_DATA const *psProvData     = NULL;
	//	CRYPT_PROVIDER_SGNR       *psProvSigner   = NULL;
	//	CRYPT_PROVIDER_CERT       *psProvCert     = NULL;

	//	psProvData = WTHelperProvDataFromStateData(sWintrustData.hWVTStateData);
	//	if (psProvData)
	//	{
	//		psProvSigner = WTHelperGetProvSignerFromChain((PCRYPT_PROVIDER_DATA)psProvData, 0 , FALSE, 0);
	//		if (psProvSigner)
	//		{
	//			psProvCert = WTHelperGetProvCertFromChain(psProvSigner, 0);
	//			if (psProvCert)
	//			{
	//				DWORD dwStrType = CERT_X500_NAME_STR;
	//				DWORD dwCount = CertGetNameString(psProvCert->pCert,
	//					CERT_NAME_RDN_TYPE,
	//					0,
	//					&dwStrType,
	//					NULL,
	//					0);
	//				if (dwCount)
	//				{
	//					LPTSTR szSubjectRDN = (LPTSTR) LocalAlloc(0, dwCount * sizeof(TCHAR));
	//					dwCount = CertGetNameString(psProvCert->pCert,
	//						CERT_NAME_RDN_TYPE,
	//						0,
	//						&dwStrType,
	//						szSubjectRDN,
	//						dwCount);
	//					if (dwCount)
	//					{
	//						//_tprintf(_T("File signer = %s\n"), szSubjectRDN);
	//						std::cout << "file signature: " << szSubjectRDN << std::endl;
	//					}

	//					LocalFree(szSubjectRDN);
	//				}
	//			}
	//		}
	//	}
	//}

	//sWintrustData.dwUIChoice = WTD_UI_NONE;
	//sWintrustData.dwStateAction = WTD_STATEACTION_CLOSE;
	//WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &guidAction, &sWintrustData);

	//return true;

#else
	assert(false);
	return false;
#endif
}
