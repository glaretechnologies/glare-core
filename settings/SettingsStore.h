/*=====================================================================
SettingsStore.h
---------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "../utils/RefCounted.h"
#include <string>
#include <vector>


/*=====================================================================
SettingsStore
-------------
Interface for a store for settings, like QSettings.
=====================================================================*/
class SettingsStore : public RefCounted
{
public:
	virtual ~SettingsStore();

	virtual bool		getBoolValue(const std::string& key, bool default_value) = 0;
	virtual void		setBoolValue(const std::string& key, bool value) = 0;

	virtual int			getIntValue(const std::string& key, int default_value) = 0;
	virtual void		setIntValue(const std::string& key, int value) = 0;

	virtual double		getDoubleValue(const std::string& key, double default_value) = 0;
	virtual void		setDoubleValue(const std::string& key, double value) = 0;

	virtual std::string getStringValue(const std::string& key, const std::string& default_value) = 0;
	virtual void		setStringValue(const std::string& key, const std::string& value) = 0;

	// Maps are stored with the fields of each entry under map_path/key/, e.g. "hidden_list/hidden_objects/120/hidden_time".
	// Fields are set and read with the methods above.  An entry exists while it has at least one field.
	// map_path is passed as its path components, e.g. {"hidden_list", "hidden_objects"}.
	// Both map methods throw glare::Exception if map_path is empty.

	// Returns the keys of the map at map_path, e.g. "120" for the example above.  Returns an empty vector if the map doesn't exist.
	virtual std::vector<std::string> getMapKeys(const std::vector<std::string>& map_path) = 0;

	// Removes the entry for key from the map at map_path, including all its fields.  Does nothing if there is no such entry.
	virtual void removeMapEntry(const std::vector<std::string>& map_path, const std::string& key) = 0;
};
