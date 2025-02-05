/*=====================================================================
RegistrySettingsStore.cpp
-------------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "RegistrySettingsStore.h"


#include <utils/PlatformUtils.h>
#include <utils/StringUtils.h>
#include <utils/ConPrint.h>


RegistrySettingsStore::RegistrySettingsStore(const std::string& organisation_name_, const std::string& app_name_)
:	organisation_name(organisation_name_), app_name(app_name_)
{}


RegistrySettingsStore::~RegistrySettingsStore()
{}


#ifdef _WIN32


void RegistrySettingsStore::getRegSubKeyAndValueName(const std::string& setting_key, std::string& subkey_out, std::string& value_name_out)
{
	std::vector<std::string> keyparts = ::split(setting_key, '/');
	if(keyparts.size() == 0)
		throw glare::Exception("setting_key.size() == 0");

	subkey_out = "SOFTWARE\\" + organisation_name + "\\" + app_name + "\\";
	for(size_t i=0; i+1<keyparts.size(); ++i)
	{
		subkey_out += keyparts[i];
		if(i + 2 < keyparts.size())
			subkey_out += "\\";
	}
	value_name_out = keyparts.back();
}


bool RegistrySettingsStore::doesRegValueExist(const std::string& setting_key)
{
	std::string subkey, value_name;
	getRegSubKeyAndValueName(setting_key, subkey, value_name);
	return PlatformUtils::doesRegKeyAndValueExist(PlatformUtils::RegHKey_CurrentUser, subkey, value_name);
}


std::string RegistrySettingsStore::getRegStringVal(const std::string& setting_key)
{
	std::string subkey, value_name;
	getRegSubKeyAndValueName(setting_key, subkey, value_name);

	const std::string stringval = PlatformUtils::getStringRegKey(PlatformUtils::RegHKey_CurrentUser, subkey, value_name);
	return stringval;

}


uint32 RegistrySettingsStore::getRegDWordVal(const std::string& setting_key)
{
	std::string subkey, value_name;
	getRegSubKeyAndValueName(setting_key, subkey, value_name);
	return PlatformUtils::getDWordRegKey(PlatformUtils::RegHKey_CurrentUser, subkey, value_name);
}


bool RegistrySettingsStore::getBoolValue(const std::string& key, bool default_value)
{
	try
	{
		if(doesRegValueExist(key))
			return getRegStringVal(key) == "true";
		else
			return default_value;
	}
	catch(glare::Exception& e)
	{
		conPrint("Warning: failed to get bool setting: " + e.what());
		return default_value;
	}
}


void RegistrySettingsStore::setBoolValue(const std::string& setting_key, bool value)
{
	std::string subkey, value_name;
	getRegSubKeyAndValueName(setting_key, subkey, value_name);
	PlatformUtils::setStringRegKey(PlatformUtils::RegHKey_CurrentUser, subkey, value_name, /*new valuedata=*/value ? "true" : "false");
}


int RegistrySettingsStore::getIntValue(const std::string& key, int default_value)
{
	try
	{
		if(doesRegValueExist(key))
		{
			// Registry only stores unsigned ints.  Just copy bits directly to get our signed int (should be stored in similar way)
			const uint32 dword_val = getRegDWordVal(key);
			int32 int_val;
			std::memcpy(&int_val, &dword_val, 4);
			return int_val;
		}
		else
			return default_value;
	}
	catch(glare::Exception& e)
	{
		conPrint("Warning: failed to get int setting: " + e.what());
		return default_value;
	}
}


void RegistrySettingsStore::setIntValue(const std::string& setting_key, int value)
{
	// Registry only stores unsigned ints.  Just copy bits directly to get our signed int
	uint32 dword_val;
	std::memcpy(&dword_val, &value, 4);
	std::string subkey, value_name;
	getRegSubKeyAndValueName(setting_key, subkey, value_name);
	PlatformUtils::setDWordRegKey(PlatformUtils::RegHKey_CurrentUser, subkey, value_name, dword_val);
}


double RegistrySettingsStore::getDoubleValue(const std::string& key, double default_value)
{
	try
	{
		if(doesRegValueExist(key))
			return stringToDouble(getRegStringVal(key));
		else
			return default_value;
	}
	catch(glare::Exception& e)
	{
		conPrint("Warning: failed to get double setting: " + e.what());
		return default_value;
	}
}


void RegistrySettingsStore::setDoubleValue(const std::string& setting_key, double value)
{
	std::string subkey, value_name;
	getRegSubKeyAndValueName(setting_key, subkey, value_name);

	const std::string value_string = ::doubleToString(value);
	PlatformUtils::setStringRegKey(PlatformUtils::RegHKey_CurrentUser, subkey, value_name, /*new valuedata=*/value_string);
}


std::string RegistrySettingsStore::getStringValue(const std::string& key, const std::string& default_value)
{
	try
	{
		if(doesRegValueExist(key))
			return getRegStringVal(key);
		else
			return default_value;
	}
	catch(glare::Exception& e)
	{
		conPrint("Warning: failed to get string setting: " + e.what());
		return default_value;
	}
}


void RegistrySettingsStore::setStringValue(const std::string& setting_key, const std::string& value)
{
	std::string subkey, value_name;
	getRegSubKeyAndValueName(setting_key, subkey, value_name);
	PlatformUtils::setStringRegKey(PlatformUtils::RegHKey_CurrentUser, subkey, value_name, /*new valuedata=*/value);
}


#endif // _WIN32
