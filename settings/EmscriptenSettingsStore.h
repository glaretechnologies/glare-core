/*=====================================================================
EmscriptenSettingsStore.h
-------------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "SettingsStore.h"
#include <string>


/*=====================================================================
EmscriptenSettingsStore
-----------------------
When running under Emscripten, e.g. in the web browser, we will store the settings in localStorage.
See https://developer.mozilla.org/en-US/docs/Web/API/Web_Storage_API
=====================================================================*/
class EmscriptenSettingsStore final : public SettingsStore
{
public:
	EmscriptenSettingsStore();
	virtual ~EmscriptenSettingsStore();

	virtual bool		getBoolValue(const std::string& key, bool default_value) override;
	virtual void		setBoolValue(const std::string& key, bool value) override;

	virtual int			getIntValue(const std::string& key, int default_value) override;
	virtual void		setIntValue(const std::string& key, int value) override;

	virtual double		getDoubleValue(const std::string& key, double default_value) override;
	virtual void		setDoubleValue(const std::string& key, double value) override;

	virtual std::string getStringValue(const std::string& key, const std::string& default_value) override;
	virtual void		setStringValue(const std::string& key, const std::string& value) override;
};
