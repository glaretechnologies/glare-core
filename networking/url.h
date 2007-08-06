#ifndef __URL_H_666_
#define __URL_H_666_


#include <string>


/*=====================================================================
URL
---
TODO: check compliance with relevant RFC.
=====================================================================*/
class URL
{
public:
	/*=====================================================================
	URL
	---
	=====================================================================*/
	URL(){}
	URL(const std::string& url);

	URL(const URL& rhs);

	/*=====================================================================
	URL
	---
	file may be absolute or relative
	=====================================================================*/
	URL(const std::string& host, const std::string& file);

	~URL();


	const URL& operator = (const URL& rhs);

	bool operator == (const URL& rhs) const;

	bool operator < (const URL& rhs) const;

	const std::string getURL() const;

	const std::string getHost() const;

	//guarenteed to return a string at least length 1, with first char = '/' or '\'
	const std::string getFile() const;

	const std::string getNoDirFilename() const;



	//these two should have a slash on the end

	const URL getDir() const;//get everthing except the file at the end

	const URL getOneDirUp() const;



	//static const URL getAbsURL(const URL& pageurl, const URL& abs_or_rel_url);

	bool hasAnyFileTypeExtension();
		
	static int getFirstSlashPos(const std::string& pageurl);//returns -1 if no slash

private:

	//static bool isRelative(const URL& abs_or_rel_url);
	static bool beginsWithSlash(const std::string s);

	std::string host;//without a / postfix
	std::string file;//without the / prefix

};



#endif //__URL_H_666_




