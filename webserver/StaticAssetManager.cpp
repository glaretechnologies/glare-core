/*=====================================================================
StaticAssetManager.cpp
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-04-14 15:39:35 +0100
=====================================================================*/
#include "StaticAssetManager.h"


#include "WebsiteExcep.h"
#include <MemMappedFile.h>
#include <Exception.h>
#include <SHA256.h>
#include <FileUtils.h>
#include <StringUtils.h>
#include <ConPrint.h>


namespace web
{


StaticAsset::StaticAsset(const std::string& disk_path_, const std::string& mime_type_)
:	disk_path(disk_path_),
	mime_type(mime_type_)
{
	try
	{
		file = new MemMappedFile(disk_path);

		// Compute ETag as the base64 encoded hash of the file

		//std::vector<unsigned char> digest;
		//SHA256::hash((const unsigned char*)file->fileData(), (const unsigned char*)file->fileData() + file->fileSize(), digest);

		//Base64::encode(&digest[0], digest.size(), this->etag);
	}
	catch(glare::Exception& e)
	{
		throw WebsiteExcep(e.what());
	}
}


StaticAsset::~StaticAsset()
{
	delete file;
}


StaticAssetManager::StaticAssetManager()
{}


StaticAssetManager::~StaticAssetManager()
{}


void StaticAssetManager::addStaticAsset(const std::string& URL_path, StaticAssetRef static_asset)
{
	conPrint("Adding static asset mapping '" + URL_path + "' => '" + static_asset->disk_path + "'.");

	static_assets[URL_path] = static_asset;
}


void StaticAssetManager::addAllFilesInDir(const std::string& URL_path_prefix, const std::string& dir)
{
	try
	{
		const std::vector<std::string> files = FileUtils::getFilesInDir(dir);
		for(size_t i=0; i<files.size(); ++i)
		{
			if(::hasExtension(files[i], "png"))
			{
				const std::string URL_path = URL_path_prefix + FileUtils::getFilename(files[i]);
				const std::string disk_path = FileUtils::join(dir, files[i]);

				conPrint("Adding static asset mapping '" + URL_path + "' => '" + disk_path + "'.");

				static_assets[URL_path] = new StaticAsset(disk_path, "image/png");
			}
		}
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw WebsiteExcep(e.what());
	}
}


} // end namespace web
