/*=====================================================================
XMLSettingsStore.cpp
------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "XMLSettingsStore.h"


#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/IndigoXMLDoc.h"
#include "../utils/FileUtils.h"
#include "../utils/XMLParseUtils.h"
#include "../utils/XMLWriteUtils.h"
#include "../utils/Exception.h"


XMLSettingsStore::XMLSettingsStore(const std::string& xml_path_)
:	xml_path(xml_path_),
	settings_dirty(false)
{
	// Try and load settings
	if(FileUtils::fileExists(xml_path))
	{
		try
		{
			IndigoXMLDoc xml_doc(xml_path);
			
			pugi::xml_node root = xml_doc.getRootElement();
			
			for(pugi::xml_node n = root.child("setting"); n; n = n.next_sibling("setting"))
			{
				settings[XMLParseUtils::parseString(n, "key")] = XMLParseUtils::parseString(n, "value");
			}
		}
		catch(glare::Exception& e)
		{
			conPrint("Warning: Error while loading XML settings: " + e.what());
		}
	}
}


XMLSettingsStore::~XMLSettingsStore()
{
	if(settings_dirty)
	{
		try
		{
			flushToDisk();
		}
		catch(glare::Exception&e)
		{
			conPrint("Warning: Error while writing XML settings: " + e.what());
		}
	}
}


bool XMLSettingsStore::getBoolValue(const std::string& key, bool default_value)
{
	auto res = settings.find(key);
	if(res == settings.end())
		return default_value;
	else
		return res->second == "true";
}


void XMLSettingsStore::setBoolValue(const std::string& key, bool value)
{
	settings[key] = boolToString(value);
	settings_dirty = true;
}


int	XMLSettingsStore::getIntValue(const std::string& key, int default_value)
{
	auto res = settings.find(key);
	if(res == settings.end())
		return default_value;
	else
		return stringToInt(res->second);
}


void XMLSettingsStore::setIntValue(const std::string& key, int value)
{
	settings[key] = int32ToString(value);
	settings_dirty = true;
}


double XMLSettingsStore::getDoubleValue(const std::string& key, double default_value)
{
	auto res = settings.find(key);
	if(res == settings.end())
		return default_value;
	else
		return stringToDouble(res->second);
}


void XMLSettingsStore::setDoubleValue(const std::string& key, double value)
{
	settings[key] = doubleToString(value);
	settings_dirty = true;
}


std::string XMLSettingsStore::getStringValue(const std::string& key, const std::string& default_value)
{
	auto res = settings.find(key);
	if(res == settings.end())
		return default_value;
	else
		return res->second;
}


void XMLSettingsStore::setStringValue(const std::string& key, const std::string& value)
{
	settings[key] = value;
	settings_dirty = true;
}


// The settings map is sorted, so the settings starting with a given prefix are contiguous in it.
std::vector<std::string> XMLSettingsStore::getMapKeys(const std::vector<std::string>& map_path)
{
	if(map_path.empty())
		throw glare::Exception("getMapKeys: map_path must not be empty");

	const std::string prefix = StringUtils::join(map_path, "/") + "/";

	std::vector<std::string> keys;
	for(auto it = settings.lower_bound(prefix); (it != settings.end()) && hasPrefix(it->first, prefix); ++it)
	{
		const size_t slash_pos = it->first.find('/', prefix.size());
		if(slash_pos != std::string::npos) // If this setting is a field of a map entry, rather than a value directly under map_path:
		{
			const std::string key = it->first.substr(prefix.size(), slash_pos - prefix.size());
			if(keys.empty() || (keys.back() != key)) // The fields of an entry are contiguous, so only need to check the last key added.
				keys.push_back(key);
		}
	}
	return keys;
}


void XMLSettingsStore::removeMapEntry(const std::vector<std::string>& map_path, const std::string& key)
{
	if(map_path.empty())
		throw glare::Exception("removeMapEntry: map_path must not be empty");

	const std::string prefix = StringUtils::join(map_path, "/") + "/" + key + "/";

	auto it = settings.lower_bound(prefix);
	while((it != settings.end()) && hasPrefix(it->first, prefix))
	{
		it = settings.erase(it);
		settings_dirty = true;
	}
}


void XMLSettingsStore::flushToDisk()
{
	std::string s = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\" ?>\n";
	s += "<settings>\n";

	for(auto it = settings.begin(); it != settings.end(); ++it)
	{
		s += "\t<setting>\n";
		XMLWriteUtils::writeStringElemToXML(s, "key",   it->first,  /*tab depth=*/2);
		XMLWriteUtils::writeStringElemToXML(s, "value", it->second, /*tab depth=*/2);
		s += "\t</setting>\n";
	}

	s += "</settings>\n";

	FileUtils::writeEntireFileTextMode(xml_path, s);

	settings_dirty = false;
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/PlatformUtils.h"


void XMLSettingsStore::test()
{
	{
		const std::string path = PlatformUtils::getTempDirPath() + "/test_store.xml";
		{
			Reference<XMLSettingsStore> store = new XMLSettingsStore(path);
			store->setBoolValue("a", true);
			store->setIntValue("b", 123);
			store->setDoubleValue("c", 123.456);
			store->setStringValue("d", "hello");
		}
		{
			Reference<XMLSettingsStore> store = new XMLSettingsStore(path);
			testAssert(store->getBoolValue("a", /*default value=*/false) == true);
			testAssert(store->getIntValue("b", /*default value=*/0) == 123);
			testAssert(store->getDoubleValue("c", /*default value=*/0) == 123.456);
			testAssert(store->getStringValue("d", /*default value=*/"") == "hello");
			
			// Test default value usage
			testAssert(store->getBoolValue("XXa", /*default value=*/false) == false);
			testAssert(store->getIntValue("XXb", /*default value=*/-1) == -1);
			testAssert(store->getDoubleValue("XXc", /*default value=*/876.123) == 876.123);
			testAssert(store->getStringValue("XXd", /*default value=*/"bleh") == "bleh");
		}
	}

	// Test maps
	{
		const std::string path = PlatformUtils::getTempDirPath() + "/test_store_maps.xml";
		if(FileUtils::fileExists(path))
			FileUtils::deleteFile(path);

		const std::vector<std::string> map_path({"hidden_list", "hidden_objects"});
		{
			Reference<XMLSettingsStore> store = new XMLSettingsStore(path);

			// Map doesn't exist
			testAssert(store->getMapKeys(map_path).empty());

			store->setDoubleValue("hidden_list/hidden_objects/120/hidden_time", 1.0);
			store->setStringValue("hidden_list/hidden_objects/120/creator_name", "alice");
			store->setDoubleValue("hidden_list/hidden_objects/130/hidden_time", 2.0);
			store->setStringValue("hidden_list/hidden_objects/direct_value", "x"); // A value directly in the map, which is not an entry.
			store->setStringValue("hidden_list/hidden_objects_other/140/hidden_time", "3"); // In a different map whose path starts with the same characters.

			// Entries with multiple fields are only returned once.
			testAssert(store->getMapKeys(map_path) == std::vector<std::string>({"120", "130"}));
		}
		{
			// Check the map is kept after saving and reloading.
			Reference<XMLSettingsStore> store = new XMLSettingsStore(path);
			testAssert(store->getMapKeys(map_path) == std::vector<std::string>({"120", "130"}));
			testAssert(store->getStringValue("hidden_list/hidden_objects/120/creator_name", "") == "alice");

			// Remove an entry with multiple fields
			store->removeMapEntry(map_path, "120");
			testAssert(store->getMapKeys(map_path) == std::vector<std::string>({"130"}));
			testAssert(store->getStringValue("hidden_list/hidden_objects/120/creator_name", "default") == "default");

			// Removing a non-existent entry does nothing
			store->removeMapEntry(map_path, "999");
			testAssert(store->getMapKeys(map_path) == std::vector<std::string>({"130"}));

			// Values outside the removed entry are kept.
			testAssert(store->getStringValue("hidden_list/hidden_objects/direct_value", "") == "x");
			testAssert(store->getMapKeys(std::vector<std::string>({"hidden_list", "hidden_objects_other"})) == std::vector<std::string>({"140"}));

			// An empty map path is an error.
			try
			{
				store->getMapKeys(std::vector<std::string>());
				failTest("Expected exception");
			}
			catch(glare::Exception&)
			{}
			try
			{
				store->removeMapEntry(std::vector<std::string>(), "130");
				failTest("Expected exception");
			}
			catch(glare::Exception&)
			{}
		}
		{
			// Check the removal is kept after saving and reloading.
			Reference<XMLSettingsStore> store = new XMLSettingsStore(path);
			testAssert(store->getMapKeys(map_path) == std::vector<std::string>({"130"}));
		}
	}
}


#endif // BUILD_TESTS
