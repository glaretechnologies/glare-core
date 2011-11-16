/*=====================================================================
X509Certificate.cpp
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at Fri Nov 11 13:48:01 +0000 2011
=====================================================================*/
#include "X509Certificate.h"


#ifdef _WIN32
#include "Exception.h"
#include "stringutils.h"
#include "platformutils.h"
#include <Windows.h>
#include <Wincrypt.h>

#define MY_ENCODING_TYPE (PKCS_7_ASN_ENCODING | X509_ASN_ENCODING)
#define MY_STRING_TYPE (CERT_OID_NAME_STR)

#pragma comment(lib, "crypt32.lib")
#endif


bool X509Certificate::verifyCertificate(const std::string &subject, const std::string &public_key_string)
{
#ifdef _WIN32
	HCERTSTORE hCertStore;

	//---------------------------------------------------------------
	// Begin Processing by opening a certificate store.
	if(!(hCertStore = CertOpenStore(
		CERT_STORE_PROV_SYSTEM,
		MY_ENCODING_TYPE,
		NULL,
		CERT_SYSTEM_STORE_CURRENT_USER,
		L"Root")))
		throw Indigo::Exception("Couldn't open Root certificate store");

	const std::wstring lpszCertSubject = StringUtils::UTF8ToPlatformUnicodeEncoding(subject);

	PCCERT_CONTEXT pDesiredCert;
	if(pDesiredCert = CertFindCertificateInStore(
		hCertStore,
		MY_ENCODING_TYPE,           // Use X509_ASN_ENCODING.
		0,                          // No dwFlags needed. 
		CERT_FIND_SUBJECT_STR,      // Find a certificate with a subject that matches the string in the next parameter.
		lpszCertSubject.c_str(),	// The Unicode string to be found in a certificate's subject.
		NULL))                      // NULL for the first call to the function. In all subsequent calls, it is the last pointer returned by the function.
	{
		PCERT_INFO cert_info = pDesiredCert->pCertInfo;
		CERT_PUBLIC_KEY_INFO pki = cert_info->SubjectPublicKeyInfo;
		CRYPT_BIT_BLOB blob = pki.PublicKey;

		// Convert the public key hex string to bytes for comparing
		const std::vector<uint8> public_key_bytes = StringUtils::convertHexToBinary(public_key_string);

		// Check to see if the public key lengths are the same
		if(public_key_bytes.size() != (size_t)blob.cbData)
			return false;

		// Check to see if the public key matches what we expect
		for(DWORD i = 0; i < blob.cbData; ++i)
			if(public_key_bytes[i] != blob.pbData[i])
				return false;

		return true; // Successfully matched the public key
	}
#else
	assert(false);
#endif
	return false;
}


#if BUILD_TESTS


#include <iostream>


void X509Certificate::test()
{
	const std::string greenbutton_cert_subj = "GreenButton for Indigo";
	const std::string greenbutton_cert_pubkey = "3082010a0282010100ba9de4caa44d4d2edaf43716024c07584bfab4403c590a050f4687c56c3884f273b7f94e86746b5bce2b4816d13d4fd4d0644d88f98344c559e4159ecd044b11077f3c75adffddb8811b3ec0bdedd29b9411d84f85febf42c8c6c2ac08ec6187ebdd9bf049090af3395eab8d8fc4aa621cea52200f5996130a22e2eda33879ba8e8f72778125a709079ca84456694e2d792f340009d4d87e9343b4dce4fca72f12aff86964d1eeb090b6e959c2d34ced33aec996a16c7bec2843f4e014c77ce0c40d465e52239eb6d0e231c071c2710c3162d69f54726e02de2b51098ffcf931cfa6f5ee1bbbdf498b81bda54ff8f6a188b3bbe7026670079c04659621a6aa010203010001";

	try
	{
		const bool found_gb_cert = verifyCertificate(greenbutton_cert_subj, greenbutton_cert_pubkey);

		if(found_gb_cert)
			std::cout << "Found GreenButton certificate" << std::endl;
		else
			std::cout << "Couldn't find GreenButton certificate" << std::endl;
	}
	catch(Indigo::Exception& e)
	{
		std::cout << "X.509 exception: " + e.what() << std::endl;
	}


#if 0
	//---------------------------------------------------------------
	//       Loop through the certificates in the store. 
	//       For each certificate,
	//             get and print the name of the 
	//                  certificate subject and issuer.
	//             convert the subject name from the certificate
	//                  to an ASN.1 encoded string and print the
	//                  octets from that string.
	//             convert the encoded string back into its form 
	//                  in the certificate.
	pCertContext = NULL;
	while(pCertContext = CertEnumCertificatesInStore(
		hCertStore,
		pCertContext))
	{
		LPTSTR pszString;
		LPTSTR pszName;
		DWORD cbSize;
		CERT_BLOB blobEncodedName;

		//-----------------------------------------------------------
		//        Get and display 
		//        the name of subject of the certificate.
		if(!(cbSize = CertGetNameString(
			pCertContext,
			CERT_NAME_SIMPLE_DISPLAY_TYPE,
			0,
			NULL,
			NULL,
			0)))
		{
			MyHandleError(TEXT("CertGetName 1 failed."));
		}

		if(!(pszName = (LPTSTR)malloc(cbSize * sizeof(TCHAR))))
		{
			MyHandleError(TEXT("Memory allocation failed."));
		}

		std::string cert_subject;

		if(CertGetNameString(
			pCertContext,
			CERT_NAME_SIMPLE_DISPLAY_TYPE,
			0,
			NULL,
			pszName,
			cbSize))
		{
			_tprintf(TEXT("\nSubject -> %s.\n"), pszName);

			cert_subject = StringUtils::PlatformToUTF8UnicodeEncoding(pszName);

			//-------------------------------------------------------
			//       Free the memory allocated for the string.
			free(pszName);
		}
		else
		{
			MyHandleError(TEXT("CertGetName failed."));
		}

		std::cout << "subject = " << cert_subject.c_str() << std::endl;

		//-----------------------------------------------------------
		//        Get and display 
		//        the name of Issuer of the certificate.
		if(!(cbSize = CertGetNameString(
			pCertContext,
			CERT_NAME_SIMPLE_DISPLAY_TYPE,
			CERT_NAME_ISSUER_FLAG,
			NULL,
			NULL,
			0)))
		{
			MyHandleError(TEXT("CertGetName 1 failed."));
		}

		if(!(pszName = (LPTSTR)malloc(cbSize * sizeof(TCHAR))))
		{
			MyHandleError(TEXT("Memory allocation failed."));
		}

		if(CertGetNameString(
			pCertContext,
			CERT_NAME_SIMPLE_DISPLAY_TYPE,
			CERT_NAME_ISSUER_FLAG,
			NULL,
			pszName,
			cbSize))
		{
			_tprintf(TEXT("Issuer  -> %s.\n"), pszName);

			//-------------------------------------------------------
			//       Free the memory allocated for the string.
			free(pszName);
		}
		else
		{
			MyHandleError(TEXT("CertGetName failed."));
		}

		//-----------------------------------------------------------
		//       Convert the subject name to an ASN.1 encoded
		//       string and print the octets in that string.

		//       First : Get the number of bytes that must 
		//       be allocated for the string.
		cbSize = CertNameToStr(
			pCertContext->dwCertEncodingType,
			&(pCertContext->pCertInfo->Subject),
			MY_STRING_TYPE,
			NULL,
			0);

		//-----------------------------------------------------------
		//  The function CertNameToStr returns the number
		//  of bytes needed for a string to hold the
		//  converted name, including the null terminator. 
		//  If it returns one, the name is an empty string.
		if (1 == cbSize)
		{
			MyHandleError(TEXT("Subject name is an empty string."));
		}

		//-----------------------------------------------------------
		//        Allocated the needed buffer. Note that this
		//        memory must be freed inside the loop or the 
		//        application will leak memory.
		if(!(pszString = (LPTSTR)malloc(cbSize * sizeof(TCHAR))))
		{
			MyHandleError(TEXT("Memory allocation failed."));
		}

		//-----------------------------------------------------------
		//       Call the function again to get the string. 
		cbSize = CertNameToStr(
			pCertContext->dwCertEncodingType,
			&(pCertContext->pCertInfo->Subject),
			MY_STRING_TYPE,
			pszString,
			cbSize);

		//-----------------------------------------------------------
		//  The function CertNameToStr returns the number
		//  of bytes in the string, including the null terminator.
		//  If it returns 1, the name is an empty string.
		if (1 == cbSize)
		{
			MyHandleError(TEXT("Subject name is an empty string."));
		}

		//-----------------------------------------------------------
		//    Get the length needed to convert the string back
		//    back into the name as it was in the certificate.
		if(!(CertStrToName(
			MY_ENCODING_TYPE,
			pszString,
			MY_STRING_TYPE,
			NULL,
			NULL,        // NULL to get the number of bytes needed for the buffer.
			&cbSize,     // Pointer to a DWORD to hold the number of bytes needed for the buffer
			NULL )))     // Optional address of a pointer to old the location for an error in the input string.
		{
			MyHandleError(
				TEXT("Could not get the length of the BLOB."));
		}

		if (!(blobEncodedName.pbData = (LPBYTE)malloc(cbSize)))
		{
			MyHandleError(
				TEXT("Memory Allocation for the BLOB failed."));
		}
		blobEncodedName.cbData = cbSize;

		if(CertStrToName(
			MY_ENCODING_TYPE,
			pszString, 
			MY_STRING_TYPE,
			NULL,
			blobEncodedName.pbData,
			&blobEncodedName.cbData,
			NULL))
		{
			_tprintf(TEXT("CertStrToName created the BLOB.\n"));
		}
		else
		{
			MyHandleError(TEXT("Could not create the BLOB."));
		}

		//-----------------------------------------------------------
		//       Free the memory.

		free(blobEncodedName.pbData);
		free(pszString);

		//-----------------------------------------------------------
		//       Pause before information on the next certificate
		//       is displayed.
		//Local_wait();
	}

	DWORD derp = GetLastError();
	if (derp == CRYPT_E_NOT_FOUND)
		std::cout << "no certificates found or end of store's list" << std::endl;

	_tprintf(
		TEXT("\nThere are no more certificates in the store. \n"));

	//---------------------------------------------------------------
	//   Close the MY store.
	if(CertCloseStore(
		hCertStore,
		CERT_CLOSE_STORE_CHECK_FLAG))
	{
		_tprintf(TEXT("The store is closed. ")
			TEXT("All certificates are released.\n"));
	}
	else
	{
		_tprintf(TEXT("The store was closed, ")
			TEXT("but certificates still in use.\n"));
	}

	_tprintf(TEXT("This demonstration program ran to completion ")
		TEXT("without error.\n"));
#endif
}


#endif
