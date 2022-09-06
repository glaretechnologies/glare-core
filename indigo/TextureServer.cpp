/*=====================================================================
TextureServer.cpp
-----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "TextureServer.h"


#include "globals.h"
#include "../graphics/Map2D.h"
#include "../graphics/DXTImageMap.h"
#include "../graphics/imformatdecoder.h"
#include "../utils/FileUtils.h"
#include "../utils/StringUtils.h"
#include "../utils/Lock.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"


static const bool VERBOSE = false; // Just for debugging


TextureServer::TextureServer(bool use_canonical_path_keys_)
:	use_canonical_path_keys(use_canonical_path_keys_)
{
}


TextureServer::~TextureServer()
{
}


Reference<Map2D> TextureServer::getTexForPath(const std::string& indigo_base_dir, const std::string& pathname)
{
	try
	{
		// NOTE: We use getActualOSPath() which converts to platform slashes, then calls getCanonicalPath().
		const std::string use_path = use_canonical_path_keys ? FileUtils::getActualOSPath(pathname) : pathname;

		return getTexForRawName(indigo_base_dir, use_path);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw TextureServerExcep(e.what(), TextureServerExcep::ErrorType_FileNotFound);
	}
}


Reference<Map2D> TextureServer::getTexForRawName(const std::string& indigo_base_dir, const std::string& use_path)
{
	Lock lock(mutex); // NOTE: We will still hold this mutex while the texture is being loaded if it is not yet inserted.

	if(!textures.isInserted(use_path))
	{
		if(VERBOSE) conPrint("Loading texture '" + use_path + "', was not already loaded.");
		Reference<Map2D> tex = loadTexture(indigo_base_dir, use_path);
		textures.insert(use_path, tex);
		return tex;
	}
	else
		return textures.getValue(use_path);
}


Reference<Map2D> TextureServer::getTexForPathIfLoaded(const std::string& pathname)
{
	// NOTE: We use getActualOSPath() which converts to platform slashes, then calls getCanonicalPath().
	const std::string use_path = use_canonical_path_keys ? FileUtils::getActualOSPath(pathname) : pathname;

	return getTexForRawNameIfLoaded(use_path);
}


Reference<Map2D> TextureServer::getTexForRawNameIfLoaded(const std::string& name)
{
	Lock lock(mutex);
	auto res = textures.getMap().find(name);
	if(res == textures.getMap().end())
		return Reference<Map2D>();
	else
		return res->second;
}


bool TextureServer::isTextureLoadedForPath(const std::string& pathname) const
{
	// Timer timer;
	try
	{
		const std::string use_path = use_canonical_path_keys ? FileUtils::getActualOSPath(pathname) : pathname;
		// if(timer.elapsed() > 0.1) conPrint("FileUtils::getActualOSPath took " + timer.elapsedStringNSigFigs(3) + " " + pathname);

		return isTextureLoadedForRawName(use_path);
	}
	catch(FileUtils::FileUtilsExcep& )
	{
		// if(timer.elapsed() > 0.1) conPrint("FileUtils::getActualOSPath took " + timer.elapsedStringNSigFigs(3) + " " + pathname);
		return false;
	}
}


bool TextureServer::isTextureLoadedForRawName(const std::string& name) const
{
	Lock lock(mutex);

	return textures.isInserted(name);
}


void TextureServer::insertTextureForPath(const Reference<Map2D>& tex, const std::string& pathname)
{
	try
	{
		const std::string use_path = use_canonical_path_keys ? FileUtils::getActualOSPath(pathname) : pathname;

		Lock lock(mutex);

		textures.insert(use_path, tex);
	}
	catch(FileUtils::FileUtilsExcep& )
	{
		assert(0);
	}
}


void TextureServer::insertTextureForRawName(const Reference<Map2D>& tex, const std::string& name)
{
	Lock lock(mutex);

	textures.insert(name, tex);
}


// private
Reference<Map2D> TextureServer::loadTexture(const std::string& indigo_base_dir_path, const std::string& pathname)
{
	try
	{
		return ImFormatDecoder::decodeImage(indigo_base_dir_path, pathname);
	}
	catch(ImFormatExcep& e)
	{
		//conPrint("Warning: failed to decode texture '" + pathname + "'.");
		throw TextureServerExcep(e.what(), TextureServerExcep::ErrorType_Misc);
	}
}


void TextureServer::removeTextureForPath(const std::string& pathname)
{
	try
	{
		const std::string use_path = use_canonical_path_keys ? FileUtils::getActualOSPath(pathname) : pathname;

		Lock lock(mutex);

		textures.remove(use_path);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw TextureServerExcep(e.what(), TextureServerExcep::ErrorType_Misc);
	}
}


void TextureServer::removeTextureForRawName(const std::string& name)
{
	Lock lock(mutex);

	textures.remove(name);
}


void TextureServer::removeUnusedTextures()
{
	Lock lock(mutex);

	for(NameMap<Reference<Map2D> >::iterator i = textures.begin(); i != textures.end(); )
	{
		NameMap<Reference<Map2D> >::iterator cur = i++; // We need to advance i now, because it can be invalidated when erased is called on it.
		if(cur->second->getRefCount() == 1)
		{
			// conPrint("Removing unused texture '" + cur->first + "'.");

			// Remove it
			textures.erase(cur);
		}
	}
}


void TextureServer::removeUnusedTexturesWithRawNamesNotInSet(const std::unordered_set<std::string>& pathnames)
{
	Lock lock(mutex);

	for(NameMap<Reference<Map2D> >::iterator i = textures.begin(); i != textures.end(); )
	{
		NameMap<Reference<Map2D> >::iterator cur = i++; // We need to advance i now, because it can be invalidated when erased is called on it.
		if(cur->second->getRefCount() == 1 && (pathnames.find(cur->first) == pathnames.end())) // If ref count is 1, and the texture path is not in 'pathnames':
		{
			if(VERBOSE) conPrint("Removing unused texture '" + cur->first + "'.");
			// Remove it
			textures.erase(cur);
		}
	}
}


void TextureServer::clear() // Remove all textures.
{
	Lock lock(mutex);
	textures.clear();
}


size_t TextureServer::numTextures() const
{
	Lock lock(mutex);
	return textures.size();
}


const std::string TextureServer::keyForPath(const std::string& path)
{
	try
	{
		return use_canonical_path_keys ? FileUtils::getActualOSPath(path) : path;
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw TextureServerExcep(e.what(), TextureServerExcep::ErrorType_Misc);
	}
}


const std::string TextureServer::keyForPathWithIdentityFallback(const std::string& path)
{
	try
	{
		return use_canonical_path_keys ? FileUtils::getActualOSPath(path) : path;
	}
	catch(FileUtils::FileUtilsExcep&)
	{
		return path;
	}
}


size_t TextureServer::getTotalMemUsage() const
{
	Lock lock(mutex);
	size_t sum_mem_usage = 0;
	for(NameMap<Reference<Map2D> >::const_iterator i = textures.begin(); i != textures.end(); ++i)
		sum_mem_usage += i->second->getByteSize();
	return sum_mem_usage;
}


std::string TextureServer::getStats() const
{
	size_t num_dxt_textures = 0;
	size_t dxt_mem_usage = 0;
	size_t dxt_decompressed_mem_usage = 0;
	size_t num_non_dxt_textures = 0;
	size_t non_dxt_mem_usage = 0;

	Lock lock(mutex);
	size_t sum_mem_usage = 0;
	for(NameMap<Reference<Map2D> >::const_iterator i = textures.begin(); i != textures.end(); ++i)
	{
		const Map2D* map = i->second.ptr();
		const size_t size = i->second->getByteSize();

		if(map->isDXTImageMap())
		{
			num_dxt_textures++;
			dxt_mem_usage += size;
			dxt_decompressed_mem_usage += map->getMapWidth() * map->getMapHeight() * map->numChannels();
		}
		else
		{
			num_non_dxt_textures++;
			non_dxt_mem_usage += size;
		}

		sum_mem_usage += size;
	}

	return
		"Uncompressed textures:" + toString(num_non_dxt_textures) + " (" + getNiceByteSize(non_dxt_mem_usage) + ")\n" +
		"Compressed textures:" + toString(num_dxt_textures) + " (" + getNiceByteSize(dxt_mem_usage) + ")\n" +
		((num_dxt_textures > 0) ? ("Compressed from:" + getNiceByteSize(dxt_decompressed_mem_usage) + "\n") : "") +
		"Total texture mem usage:" + getNiceByteSize(sum_mem_usage);
}


void TextureServer::printStats()
{
	conPrint("------------------TextureServer::printStats()------------------");
	Lock lock(mutex);
	size_t sum_mem_usage = 0;
	for(NameMap<Reference<Map2D> >::iterator i = textures.begin(); i != textures.end(); ++i)
	{
		const size_t W = i->second->getMapWidth();
		const size_t H = i->second->getMapHeight();
		const size_t mem = i->second->getByteSize();

		conPrint(i->first + " (" + toString(W) + "x" + toString(H) + "x" + toString(i->second->numChannels()) + ", " + ::getNiceByteSize((uint64)mem) + ")");

		sum_mem_usage += mem;
	}

	conPrint("Total mem usage: " + ::getNiceByteSize((uint64)sum_mem_usage));
	conPrint("---------------------------------------------------------------");
}


#if BUILD_TESTS


#include "../indigo/globals.h"
#include "../utils/TestUtils.h"
#include "../graphics/image.h"


void TextureServer::test(const std::string& indigo_base_dir)
{
	conPrint("TextureServer::test()");

	// Try loading a single texture
	try
	{
		TextureServer server(/*use_canonical_path_keys=*/true);
		const std::string path = TestUtils::getTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg";
		const std::string key = server.keyForPath(path);
		Reference<Map2D> map = server.getTexForPath(indigo_base_dir, path);
		testAssert(map.nonNull());
		testAssert(server.numTextures() == 1); // Check there is one texture loaded.
		testAssert(server.isTextureLoadedForPath(path));
		testAssert(server.isTextureLoadedForRawName(key));
		testAssert(!server.isTextureLoadedForPath("BLEH"));
		testAssert(!server.isTextureLoadedForRawName("BLEH"));

		Reference<Map2D> map2 = server.getTexForPath(indigo_base_dir, path); // Try getting it again.

		testAssert(map.getPointer() == map2.getPointer());
		testAssert(server.numTextures() == 1); // Check there is one texture loaded.
	}
	catch(TextureServerExcep& e)
	{
		failTest(e.what());
	}

	// Try loading a single texture with backslashes in the path name.  Should work on Linux, OS X and Windows as slashes will be converted.
	try
	{
		TextureServer server(/*use_canonical_path_keys=*/true);
		const std::string path = TestUtils::getTestReposDir() + "\\testfiles\\italy_bolsena_flag_flowers_stairs_01.jpg";
		Reference<Map2D> map = server.getTexForPath(indigo_base_dir, path);
		testAssert(map.nonNull());
		testAssert(server.numTextures() == 1); // Check there is one texture loaded.
		testAssert(server.isTextureLoadedForPath(path));
	}
	catch(TextureServerExcep& e)
	{
		failTest(e.what());
	}

	// Try loading the same texture with a case variation in names.  Should only load it once.
#if defined(_WIN32) || defined(OSX) // If we are on a case-insensitive filesystem:
	try
	{
		TextureServer server(/*use_canonical_path_keys=*/true);

		const std::string path1 = TestUtils::getTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg";
		Reference<Map2D> map1 = server.getTexForPath(indigo_base_dir, path1);
		testAssert(map1.nonNull());

		testAssert(server.numTextures() == 1); // Check there is one texture loaded.

		const std::string path2 = TestUtils::getTestReposDir() + "/testfiles/ITALY_bolsena_flag_flowers_stairs_01.jpg"; // Note the uppercase ITALY.
		Reference<Map2D> map2 = server.getTexForPath(indigo_base_dir, path2);
		testAssert(map2.nonNull());

		testAssert(server.numTextures() == 1); // Check there is one texture loaded.

		testAssert(server.isTextureLoadedForPath(path1)); // Both variations on path name should be alright.
		testAssert(server.isTextureLoadedForPath(path2));
		testAssert(!server.isTextureLoadedForPath("BLEH"));
	}
	catch(TextureServerExcep& e)
	{
		failTest(e.what());
	}
#endif


	// Try loading then removing a single texture
	try
	{
		TextureServer server(/*use_canonical_path_keys=*/true);
		const std::string path = TestUtils::getTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg";
		Reference<Map2D> map = server.getTexForPath(indigo_base_dir, path);
		testAssert(map.nonNull());
		testAssert(server.numTextures() == 1); // Check there is one texture loaded.
		testAssert(server.isTextureLoadedForPath(path));

		server.removeTextureForPath(path);
		testAssert(server.numTextures() == 0); // Check there is one texture loaded.
		testAssert(!server.isTextureLoadedForPath(path));
	}
	catch(TextureServerExcep& e)
	{
		failTest(e.what());
	}

	// Try inserting a single texture that does not correspond to a path on disk:
	try
	{
		TextureServer server(/*use_canonical_path_keys=*/true);
		Reference<Map2D> map = new Image(16, 16);

		const std::string name = "TEXNAME";

		testAssert(!server.isTextureLoadedForRawName(name));
		testAssert(server.numTextures() == 0);

		server.insertTextureForRawName(map, name);

		testAssert(map.nonNull());
		testAssert(server.numTextures() == 1); // Check there is one texture loaded.
		testAssert(server.isTextureLoadedForRawName(name));

	}
	catch(TextureServerExcep& e)
	{
		failTest(e.what());
	}

	//========================== Test removeUnusedTextures() ==================================
	try
	{
		TextureServer server(/*use_canonical_path_keys=*/true);
		
		Reference<Map2D> a = new Image(16, 16);
		server.insertTextureForRawName(a, "a");

		Reference<Map2D> b = new Image(16, 16);
		server.insertTextureForRawName(b, "b");

		testAssert(server.numTextures() == 2);

		// Both textures should have ref count 2 currently.
		testAssert(a->getRefCount() == 2 && b->getRefCount() == 2);
		server.removeUnusedTextures();
		testAssert(server.numTextures() == 2);

		// Zero the 'a' reference.
		a = Reference<Map2D>(NULL);
		server.removeUnusedTextures();

		testAssert(server.numTextures() == 1);
		testAssert(!server.isTextureLoadedForRawName("a")); // 'a' should have been removed
		testAssert(server.isTextureLoadedForRawName("b")); 

		// Zero the 'b' reference.
		b = Reference<Map2D>(NULL);
		server.removeUnusedTextures();

		testAssert(server.numTextures() == 0);
	}
	catch(TextureServerExcep& e)
	{
		failTest(e.what());
	}

	//========================== Test removeUnusedTexturesWithPathKeysNotInSet() ==================================
#if defined(_WIN32) || defined(OSX) // If we are on a case-insensitive filesystem:
	try
	{
		TextureServer server(/*use_canonical_path_keys=*/true);
		
		Reference<Map2D> a = server.getTexForPath(indigo_base_dir, TestUtils::getTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg");

		// Load same path but with different case.  'a' and 'b' should refer to same tex.
		Reference<Map2D> b = server.getTexForPath(indigo_base_dir, TestUtils::getTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.JPG");


		Reference<Map2D> c = server.getTexForPath(indigo_base_dir, TestUtils::getTestReposDir() + "/testfiles/textures/checker.jpg");

		testAssert(server.numTextures() == 2);
		testAssert(a->getRefCount() == 3 && b->getRefCount() == 3 && c->getRefCount() == 2);

		// Set of textures not to remove
		std::unordered_set<std::string> keys;
		keys.insert(server.keyForPath(TestUtils::getTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg"));

		server.removeUnusedTexturesWithRawNamesNotInSet(keys);

		// Since no ref counts were 1, nothing should have been removed.
		testAssert(server.numTextures() == 2);

		// Zero all refs
		a = NULL;
		b = NULL;
		c = NULL;

		testAssert(server.numTextures() == 2);
		server.removeUnusedTexturesWithRawNamesNotInSet(keys); // Should remove checker texture.
		testAssert(server.numTextures() == 1);
		testAssert(server.isTextureLoadedForPath(TestUtils::getTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg"));

	}
	catch(TextureServerExcep& e)
	{
		failTest(e.what());
	}
#endif
}


#endif
