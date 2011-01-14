/*=====================================================================
idmap.h
---------
File created on Fri Jan 14 13:01:07 2004
=====================================================================*/
#ifndef __IDMAP_H_666_
#define __IDMAP_H_666_

#include "platform.h"
#include "stringutils.h"
#include <map>
#include <string>


class IdMapExcep
{
public:
	IdMapExcep(const std::string& s_) : s(s_) {}
	~IdMapExcep(){}

	const std::string& what() const { return s; }
private:
	std::string s;
};



/*=====================================================================
IdMap
-------

=====================================================================*/
template <class T>
class IdMap
{
public:
	/*=====================================================================
	IdMap
	-------
	
	=====================================================================*/
	inline IdMap();

	inline ~IdMap();

	void removeAndDeleteAll();

	void insert(const uint32 key, const T& value);

	void remove(const uint32 key);

	bool isInserted(const uint32 key) const;

	unsigned int size() const { return (unsigned int)idmap.size(); }

	//throws idmapExcep if not in map
	//OLDreturns NULL if no such key inserted.
	const T& getValue(const uint32 key);

	typedef typename std::map<uint32, T>::const_iterator const_iterator;
	typedef typename std::map<uint32, T>::iterator iterator;

	inline const_iterator begin() const { return idmap.begin(); }
	inline const_iterator end() const { return idmap.end(); }
	inline iterator begin() { return idmap.begin(); }
	inline iterator end() { return idmap.end(); }

private:
	std::map<uint32, T> idmap;
};


template <class T>
IdMap<T>::IdMap()
{
}

template <class T>
IdMap<T>::~IdMap()
{
}

template <class T>
void IdMap<T>::removeAndDeleteAll()
{
	for(typename std::map<std::string, T>::iterator i = idmap.begin(); i != idmap.end(); ++i)
		delete (*i).second;

	idmap.clear();
}


template <class T>
void IdMap<T>::insert(const uint32 key, const T& value)
{
	idmap[key] = value;
}

template <class T>
void IdMap<T>::remove(const uint32 key)
{
	idmap.erase(key);
}

template <class T>
bool IdMap<T>::isInserted(const uint32 key) const
{
	return idmap.find(key) != idmap.end();
}

	//returns NULL if no such key inserted.
template <class T>
const T& IdMap<T>::getValue(const uint32 key)
{
	typename std::map<uint32, T>::iterator result = idmap.find(key);

	if(result == idmap.end())
	{
		//assert(0);
		//return (const T)NULL;
		//return (*result).second;
		throw IdMapExcep("key '" + uIntToString(key) + "' not found");
	}
	else
		return (*result).second;
}

#endif //__IDMAP_H_666_




