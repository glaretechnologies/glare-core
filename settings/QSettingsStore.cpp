/*=====================================================================
QSettingsStore.cpp
------------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "QSettingsStore.h"


#include "../qt/QtUtils.h"
#include "../utils/StringUtils.h"
#include "../utils/Exception.h"
#include <QtCore/QSettings>
#include <QtCore/QStringList>


QSettingsStore::QSettingsStore(QSettings* settings_)
:	settings(settings_)
{}


QSettingsStore::~QSettingsStore()
{}


bool QSettingsStore::getBoolValue(const std::string& key, bool default_value)
{
	return settings->value(QtUtils::toQString(key), default_value).toBool();
}


void QSettingsStore::setBoolValue(const std::string& key, bool value)
{
	settings->setValue(QtUtils::toQString(key), value);
}


int	QSettingsStore::getIntValue(const std::string& key, int default_value)
{
	return settings->value(QtUtils::toQString(key), default_value).toInt();
}


void QSettingsStore::setIntValue(const std::string& key, int value)
{
	settings->setValue(QtUtils::toQString(key), value);
}


double QSettingsStore::getDoubleValue(const std::string& key, double default_value)
{
	return settings->value(QtUtils::toQString(key), default_value).toDouble();
}


void QSettingsStore::setDoubleValue(const std::string& key, double value)
{
	settings->setValue(QtUtils::toQString(key), value);
}


std::string QSettingsStore::getStringValue(const std::string& key, const std::string& default_value)
{
	return QtUtils::toStdString(settings->value(QtUtils::toQString(key), QtUtils::toQString(default_value)).toString());
}


void QSettingsStore::setStringValue(const std::string& key, const std::string& value)
{
	settings->setValue(QtUtils::toQString(key), QtUtils::toQString(value));
}


// Map entries are QSettings groups under the map's group.
std::vector<std::string> QSettingsStore::getMapKeys(const std::vector<std::string>& map_path)
{
	if(map_path.empty())
		throw glare::Exception("getMapKeys: map_path must not be empty");

	settings->beginGroup(QtUtils::toQString(StringUtils::join(map_path, "/")));
	const QStringList groups = settings->childGroups();
	settings->endGroup();

	std::vector<std::string> keys(groups.size());
	for(int i=0; i<groups.size(); ++i)
		keys[i] = QtUtils::toStdString(groups[i]);
	return keys;
}


void QSettingsStore::removeMapEntry(const std::vector<std::string>& map_path, const std::string& key)
{
	if(map_path.empty())
		throw glare::Exception("removeMapEntry: map_path must not be empty");

	settings->remove(QtUtils::toQString(StringUtils::join(map_path, "/") + "/" + key)); // Removes the entry group and all settings in it.
}
