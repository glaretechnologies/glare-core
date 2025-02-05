/*=====================================================================
EmscriptenSettingsStore.cpp
---------------------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "EmscriptenSettingsStore.h"


#include <utils/PlatformUtils.h>
#include <utils/StringUtils.h>
#include <utils/ConPrint.h>
#if EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/html5.h>
#include <string>
#endif


EmscriptenSettingsStore::EmscriptenSettingsStore()
{}


EmscriptenSettingsStore::~EmscriptenSettingsStore()
{}


#if EMSCRIPTEN

// When running under Emscripten, e.g. in the web browser, we will store the settings in localStorage.  See https://developer.mozilla.org/en-US/docs/Web/API/Web_Storage_API


// Define getLocalStorageKeyValue(const char* key) function
EM_JS(char*, getLocalStorageKeyValue, (const char* key), {
	const res = localStorage.getItem(UTF8ToString(key));
	if(res == null)
		return null;
	else
		return stringToNewUTF8(res);
});


// Define setLocalStorageKeyValue(const char* key, const char* value) function
EM_JS(void, setLocalStorageKeyValue, (const char* key, const char* value), {
	localStorage.setItem(UTF8ToString(key), UTF8ToString(value));
});


bool EmscriptenSettingsStore::getBoolValue(const std::string& key, bool default_value)
{
	try
	{
		char* val = getLocalStorageKeyValue(key.c_str());
		if(val == NULL)
			return default_value;
		else
		{
			const bool bool_val = stringEqual(val, "true");
			free(val);
			return bool_val;
		}
	}
	catch(glare::Exception& e)
	{
		conPrint("Warning: failed to get bool setting: " + e.what());
		return default_value;
	}
}


void EmscriptenSettingsStore::setBoolValue(const std::string& key, bool value)
{
	setLocalStorageKeyValue(key.c_str(), value ? "true" : "false");
}


int EmscriptenSettingsStore::getIntValue(const std::string& key, int default_value)
{
	try
	{
		char* val = getLocalStorageKeyValue(key.c_str());
		if(val == NULL)
			return default_value;
		else
		{
			const int int_val = stringToInt(std::string(val));
			free(val);
			return int_val;
		}
	}
	catch(glare::Exception& e)
	{
		conPrint("Warning: failed to get int setting: " + e.what());
		return default_value;
	}
}


void EmscriptenSettingsStore::setIntValue(const std::string& key, int value)
{
	setLocalStorageKeyValue(key.c_str(), toString(value).c_str());
}


double EmscriptenSettingsStore::getDoubleValue(const std::string& key, double default_value)
{
	try
	{
		char* val = getLocalStorageKeyValue(key.c_str());
		if(val == NULL)
			return default_value;
		else
		{
			const double double_val = stringToDouble(std::string(val));
			free(val);
			return double_val;
		}
	}
	catch(glare::Exception& e)
	{
		conPrint("Warning: failed to get double setting: " + e.what());
		return default_value;
	}
}


void EmscriptenSettingsStore::setDoubleValue(const std::string& key, double value)
{
	setLocalStorageKeyValue(key.c_str(), doubleToString(value).c_str());
}


std::string EmscriptenSettingsStore::getStringValue(const std::string& key, const std::string& default_value)
{
	try
	{
		char* val = getLocalStorageKeyValue(key.c_str());
		if(val == NULL)
			return default_value;
		else
		{
			const std::string string_val(val);
			free(val);
			return string_val;
		}
	}
	catch(glare::Exception& e)
	{
		conPrint("Warning: failed to get sting setting: " + e.what());
		return default_value;
	}
}


void EmscriptenSettingsStore::setStringValue(const std::string& key, const std::string& value)
{
	setLocalStorageKeyValue(key.c_str(), value.c_str());
}


#endif
