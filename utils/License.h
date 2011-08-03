/*=====================================================================
License.h
---------
File created by ClassTemplate on Thu Mar 19 14:06:32 2009
=====================================================================*/
#ifndef __LICENSE_H_666_
#define __LICENSE_H_666_


#include <string>
#include <vector>


/*=====================================================================
License
-------

=====================================================================*/
class License
{
public:

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

		REVIT_3_X,
		
		NETWORK_FLOATING_FULL,
		NETWORK_FLOATING_NODE,

		RT_3_X
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

	static void verifyLicense(const std::string& appdata_path, LicenceType& license_type_out, std::string& user_id_out); // throws LicenseExcep

	// A combination of the CPU type and MAC address
	static const std::string getPrimaryHardwareIdentifier(); // throws LicenseExcep
	static const std::vector<std::string> getHardwareIdentifiers(); // throws LicenseExcep


	static const std::string currentLicenseSummaryString(const std::string& appdata_path);

	static bool verifyKey(const std::string& key, const std::string& hash);
	static const std::string decodeBase64(const std::string& data);

	static const std::string networkFloatingHash(const std::string& input);

	static void test();
private:
	static const std::string ensureNewLinesPresent(const std::string& data);

	static bool tryVerifyNetworkLicence(const std::string& appdata_path, LicenceType& license_type_out, std::string& user_id_out);

};


#endif //__LICENSE_H_666_
