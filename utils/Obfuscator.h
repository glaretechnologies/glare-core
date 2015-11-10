/*=====================================================================
Obfuscator.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Tue May 25 18:32:39 +1200 2010
=====================================================================*/
#pragma once


#include <string>
#include <set>


/*=====================================================================
Obfuscator
-------------------

=====================================================================*/
class Obfuscator
{
public:
	enum Lang
	{
		Lang_OpenCL,
		Lang_Winter
	};

	Obfuscator(bool collapse_whitespace, bool remove_comments, bool change_tokens,/* bool cryptic_tokens, */Lang lang);
	~Obfuscator();

	static const std::string tokenHashString(const std::string& t);
	const std::string mapToken(const std::string& t);

	const std::string obfuscate(const std::string& s);

	static void obfuscateKernels(const std::string& kernel_dir);

	const std::string obfuscateWinterSource(const std::string& src);
	const std::string obfuscateWinterSourceIfObfuscating(const std::string& src) { return useObfuscatedSource() ? obfuscateWinterSource(src) : src; }

	static const std::string hashIfObfuscating(const std::string& t) { return useObfuscatedSource() ? tokenHashString(t) : t; }

	static bool useObfuscatedSource() { return true; }

	static const std::string encryptedFilename(const std::string& plaintext_filename);

	// Read a source file from disk, then decrypt it.
	static const std::string readAndDecryptFile(const std::string& path);

	// Read a source file from disk.  If obfuscation & encryption is enabled, will decrypt the file contents. 
	// Throws Indigo::Exception on failure.
	static const std::string readFileFromDisk(const std::string& indigo_base_dir, const std::string& plain_text_path);

	// Change their obfuscated names so that they will be the same as the obfuscated names generated from winter code.
	static const std::string renameOpenCLSupportFunctions(const std::string& support_opencl_code);


	static void test();
private:
	void addOpenCLKeywords();
	void addWinterKeywords();

	bool collapse_whitespace;
	bool remove_comments;
	bool change_tokens;
	//bool cryptic_tokens;

	std::set<std::string> keywords;

	Lang lang;
};
