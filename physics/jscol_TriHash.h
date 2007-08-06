/*=====================================================================
TriHash.h
---------
File created by ClassTemplate on Wed Jul 27 18:03:06 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TRIHASH_H_666_
#define __TRIHASH_H_666_





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
	inline unsigned int indexForTriIndex(unsigned int tri_index) const;
	inline void addTriIndex(unsigned int tri_index);
	inline bool containsTriIndex(unsigned int tri_index);

private:
	static const unsigned int HASH_MASK = 0x0000000F;
	static const unsigned int TABLE_SIZE = HASH_MASK + 1;//16;

	unsigned int entries[TABLE_SIZE];
};


void TriHash::clear()
{
	for(unsigned int i=0; i<TABLE_SIZE; ++i)
		entries[i] = 0xFFFFFFFF;
}

inline unsigned int TriHash::indexForTriIndex(unsigned int tri_index) const
{
	return tri_index & HASH_MASK;
}

inline void TriHash::addTriIndex(unsigned int tri_index)
{
	entries[indexForTriIndex(tri_index)] = tri_index;
}

inline bool TriHash::containsTriIndex(unsigned int tri_index)
{
	return entries[indexForTriIndex(tri_index)] == tri_index;
}



} //end namespace js


#endif //__TRIHASH_H_666_




