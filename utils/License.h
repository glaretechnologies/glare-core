/*===================================================================
Copyright Glare Technologies Limited 2012 -
File created by ClassTemplate on Thu Mar 19 14:06:32 2009
====================================================================*/
#pragma once


#include <string>
#include <vector>


/*=====================================================================
License
-------

=====================================================================*/
class License
{
public:

	friend class LicenseTestThread;

	class LicenseExcep
	{
	public:
		LicenseExcep(const std::string& s_) : s(s_) {}
		const std::string& what() const { return s; }
	private:
		std::string s;
	};


	enum LicenceType
	{
		UNLICENSED,
		FULL_LIFETIME,

		FULL_2_X,
		BETA_2_X,
		NODE_2_X,
		SDK_2_X,

		FULL_3_X,
		NODE_3_X,

		FULL_4_X,
		NODE_4_X,

		REVIT_3_X,
		REVIT_4_X,

		NETWORK_FLOATING_FULL,
		NETWORK_FLOATING_NODE,

		RT_3_X,
		RT_4_X,

		GREENBUTTON_CLOUD,

		INDIGO_RT_ICLONE // Restricted version of Indigo RT, works just with Reallusion's iClone software.
	};


	enum LicenceErrorCode
	{
		LicenceErrorCode_NoError = 0,
		LicenceErrorCode_NetworkLicenceFileNotFound = 1,
		LicenceErrorCode_LicenceSigFileNotFound = 2,
		LicenceErrorCode_FileReadFailed = 3,
		LicenceErrorCode_FileEmpty = 4,
		LicenceErrorCode_WrongNumComponents = 5,
		LicenceErrorCode_BadLicenceType = 6,
		LicenceErrorCode_EmptyHash = 7,
		LicenceErrorCode_NoHashMatch = 8,
		LicenceErrorCode_LicenceExpired = 9,
		LicenceErrorCode_MiscFileExcep = 10,
		LicenceErrorCode_IndigoRTUsedForIndigoRenderer = 11,
		LicenceErrorCode_OnlineLicenceFileNotFound = 12
	};


	static bool licenceIsForOldVersion(LicenceType t);
	/*
	Should we apply a watermark?
	This depends on the license type, and the date, in the case of the Beta licence type.
	*/
	static bool shouldApplyWatermark(LicenceType t);
	static bool shouldApplyResolutionLimits(LicenceType t);

	static unsigned int maxUnlicensedResolution();
	static bool dimensionsExceedLicenceDimensions(LicenceType t, int width, int height);

	static const std::string licenseTypeToString(LicenceType t);
	static const std::string licenseTypeToCodeString(LicenceType t);

	static void verifyLicense(const std::string& appdata_path, LicenceType& licence_type_out, std::string& user_id_out,
		LicenceErrorCode& local_err_code_out, LicenceErrorCode& network_lic_err_code_out); // throws LicenseExcep

	// A combination of the CPU type and MAC address
	static const std::string getPrimaryHardwareIdentifier(); // throws LicenseExcep
	static const std::vector<std::string> getHardwareIdentifiers(); // throws LicenseExcep

	// The MAC address used to be lower-case on OS X.  For backwards compatibility, this method exists to return the old lower-case MAC address primary hardware ID.
	static const std::string getOldPrimaryHardwareId();

	static const std::string networkFloatingHash(const std::string& input);

	static const std::string currentLicenseSummaryString(const std::string& appdata_path);

	static bool verifyKey(const std::string& key, std::string& hash);

	// License allocs some mem in globals when called (due to OpenSSL), so call before capturing start mem state in test suite.
	static void warmup();
	static void test();
private:
	
	static const std::string getLowerCaseMACAddrHardwareID(const std::string& hardware_id);
	
	static const std::string decodeBase64(const std::string& data);

	static void verifyLicenceString(const std::string& licence_string, const std::vector<std::string>& hardware_ids, LicenceType& licence_type_out, std::string& user_id_out, LicenceErrorCode& error_code_out);

	static const std::string ensureNewLinesPresent(const std::string& data);

	static bool tryVerifyNetworkLicence(const std::string& appdata_path, LicenceType& license_type_out, std::string& user_id_out, LicenceErrorCode& error_code_out);
	static bool tryVerifyOnlineLicence(const std::string& appdata_path, LicenceType& license_type_out, std::string& user_id_out, LicenceErrorCode& error_code_out);

	static bool verifyKey(const std::string& key, std::string& hash, const std::string& public_key);

};
