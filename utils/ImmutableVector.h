/*=====================================================================
ImmutableVector.h
-------------------
Copyright Glare Technologies Limited 2014 -
Generated at 2014-03-29 12:47:28 +0000
=====================================================================*/
#pragma once


#include "../maths/mathstypes.h"
#include "ThreadSafeRefCounted.h"
#include "Reference.h"
#include <assert.h>
#include <vector>


/*=====================================================================
ImmutableVector
-------------------
B is the branching factor / block size.
B must be a power of 2 and >= 4.
=====================================================================*/


template <class T, size_t B>
class ImmutableVectorNode : public ThreadSafeRefCounted
{
public:
	inline ImmutableVectorNode() : interior(false), num(0) {}

	inline bool isInterior() const { return interior; }
	inline ImmutableVectorNode<T, B>* getLastChild() { return children[num-1].getPointer(); }


	inline void operator = (const ImmutableVectorNode& other);

	
	inline void moveItemsFrom(ImmutableVectorNode<T, B>* const src);

	inline void moveChildrenFrom(ImmutableVectorNode<T, B>* const src);

	bool interior;
	T items[B];
	int num; // num items or children.

	Reference<ImmutableVectorNode<T, B> > children[B];
};


template <class T, size_t B>
class ImmutableVector : public ThreadSafeRefCounted
{
public:
	inline ImmutableVector();
	inline ~ImmutableVector();

	inline void operator = (const ImmutableVector& other);


	// Return a copy of the ImmutableVector, with the item at the given index updated.
	// Does not change the original.
	inline void update(size_t index, const T& item, ImmutableVector& copy_out) const;


	inline const T& operator [] (size_t index) const;

	// Return a copy of the ImmutableVector, with the item inserted at the end of the vector.
	// Does not change the original.
	inline void push_back(const T& item, ImmutableVector& copy_out) const;


	inline size_t size() const { return total_num_items; }

	// This version mutates the vector in-place.
	inline void pushBackMutateInPlace(const T& item);

private:
	inline void rebalanceIfNeeded(ImmutableVectorNode<T, B>** stack, ImmutableVectorNode<T, B>* n);
	inline int log2B() const { return log2B_; }
	ImmutableVectorNode<T, B> root;
	size_t total_num_items;
	int depth;
	int log2B_;
};


void testImmutableVector();


template <class T, size_t B>
void ImmutableVectorNode<T, B>::operator = (const ImmutableVectorNode& other)
{
	interior = other.interior;
	num = other.num;
	for(int i=0; i<B; ++i) // NOTE: just go to num here?
		items[i] = other.items[i];
	for(int i=0; i<B; ++i)
		children[i] = other.children[i];
}


template <class T, size_t B>
void ImmutableVectorNode<T, B>::moveItemsFrom(ImmutableVectorNode<T, B>* const src)
{
	const int N = src->num;

	for(int i=0; i<N; ++i)
		items[i] = src->items[i];
	this->num = N;

	// Clear src items
	for(int i=0; i<N; ++i)
		src->items[i] = T();
	src->num = 0;
}


template <class T, size_t B>
void ImmutableVectorNode<T, B>::moveChildrenFrom(ImmutableVectorNode<T, B>* const src)
{
	const int N = src->num;

	for(int i=0; i<N; ++i)
		children[i] = src->children[i];
	this->num = N;

	for(int i=0; i<N; ++i)
		src->children[i] = Reference<ImmutableVectorNode<T, B> >();
	src->num = 0;
}


inline int ImmutableVectorintLog2(int x)
{
	int y = 0;
	while (x >>= 1) y++;
	return y;
}


template <class T, size_t B>
ImmutableVector<T, B>::ImmutableVector()
:	total_num_items(0),
	depth(1),
	log2B_(ImmutableVectorintLog2(B))
{
	assert(B >= 4);
	assert(Maths::isPowerOfTwo(B));
}


template <class T, size_t B>
ImmutableVector<T, B>::~ImmutableVector()
{}


template <class T, size_t B>
void ImmutableVector<T, B>::operator = (const ImmutableVector& other)
{
	root = other.root;
	total_num_items = other.total_num_items;
	depth = other.depth;
	log2B_ = other.log2B_;
}


/*inline int intPow(int x, int y)
{
	int res = x;
	for(int z=1; z<y; ++z)
		res *= x;
	return res;
}*/


template <class T, size_t B>
void ImmutableVector<T, B>::update(size_t index, const T& item, ImmutableVector& copy_out) const
{
	size_t offset = index; // Offset from current node left item index.

	// Make a copy of the root as we know one of its child pointers, or items will change.
	copy_out.root = root;
	copy_out.total_num_items = total_num_items;
	copy_out.depth = depth;

	ImmutableVectorNode<T, B>* n = &copy_out.root; // Pointer to current node.  This will always be a copied node.
	int layer = 0;
	while(1)
	{
		if(!n->isInterior())
		{
			// Leaf node.  This should already be a copy of the original leaf node.

			assert(offset < B);
			assert(offset < n->num);

			n->items[offset] = item;
			break;
		}
		else
		{
			// Interior node
			
			size_t child_i = offset >> ((depth - layer - 1) * log2B());

			offset -= child_i << ((depth - layer - 1) * log2B());

			// Create copy of child node.
			Reference<ImmutableVectorNode<T, B> > new_child = new ImmutableVectorNode<T, B>();
			*new_child = *n->children[child_i];

			n->children[child_i] = new_child; // Update pointer in n to point to our new copy.

			// Traverse to the child we picked
			n = new_child.getPointer();
		}

		layer++;
	}
}


template <class T, size_t B>
void ImmutableVector<T, B>::pushBackMutateInPlace(const T& item)
{
	// Traverse tree down to last node, building a stack of interior nodes that we traversed.
	ImmutableVectorNode<T, B>* stack[32];
	int num_stack_items = 0;

	ImmutableVectorNode<T, B>* n = &root;
	while(n->isInterior())
	{
		assert(num_stack_items < 32);
		stack[num_stack_items++] = n;
		n = n->getLastChild();
	}
	
	// Append item to node
	n->items[n->num++] = item;

	total_num_items++;

	rebalanceIfNeeded(stack, n);
}


template <class T, size_t B>
void ImmutableVector<T, B>::push_back(const T& item, ImmutableVector<T, B>& copy_out) const
{
	// Traverse tree down to last node, building a stack of interior nodes that we traversed.
	ImmutableVectorNode<T, B>* stack[32];
	int num_stack_items = 0;


	// Make a copy of the root as we know one of its child pointers, or items will change.
	copy_out.root = root;
	copy_out.total_num_items = total_num_items;
	copy_out.depth = depth;

	// Get leaf that we will be adding the item to.
	// Make copies of all ancestors of it as we go.
	ImmutableVectorNode<T, B>* n = &copy_out.root; // This will always point to a copied node.
	while(n->isInterior())
	{
		// Create copy of child node.
		Reference<ImmutableVectorNode<T, B> > new_child = new ImmutableVectorNode<T, B>();
		*new_child = *n->children[n->num - 1];

		n->children[n->num - 1] = new_child; // Update pointer in n to point to our new copy.

		assert(num_stack_items < 32);
		stack[num_stack_items++] = n;
		n = new_child.getPointer();
	}
	
	// Append item to node
	n->items[n->num++] = item;

	copy_out.total_num_items++;

	copy_out.rebalanceIfNeeded(stack, n);
}


template <class T, size_t B>
void ImmutableVector<T, B>::rebalanceIfNeeded(ImmutableVectorNode<T, B>** stack, ImmutableVectorNode<T, B>* n)
{
	const int old_depth = depth;
	
	// If node is now full, add a new node
	if(n->num == B)
	{
		// If we have totally filled up to the current depth:
		if(total_num_items == (size_t)1 << (log2B() * depth)) // intPow(B, depth))
		{
			if(total_num_items == B) // Special case rebalance for when root node is full.
			{
				Reference<ImmutableVectorNode<T, B> > new_child = new ImmutableVectorNode<T, B>();
				new_child->moveItemsFrom(&root);

				root.num = 2;
				root.interior = true;
				root.children[0] = new_child;
				root.children[1] = new ImmutableVectorNode<T, B>();
			}
			else
			{
				assert(root.interior);
				// Make new node n
				Reference<ImmutableVectorNode<T, B> > new_child = new ImmutableVectorNode<T, B>();
				new_child->moveChildrenFrom(&root);
				new_child->interior = true;
				root.num = 1;
				root.interior = true;
				root.children[0] = new_child;

				// Create nodes m_1, m_2, m_3 ... m_{old_depth}
				ImmutableVectorNode<T, B>* parent = &root;
				for(int i = 1; i <= old_depth-1; ++i)
				{
					Reference<ImmutableVectorNode<T, B> >  m_i = new ImmutableVectorNode<T, B>();
					m_i->interior = true;
					parent->children[parent->num++] = m_i; // Append child

					parent = m_i.getPointer();
				}

				// Create last leaf node
				Reference<ImmutableVectorNode<T, B> >  m_old_depth = new ImmutableVectorNode<T, B>();
				parent->children[parent->num++] = m_old_depth; // Append child

			}
			depth++;

		}
		else
		{
			// Walk up stack until we get to a node that has < B children.
			for(int z=depth-2; z>=0; --z)
			{
				// If we have found an interior node that is not full, at level z:
				if(stack[z]->num < B)
				{
					// Walk back down to bottom level, creating new nodes as necessary

					// Create interior nodes m_{z+1}, m_{z+2}, ... m_{depth-2}
					ImmutableVectorNode<T, B>* parent = stack[z];
					for(int i = z + 1; i <= depth-2; ++i)
					{
						Reference<ImmutableVectorNode<T, B> > m_i = new ImmutableVectorNode<T, B>();
						m_i->interior = true;
						parent->children[parent->num++] = m_i; // Append child pointer to parent

						parent = m_i.getPointer();
					}

					// Create leaf node m_{depth-1}
					Reference<ImmutableVectorNode<T, B> > m_depth_1 = new ImmutableVectorNode<T, B>();
					parent->children[parent->num++] = m_depth_1; // Append child pointer to parent
					return;
				}
			}
		}
	}
}


template <class T, size_t B>
const T& ImmutableVector<T, B>::operator [] (size_t index) const
{
	assert(index < total_num_items);

	size_t offset = index; // Offset from current node left item index.

	const ImmutableVectorNode<T, B>* n = &root;
	int layer = 0;
	while(n->isInterior())
	{
		// int num_items_per_child_subtree = intPow(B, depth - layer - 1);
		// int child_i = (int)offset / num_items_per_child_subtree;
		// offset -= child_i * num_items_per_child_subtree;

		size_t child_i = offset >> ((depth - layer - 1) * log2B());

		offset -= child_i << ((depth - layer - 1) * log2B());

		// Traverse to the child we picked
		n = n->children[child_i].getPointer();
		layer++;
	}

	// Leaf node
	assert(offset < B);
	assert(offset < n->num);
	return n->items[offset];
}
