/*=====================================================================
QtUtils.h
---------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include <QtCore/QString>
#include "../utils/string_view.h"
class QWidget;
class QCheckBox;
class ArgumentParser;
class QLayout;
class QMainWindow;
namespace Indigo { class String; }


/*=====================================================================
QtUtils
-------

=====================================================================*/
namespace QtUtils
{

/*
	Convert an Indigo string into a QT string.
*/
const QString toQString(const string_view s);


const QString toQString(const Indigo::String& s);


/*
	Convert a QT string to an std::string.
*/
const std::string toIndString(const QString& s);
const std::string toStdString(const QString& s); // Same as toIndString()

/*
	Convert a QT string to an Indigo::String.
*/
const Indigo::String toIndigoString(const QString& s);

/*
	Returns the suppported image filters for loading images,
*/
const QString imageLoadFilters();

void showErrorMessageDialog(const std::string& error_msg, QWidget* parent = NULL);
void showErrorMessageDialog(const QString& error_msg, QWidget* parent = NULL);

/*
backgroundModeCheckBox may be NULL.
*/
void setProcessPriority(const ArgumentParser& parsed_args, QCheckBox* backgroundModeCheckBox);

void ClearLayout(QLayout* layout, bool deleteWidgets);
void RemoveLayout(QWidget* widget);


// Escape a string for HTML. (replace "<" with "&lt;" etc..)
const std::string htmlEscape(const std::string& s);

const std::string urlEscape(const std::string& s);


// If window was minimised, restore to non-minimised state.
void unminimiseWindow(QMainWindow* window);


} // end namespace QtUtils


