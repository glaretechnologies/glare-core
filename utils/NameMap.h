/*=====================================================================
NameMap.h
---------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <unordered_map>
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
Map from string to some value type.
=====================================================================*/
template <class T>
class NameMap
{
public:
	inline NameMap();

	inline ~NameMap();

	void clear();

	void removeAndDeleteAll();

	void insert(const std::string& key, const T& value);

	void remove(const std::string& key);

	bool isInserted(const std::string& key) const;

	size_t size() const { return namemap.size(); }

	// throws NameMapExcep if not in map
	const T& getValue(const std::string& key);

	typedef typename std::unordered_map<std::string, T>::const_iterator const_iterator;
	typedef typename std::unordered_map<std::string, T>::iterator iterator;

	inline const_iterator begin() const { return namemap.begin(); }
	inline const_iterator end() const { return namemap.end(); }
	inline iterator begin() { return namemap.begin(); }
	inline iterator end() { return namemap.end(); }

	inline void erase(iterator& i);

	std::unordered_map<std::string, T>& getMap() { return namemap; }
private:
	std::unordered_map<std::string, T> namemap;
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
void NameMap<T>::clear()
{
	namemap.clear();
}

template <class T>
void NameMap<T>::removeAndDeleteAll()
{
	for(typename std::unordered_map<std::string, T>::iterator i = namemap.begin(); i != namemap.end(); ++i)
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


template <class T>
const T& NameMap<T>::getValue(const std::string& key)
{
	typename std::unordered_map<std::string, T>::iterator result = namemap.find(key);

	if(result == namemap.end())
		throw NameMapExcep("key '" + key + "' not found");
	else
		return (*result).second;
}


template <class T>
void NameMap<T>::erase(iterator& i)
{
	namemap.erase(i);
}
