#pragma once


#include "../utils/platform.h"
#include "../maths/vec3.h"


const int MLG_RES = 4;


template <class NodeData>
class MultiLevelGridNode
{
public:
	MultiLevelGridNode();


	inline void setBaseChildIndex(uint32 base_child_index_) { base_child_index = base_child_index_; }
	inline uint32 getBaseChildIndex() const { return base_child_index; }

	/*
		0 = empty leaf
		1 = interior
		2 = leaf containing >= 1 object.
	*/
	inline bool cellIsInterior(const Vec3<int>& cell) const;
	//bool cellIsEmpty(int i, int j, int k);
	//bool cellIsNonEmptyLeaf(int i, int j, int k);

	//unsigned int cellIsInteriorType() const { return 0; }
	//unsigned int type;
	//unsigned int child_index;

	inline void markCellAsInterior(const Vec3<int>& cell);

	inline int cellChildIndex(const Vec3<int>& cell) const;

	/*inline void setCellChildIndex(int i, int j, int k, int index)
	{
		cells[i*16 + j*4 + k] = index;
	}*/
private:
	uint32 base_child_index;
	uint64 interior; // bit is set to 1 if cell i is an interior cell, 0 otherwise.
	//int cells[MLG_RES * MLG_RES * MLG_RES];
public:
	NodeData data;
};


template <class NodeData>
MultiLevelGridNode<NodeData>::MultiLevelGridNode()
{
	base_child_index = 0;
	interior = 0;
}


#if defined _WIN32
#include <intrin.h> // TEMP for __popcnt
#endif


inline uint64 mlgPopCount(uint64 x)
{
#if defined _WIN64
	return __popcnt64(x);
#elif defined _WIN32
	return __popcnt(x & 0xFFFFFFFFu) + __popcnt(x >> 32);
#else
	/*uint64 c = 0;
	for(int i=0; i<64; ++i)
	{
		uint64 bitval = (x >> i) & 0x1;
		c += bitval;
	}
	return c;*/
	// See http://gcc.gnu.org/onlinedocs/gcc-3.4.3/gcc/Other-Builtins.html
	//NOTE: check this works in 32 bit mode.
	return __builtin_popcountll(x);
#endif
}


template <class NodeData>
bool MultiLevelGridNode<NodeData>::cellIsInterior(const Vec3<int>& cell) const
{
	int cell_i = cell.z*16 + cell.y*4 + cell.x;
	return (interior >> cell_i) & 0x1; // Get ith bit
}


template <class NodeData>
void MultiLevelGridNode<NodeData>::markCellAsInterior(const Vec3<int>& cell)
{
	// Set bit i in interior to 1.
	int cell_i = cell.z*16 + cell.y*4 + cell.x;
	uint64 newbit = (uint64)1 << cell_i;
	interior |= newbit;
}


/*
To get number of bits set to right of index 6:

7	6	5	4	3	2	1	0

shift left by 1 place:  (by 7 - 6 places)

6	5	4	3	2	1	0	_

then mask off the leftmost bit:

_	5	4	3	2	1	0	_
*/

template <class NodeData>
int MultiLevelGridNode<NodeData>::cellChildIndex(const Vec3<int>& cell) const
{
	int cell_i = cell.z*16 + cell.y*4 + cell.x;

	// We want to count the number of bits with value 1 with index greater than i.
	uint32 num_prev_cells = (uint32)mlgPopCount((interior << (63 - cell_i)) & 0x7FFFFFFFFFFFFFFFull); // (uint32)mlgPopCount(interior >> (cell_i + 1));

	return base_child_index + num_prev_cells;
}
