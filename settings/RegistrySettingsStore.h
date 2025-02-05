/*=====================================================================
RegistrySettingsStore.h
-----------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "SettingsStore.h"
#include <utils/Platform.h>
#include <string>


/*=====================================================================
RegistrySettingsStore
---------------------
Win32 only
=====================================================================*/
class RegistrySettingsStore final : public SettingsStore
{
public:
	// organisation_name: e.g. "Glare Technologies"
	// app_name: e.g. "Substrata"
	RegistrySettingsStore(const std::string& organisation_name, const std::string& app_name);
	virtual ~RegistrySettingsStore();

	virtual bool		getBoolValue(const std::string& key, bool default_value) override;
	virtual void		setBoolValue(const std::string& key, bool value) override;

	virtual int			getIntValue(const std::string& key, int default_value) override;
	virtual void		setIntValue(const std::string& key, int value) override;

	virtual double		getDoubleValue(const std::string& key, double default_value) override;
	virtual void		setDoubleValue(const std::string& key, double value) override;

	virtual std::string getStringValue(const std::string& key, const std::string& default_value) override;
	virtual void		setStringValue(const std::string& key, const std::string& value) override;

private:
	void getRegSubKeyAndValueName(const std::string& setting_key, std::string& subkey_out, std::string& value_name_out);
	bool doesRegValueExist(const std::string& setting_key);
	std::string getRegStringVal(const std::string& setting_key);
	uint32 getRegDWordVal(const std::string& setting_key);

	std::string organisation_name, app_name;
};
