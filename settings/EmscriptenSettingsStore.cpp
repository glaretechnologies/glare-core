/*=====================================================================
EmscriptenSettingsStore.cpp
---------------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "EmscriptenSettingsStore.h"
#include "../utils/Exception.h"


#if EMSCRIPTEN


#include <utils/PlatformUtils.h>
#include <utils/StringUtils.h>
#include <utils/ConPrint.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <string>
#include <set>


EmscriptenSettingsStore::EmscriptenSettingsStore()
{}


EmscriptenSettingsStore::~EmscriptenSettingsStore()
{}


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


// Define getLocalStorageNumKeys() function
EM_JS(int, getLocalStorageNumKeys, (), {
	return localStorage.length;
});


// Define getLocalStorageKey(int i) function.  Returns the i-th key, or null if i is out of range.  The returned string must be freed.
EM_JS(char*, getLocalStorageKey, (int i), {
	const res = localStorage.key(i);
	if(res == null)
		return null;
	else
		return stringToNewUTF8(res);
});


// Define removeLocalStorageKeysWithPrefix(const char* prefix) function
EM_JS(void, removeLocalStorageKeysWithPrefix, (const char* prefix), {
	const prefix_str = UTF8ToString(prefix);
	const keys_to_remove = [];
	for(let i=0; i<localStorage.length; ++i)
	{
		const key = localStorage.key(i);
		if(key.startsWith(prefix_str))
			keys_to_remove.push(key);
	}
	// Remove after iterating, as removing items changes the key indices.
	for(const key of keys_to_remove)
		localStorage.removeItem(key);
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


std::vector<std::string> EmscriptenSettingsStore::getMapKeys(const std::vector<std::string>& map_path)
{
	if(map_path.empty())
		throw glare::Exception("getMapKeys: map_path must not be empty");

	const std::string prefix = StringUtils::join(map_path, "/") + "/";

	// localStorage keys are in no particular order, so use a set to remove the duplicate keys from entries with multiple fields.
	std::set<std::string> keys;
	const int num_keys = getLocalStorageNumKeys();
	for(int i=0; i<num_keys; ++i)
	{
		char* storage_key = getLocalStorageKey(i);
		if(storage_key)
		{
			const std::string storage_key_str(storage_key);
			free(storage_key);

			if(hasPrefix(storage_key_str, prefix))
			{
				const size_t slash_pos = storage_key_str.find('/', prefix.size());
				if(slash_pos != std::string::npos) // If this setting is a field of a map entry, rather than a value directly under map_path:
					keys.insert(storage_key_str.substr(prefix.size(), slash_pos - prefix.size()));
			}
		}
	}

	return std::vector<std::string>(keys.begin(), keys.end());
}


void EmscriptenSettingsStore::removeMapEntry(const std::vector<std::string>& map_path, const std::string& key)
{
	if(map_path.empty())
		throw glare::Exception("removeMapEntry: map_path must not be empty");

	removeLocalStorageKeysWithPrefix((StringUtils::join(map_path, "/") + "/" + key + "/").c_str());
}


#endif
