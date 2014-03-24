/*=====================================================================
NameMap.h
---------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Sun Nov 07 02:10:07 2004
=====================================================================*/
#pragma once


#include "Platform.h"
#include <map>
#include <string>


class NameMapExcep
{
public:
	NameMapExcep(const std::string& s_) : s(s_) {}
	~NameMapExcep(){}

	const std::string& what() const { return s; }
private:
	std::string s;
};



/*=====================================================================
NameMap
-------

=====================================================================*/
template <class T>
class NameMap
{
public:
	/*=====================================================================
	NameMap
	-------
	
	=====================================================================*/
	inline NameMap();

	inline ~NameMap();

	void removeAndDeleteAll();

	void insert(const std::string& key, const T& value);

	void remove(const std::string& key);

	bool isInserted(const std::string& key) const;

	unsigned int size() const { return (unsigned int)namemap.size(); }

	//throws NameMapExcep if not in map
	//OLDreturns NULL if no such key inserted.
	const T& getValue(const std::string& key);

	typedef typename std::map<std::string, T>::const_iterator const_iterator;
	typedef typename std::map<std::string, T>::iterator iterator;

	inline const_iterator begin() const { return namemap.begin(); }
	inline const_iterator end() const { return namemap.end(); }
	inline iterator begin() { return namemap.begin(); }
	inline iterator end() { return namemap.end(); }

	inline void erase(iterator& i);

private:
	std::map<std::string, T> namemap;
};


template <class T>
NameMap<T>::NameMap()
{
}

template <class T>
NameMap<T>::~NameMap()
{
}

template <class T>
void NameMap<T>::removeAndDeleteAll()
{
	for(typename std::map<std::string, T>::iterator i = namemap.begin(); i != namemap.end(); ++i)
		delete (*i).second;

	namemap.clear();
}


template <class T>
void NameMap<T>::insert(const std::string& key, const T& value)
{
	namemap[key] = value;
}

template <class T>
void NameMap<T>::remove(const std::string& key)
{
	namemap.erase(key);
}

template <class T>
bool NameMap<T>::isInserted(const std::string& key) const
{
	return namemap.find(key) != namemap.end();
}

	//returns NULL if no such key inserted.
template <class T>
const T& NameMap<T>::getValue(const std::string& key)
{
	typename std::map<std::string, T>::iterator result = namemap.find(key);

	if(result == namemap.end())
	{
		//assert(0);
		//return (const T)NULL;
		//return (*result).second;
		throw NameMapExcep("key '" + key + "' not found");
	}
	else
		return (*result).second;
}


template <class T>
void NameMap<T>::erase(iterator& i)
{
	namemap.erase(i);
}
