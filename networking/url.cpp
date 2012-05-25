#include "url.h"


#include "../utils/stringutils.h"
#include <assert.h>


URL::URL(const std::string& url_)
{
	std::string url = url_;

	//-----------------------------------------------------------------
	//strip off any leading http:// (shouldn't strictly be in url anyway?)
	//-----------------------------------------------------------------
	if(url.size() >= 7 && url.substr(0, 7) == "http://")
		url = url.substr(7, url.length() - 7);

	//-----------------------------------------------------------------
	//find first / or \
	//-----------------------------------------------------------------
	const int first_slashpos = getFirstSlashPos(url);

	if(first_slashpos == -1)
	{
		host = url;
		file = "/";
	
	}
	else
	{

		//-----------------------------------------------------------------
		//split up into host and file at host
		//-----------------------------------------------------------------
		host = url.substr(0, first_slashpos);
		
		file = url.substr(first_slashpos, url.length() - first_slashpos);
	}

}

URL::URL(const std::string& host_, const std::string& file_)
:	host(host_), file(file_)
{
	//if(file.length() >= 0)
	//	if(file[file.length() - 1] == '/' || file[file.length() - 1] == '\\')
	//		file = file.substr(0, file.length() - 1);//lop off last slash

	//-----------------------------------------------------------------
	//add a slash to the start of file if it does not already have one
	//-----------------------------------------------------------------
	if(!(hasPrefix(file, "\\") || hasPrefix(file, "/")))
		file = "/" + file;
}

URL::URL(const URL& rhs)
:	host(rhs.host), file(rhs.file)
{

}

URL::~URL()
{
	
}

const URL& URL::operator = (const URL& rhs)
{
	host = rhs.host;
	file = rhs.file;

	return *this;
}

bool URL::operator == (const URL& rhs) const
{
	return host == rhs.host && file == rhs.file;
}

bool URL::operator < (const URL& rhs) const
{
	return getURL() < rhs.getURL();
}


const std::string URL::getURL() const
{
	//if(file.length() == 0)
	//	return host;

	/*if(file[0] == '\\' || file[0] == '/')
		return host + file;
	else
		return host + '/' + file;*/
	return host + file;
}

const std::string URL::getHost() const
{
	return host;
}
const std::string URL::getFile() const
{
	assert(file.length() >= 1);
	assert(file[0] == '/' || file[0] == '\\');

	return file;
}



const std::string URL::getNoDirFilename() const
{
	for(int i=(int)file.length() - 1; i>= 0; i--)
	{
		if(file[i] == '/' || file[i] == '\\')
		{
			return file.substr(i + 1, file.length() - (i - 1));
		}
	}

	//-----------------------------------------------------------------
	//no slash, just return whole file
	//-----------------------------------------------------------------
	return file;
}
		

const URL URL::getDir() const//get everthing except the file at the end
{
	for(int i=(int)file.length() - 1; i>= 0; i--)
	{
		if(file[i] == '/' || file[i] == '\\')
		{
			assert(i+1 <= (int)file.length());

			return URL(host, file.substr(0, i+1));
		}
	}

	//-----------------------------------------------------------------
	//no slash, just return whole url
	//-----------------------------------------------------------------
	return *this;
}

const URL URL::getOneDirUp() const
{
	//-----------------------------------------------------------------
	//get the current directory
	//-----------------------------------------------------------------
	URL curdir = getDir(); 

	if(curdir.getFile().length() == 1)
	{
		//already at root dir.. so can't go up one.
		return curdir;//just return current dir.
		//NOTE: might be better to signal error here
	}

	//-----------------------------------------------------------------
	//strip the rightmost slash off the current dir URL
	//-----------------------------------------------------------------
	curdir = URL(curdir.getURL().substr(0, curdir.getURL().length() - 1));

	//-----------------------------------------------------------------
	//find the rightmost slash
	//-----------------------------------------------------------------
	for(int i=(int)curdir.getFile().length() - 1; i>= 0; i--)
	{
		if(curdir.getFile()[i] == '/' || curdir.getFile()[i] == '\\')
		{
			assert(i+1 <= (int)curdir.getFile().length());

			return URL(curdir.getHost(), curdir.getFile().substr(0, i+1));
		}
	}

	assert(0);
	return getDir();


}


/*const URL URL::getAbsURL(const URL& pageurl, const URL& abs_or_rel_url)
{
	if(isRelative(abs_or_rel_url))
	{
		if(beginsWithSlash(abs_or_rel_url.getURL()))
		{
			return URL(pageurl.getDir() + abs_or_rel_url.getURL());
		}
		else
		{
			return URL(pageurl.getDir() + '/' + abs_or_rel_url.getURL());
		}
	}

	//-----------------------------------------------------------------
	//else is absolute
	//-----------------------------------------------------------------
	return abs_or_rel_url;
}*/


/*bool URL::isRelative(const URL& abs_or_rel_url)
{
	const int first_slashpos = getFirstSlashPos(abs_or_rel_url.getURL());

	if(first_slashpos == -1)
	{
		//no slash, must be relative
		return true;
	}

	const std::string hostordir = abs_or_rel_url.getURL().substr(0, first_slashpos-1);

	for(int i=0; i<hostordir.length(); i++)
	{
		if(hostordir[i] == '.')
		{
			//must be a host name if it has a dot in it
			return false;
		}
	}

	//-----------------------------------------------------------------
	//no dot in left bit == not hostname == relative URL
	//-----------------------------------------------------------------
	return true;

}*/

int URL::getFirstSlashPos(const std::string& urlstring)//returns -1 if no slash
{

	if(urlstring.length() == 0)
		return -1;


	int first_slashpos = (int)urlstring.length();

	for(int i=(int)urlstring.length() - 1; i>= 0; i--)
	{
		if(urlstring[i] == '/' || urlstring[i] == '\\')
		{
			first_slashpos = i;
		}
	}

	if(first_slashpos == urlstring.length())
	{
		//no slash found
		return -1;
	}
	else
		return first_slashpos;
}


bool URL::beginsWithSlash(const std::string s)
{
	if(s.length() == 0)
		return false;

	if(s[0] == '\\' || s[0] == '/')
		return true;
	else
		return false;
}




bool URL::hasAnyFileTypeExtension()
{
	//has a file extension iff there is a dot in the file bit

	if(getFile().find_first_of(".") == std::string::npos)
		return false;
	else
		return true;
}