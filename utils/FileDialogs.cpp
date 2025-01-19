/*=====================================================================
FileDialogs.cpp
---------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "FileDialogs.h"


#include "StringUtils.h"
#include "PlatformUtils.h"
#if defined(_WIN32)
#include "IncludeWindows.h"
#include "ComObHandle.h"
#include <shobjidl.h> 
#endif


#if defined(_WIN32)
template <class T>
static void setFileTypes(ComObHandle<T>& dialog, const FileDialogs::Options& options)
{
	// Set file types
	std::vector<COMDLG_FILTERSPEC> filterspecs(options.file_types.size());
	std::vector<std::wstring> names(options.file_types.size());
	std::vector<std::wstring> specs(options.file_types.size());
	for(size_t i=0; i<options.file_types.size(); ++i)
	{
		names[i] = StringUtils::UTF8ToPlatformUnicodeEncoding(options.file_types[i].name);
		specs[i] = StringUtils::UTF8ToPlatformUnicodeEncoding(options.file_types[i].filter);
		filterspecs[i].pszName = names[i].c_str();
		filterspecs[i].pszSpec = specs[i].c_str();
	}
	HRESULT res = dialog->SetFileTypes((UINT)filterspecs.size(), filterspecs.data());
	if(FAILED(res))
		throw glare::Exception("SetFileTypes failed: " + PlatformUtils::COMErrorString(res));
}
#endif


std::string FileDialogs::showOpenFileDialog(const Options& options)
{
#if defined(_WIN32)
	// Adapted from https://stackoverflow.com/questions/68601080/how-do-you-open-a-file-explorer-dialogue-in-c

	ComObHandle<IFileOpenDialog> file_open_dialog;
	HRESULT res = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, (void**)&file_open_dialog.ptr);
	if(FAILED(res))
		throw glare::Exception("Failed to create FileOpenDialog COM object: " + PlatformUtils::COMErrorString(res));

	if(!options.dialog_title.empty())
		file_open_dialog->SetTitle(StringUtils::UTF8ToPlatformUnicodeEncoding(options.dialog_title).c_str());

	setFileTypes(file_open_dialog, options);

	// Show open file dialog window
	res = file_open_dialog->Show(NULL);
	if(res == HRESULT_FROM_WIN32(ERROR_CANCELLED)) // If the user closed the window by cancelling the operation:
		return "";
	else if(FAILED(res))
		throw glare::Exception("Failed to show FileOpenDialog: " + PlatformUtils::COMErrorString(res));

	// Get the choice that the user made in the dialog
	ComObHandle<IShellItem> files;
	res = file_open_dialog->GetResult(&files.ptr);
	if(FAILED(res))
		throw glare::Exception("GetResult failed");

	// Get the filename
	PWSTR path_cstr;
	res = files->GetDisplayName(SIGDN_FILESYSPATH, &path_cstr);
	if(FAILED(res))
		throw glare::Exception("GetDisplayName failed: " + PlatformUtils::COMErrorString(res));

	const std::wstring wpath(path_cstr);
	CoTaskMemFree(path_cstr); // We need to manually free this string

	const std::string path = StringUtils::PlatformToUTF8UnicodeEncoding(wpath);

	return path;
#else
	throw glare::Exception("FileDialogs::showOpenFileDialog not implemented");
#endif
}


std::string FileDialogs::showSaveFileDialog(const Options& options)
{
#if defined(_WIN32)
	// Adapted from https://stackoverflow.com/questions/68601080/how-do-you-open-a-file-explorer-dialogue-in-c

	ComObHandle<IFileSaveDialog> file_save_dialog;
	HRESULT res = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_ALL, IID_IFileSaveDialog, (void**)&file_save_dialog.ptr);
	if(FAILED(res))
		throw glare::Exception("Failed to create FileSaveDialog COM object: " + PlatformUtils::COMErrorString(res));

	if(!options.dialog_title.empty())
		file_save_dialog->SetTitle(StringUtils::UTF8ToPlatformUnicodeEncoding(options.dialog_title).c_str());

	setFileTypes(file_save_dialog, options);

	// Show open file dialog window
	res = file_save_dialog->Show(NULL);
	if(res == HRESULT_FROM_WIN32(ERROR_CANCELLED)) // If the user closed the window by cancelling the operation:
		return "";
	else if(FAILED(res))
		throw glare::Exception("Failed to show FileSaveDialog: " + PlatformUtils::COMErrorString(res));

	// Get the choice that the user made in the dialog
	ComObHandle<IShellItem> files;
	res = file_save_dialog->GetResult(&files.ptr);
	if(FAILED(res))
		throw glare::Exception("GetResult failed");

	// Get the filename
	PWSTR path_cstr;
	res = files->GetDisplayName(SIGDN_FILESYSPATH, &path_cstr);
	if(FAILED(res))
		throw glare::Exception("GetDisplayName failed: " + PlatformUtils::COMErrorString(res));

	const std::wstring wpath(path_cstr);
	CoTaskMemFree(path_cstr); // We need to manually free this string

	std::string path = StringUtils::PlatformToUTF8UnicodeEncoding(wpath);

	// If the file doesn't have an extension, add one:
	if(path.find('.') == std::string::npos)
	{
		// Get selected file type index (1-based):
		UINT file_type_index = 1;
		file_save_dialog->GetFileTypeIndex(&file_type_index);
		const size_t index = (size_t)(file_type_index - 1);
		if(index < options.file_types.size())
		{
			path += "." + options.file_types[index].extension;
		}
	}


	return path;
#else
	throw glare::Exception("FileDialogs::showSaveFileDialog not implemented");
#endif
}
