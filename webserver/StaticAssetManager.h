/*=====================================================================
StaticAssetManager.h
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-04-14 15:39:35 +0100
=====================================================================*/
#pragma once


#include <ThreadSafeRefCounted.h>
#include <Reference.h>
#include <string>
#include <vector>
#include <map>
class MemMappedFile;


namespace web
{


/*=====================================================================
StaticAssetManager
-------------------

=====================================================================*/
class StaticAsset : public ThreadSafeRefCounted
{
public:
	StaticAsset(const std::string& disk_path, const std::string& mime_type);
	~StaticAsset();

	std::string disk_path;
	std::string mime_type;
	//std::string etag;
	MemMappedFile* file;
};
typedef Reference<StaticAsset> StaticAssetRef;


class StaticAssetManager
{
public:
	StaticAssetManager();
	~StaticAssetManager();

	void addStaticAsset(const std::string& URL_path, StaticAssetRef static_asset);

	void addAllFilesInDir(const std::string& URL_path_prefix, const std::string& dir);
	
	const std::map<std::string, StaticAssetRef>& getStaticAssets() const { return static_assets; }
private:
	std::map<std::string, StaticAssetRef> static_assets;
};


} // end namespace web
