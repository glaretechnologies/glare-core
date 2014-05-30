#pragma once


#include "../utils/Platform.h"
#include "../maths/Vec4i.h"


const int MLG_RES = 4;


template <class NodeData>
class MultiLevelGridNode
{
public:
	MultiLevelGridNode();


	inline void setBaseChildIndex(uint32 base_child_index_) { base_child_index = base_child_index_; }
	inline uint32 getBaseChildIndex() const { return base_child_index; } // Will be zero if this is a leaf node.

	
	inline bool isCellMarked(const Vec4i& cell) const;
	inline bool isCellMarked(int cell_i) const;


	inline void markCell(const Vec4i& cell);
	inline void markCell(int cell_i);
	inline void unmarkCell(int cell_i);

	inline int cellChildIndex(const Vec4i& cell) const;


private:
	uint32 base_child_index;
	uint64 interior; // bit is set to 1 if cell i is an interior cell, 0 otherwise.
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
	//NOTE: popcnt was introduced with SSE4.2 (see http://en.wikipedia.org/wiki/SSE4)
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
bool MultiLevelGridNode<NodeData>::isCellMarked(const Vec4i& cell) const
{
	int cell_i = cell.x[2]*16 + cell.x[1]*4 + cell.x[0];
	return (interior >> cell_i) & 0x1; // Get ith bit
}


template <class NodeData>
bool MultiLevelGridNode<NodeData>::isCellMarked(int cell_i) const
{
	return (interior >> cell_i) & 0x1; // Get ith bit
}


template <class NodeData>
void MultiLevelGridNode<NodeData>::markCell(const Vec4i& cell)
{
	// Set bit i in interior to 1.
	int cell_i = cell.x[2]*16 + cell.x[1]*4 + cell.x[0];
	uint64 newbit = (uint64)1 << cell_i;
	interior |= newbit;
}


template <class NodeData>
void MultiLevelGridNode<NodeData>::markCell(int cell_i)
{
	// Set bit i in interior to 1.
	uint64 newbit = (uint64)1 << cell_i;
	interior |= newbit;
}


template <class NodeData>
void MultiLevelGridNode<NodeData>::unmarkCell(int cell_i)
{
	// Set bit i in interior to 1.
	uint64 newbit = (uint64)1 << cell_i;
	interior &= ~newbit;
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
int MultiLevelGridNode<NodeData>::cellChildIndex(const Vec4i& cell) const
{
	int cell_i = cell.x[2]*16 + cell.x[1]*4 + cell.x[0];

	// We want to count the number of bits with value 1 with index greater than i.
	uint32 num_prev_cells = (uint32)mlgPopCount((interior << (63 - cell_i)) & 0x7FFFFFFFFFFFFFFFull); // (uint32)mlgPopCount(interior >> (cell_i + 1));

	return base_child_index + num_prev_cells;
}
