/*=====================================================================
TextureServer.h
---------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "../utils/NameMap.h"
#include "../utils/Reference.h"
#include "../utils/Mutex.h"
#include "../utils/Exception.h"
#include "../utils/ThreadSafeRefCounted.h"
#include <string>
#include <unordered_set>
class Map2D;


class TextureServerExcep : public glare::Exception
{
public:
	enum ErrorType
	{
		ErrorType_Misc,
		ErrorType_FileNotFound
	};

	TextureServerExcep(const std::string& msg_, ErrorType error_type_) : glare::Exception(msg_), error_type(error_type_) {}
	~TextureServerExcep(){}
	ErrorType error_type;
};


/*=====================================================================
TextureServer
-------------
Reads textures from disk, and returns references to them when queried by path name.

A single texture file can be identified by different pathnames varying by just pathname case.
So we use a 'canonical' path for the map key.
We do this canonicalisation if use_canonical_path_keys arg to the constructor is true.

Since the TextureServer object may be accessed concurrently from different threads - the UI thread(s) and
the core thread(s), this class needs to be thread-safe.
=====================================================================*/
class TextureServer : public ThreadSafeRefCounted
{
public:
	TextureServer(bool use_canonical_path_keys);

	virtual ~TextureServer();

	// Return a texture from disk identified by path 'pathname'.
	// Loads from disk if texture is not already loaded.
	// Throws TextureServerExcep if loading of texture fails.
	Reference<Map2D> getTexForPath(const std::string& indigo_base_dir, const std::string& pathname);

	// Return a texture from disk identified by path 'name', without converting to canonical path.
	// Loads from disk if texture is not already loaded.
	// Throws TextureServerExcep if loading of texture fails.
	Reference<Map2D> getTexForRawName(const std::string& indigo_base_dir, const std::string& name);

	// Return a texture if already loaded, if not, return null ref.
	Reference<Map2D> getTexForPathIfLoaded(const std::string& pathname);

	// Return a texture if already loaded, if not, return null ref.
	Reference<Map2D> getTexForRawNameIfLoaded(const std::string& pathname);

	// Convert pathname to canonical path, then see if texture is loaded.
	bool isTextureLoadedForPath(const std::string& pathname) const;

	// Don't try and convert name to a canonical path for a file on disk, just query directly.
	bool isTextureLoadedForRawName(const std::string& name) const;
	
	// Insert a texture that corresponds to a file on disk.
	// Pathname will be converted to canonical path for the key.
	void insertTextureForPath(const Reference<Map2D>& tex, const std::string& pathname);

	// Insert a texture that does necessarily correspond to a file on disk.
	void insertTextureForRawName(const Reference<Map2D>& tex, const std::string& name);

	// Convert pathname to canonical path, then remove that texture from textures, if present.
	void removeTextureForPath(const std::string& pathname);

	// Remove the texture from textures, if present.
	void removeTextureForRawName(const std::string& name);

	// Remove all textures that have a reference count of 1, i.e. are only pointed to by the texture server.
	void removeUnusedTextures();

	// Remove all textures with a given key (canonical pathname) that have a reference count of 1, i.e. are only pointed to by the texture server, and that are not in the set 'pathnames'.
	void removeUnusedTexturesWithRawNamesNotInSet(const std::unordered_set<std::string>& keys);

	void clear(); // Remove all textures.

	size_t numTextures() const;

	// Convert pathname to canonical path.  Throws TextureServerExcep if can't get the canonical path (for example if file does not exist).
	const std::string keyForPath(const std::string& pathname);

	// Convert pathname to canonical path.  Returns pathname if can't get the canonical path (for example if file does not exist).
	const std::string keyForPathWithIdentityFallback(const std::string& pathname);

	bool useCanonicalPaths() const { return use_canonical_path_keys; }
	void setUseCanonicalPathKeys(bool b) { use_canonical_path_keys = b; }

	size_t getTotalMemUsage() const;

	// Returns list of information in format
	// key:value
	// key:value 
	std::string getStats() const;

	void printStats();

	static void test(const std::string& indigo_base_dir);

private:
	Reference<Map2D> loadTexture(const std::string& indigo_base_dir_path, const std::string& pathname);

	NameMap<Reference<Map2D> > textures		GUARDED_BY(mutex); // Map from raw name (canonical path key) to map reference.

	mutable Mutex mutex;

	bool use_canonical_path_keys;
};
