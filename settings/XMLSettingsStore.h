/*=====================================================================
XMLSettingsStore.h
------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "SettingsStore.h"
#include <map>


/*=====================================================================
XMLSettingsStore
----------------
Implements the SettingsStore interface using an XML file.
=====================================================================*/
class XMLSettingsStore final : public SettingsStore
{
public:
	XMLSettingsStore(const std::string& xml_path);
	virtual ~XMLSettingsStore();

	virtual bool		getBoolValue(const std::string& key, bool default_value) override;
	virtual void		setBoolValue(const std::string& key, bool value) override;

	virtual int			getIntValue(const std::string& key, int default_value) override;
	virtual void		setIntValue(const std::string& key, int value) override;

	virtual double		getDoubleValue(const std::string& key, double default_value) override;
	virtual void		setDoubleValue(const std::string& key, double value) override;

	virtual std::string getStringValue(const std::string& key, const std::string& default_value) override;
	virtual void		setStringValue(const std::string& key, const std::string& value) override;

	static void test();
private:
	void flushToDisk();
	
	std::string xml_path;
	
	std::map<std::string, std::string> settings;
	
	bool settings_dirty;
};
