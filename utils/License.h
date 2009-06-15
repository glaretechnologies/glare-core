/*=====================================================================
License.h
---------
File created by ClassTemplate on Thu Mar 19 14:06:32 2009
=====================================================================*/
#ifndef __LICENSE_H_666_
#define __LICENSE_H_666_


#include <string>


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
		FULL,
		BETA,
		NODE,
		FULL_LIFETIME
	};


	/*
	Should we apply a watermark?
	This depends on the license type, and the date, in the case of the Beta licence type.
	*/
	static bool shouldApplyWatermark(LicenceType t);
	static bool shouldApplyResolutionLimits(LicenceType t);

	static const std::string licenseTypeToString(LicenceType t);

	static void verifyLicense(const std::string& indigo_base_path, LicenceType& license_type_out, std::string& user_id_out); // throws LicenseExcep

	// A combination of the CPU type and MAC address
	static const std::string getHardwareIdentifier(); // throws LicenseExcep


	static const std::string currentLicenseSummaryString(const std::string& indigo_base_dir_path);

	static bool verifyKey(const std::string& key, const std::string& hash);

	static const std::string decodeBase64(const std::string& data);

	static void test();
private:
	static const std::string ensureNewLinesPresent(const std::string& data);
};


#endif //__LICENSE_H_666_
