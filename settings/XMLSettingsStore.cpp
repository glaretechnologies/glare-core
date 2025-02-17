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
}


#endif // BUILD_TESTS
