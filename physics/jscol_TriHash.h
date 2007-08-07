/*=====================================================================
TriHash.h
---------
File created by ClassTemplate on Wed Jul 27 18:03:06 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TRIHASH_H_666_
#define __TRIHASH_H_666_


#include "../utils/Platform.h"



namespace js
{




/*=====================================================================
TriHash
-------

=====================================================================*/
class TriHash
{
public:
	/*=====================================================================
	TriHash
	-------
	
	=====================================================================*/
	TriHash();

	~TriHash();

	inline void clear();
	inline uint32 indexForTriIndex(uint32 tri_index) const;
	inline void addTriIndex(uint32 tri_index);
	inline bool containsTriIndex(uint32 tri_index);

private:
	static const uint32 HASH_MASK = 0x0000000F;
	static const uint32 TABLE_SIZE = HASH_MASK + 1;//16;

	uint32 entries[TABLE_SIZE];
};


void TriHash::clear()
{
	for(uint32 i=0; i<TABLE_SIZE; ++i)
		entries[i] = 0xFFFFFFFF;
}

inline uint32 TriHash::indexForTriIndex(uint32 tri_index) const
{
	return tri_index & HASH_MASK;
}

inline void TriHash::addTriIndex(uint32 tri_index)
{
	entries[indexForTriIndex(tri_index)] = tri_index;
}

inline bool TriHash::containsTriIndex(uint32 tri_index)
{
	return entries[indexForTriIndex(tri_index)] == tri_index;
}



} //end namespace js


#endif //__TRIHASH_H_666_




