/*===================================================================

  
  digital liberation front 2001
  
  _______    ______      _______
 /______/\  |______|    /\______\  
|       \ \ |      |   / /       |    
|	      \| |      |  |/         |  
|_____    \ |      |_ /    ______|       
 ____|    | |      |_||    |_____          
     |____| |________||____|                
           



Code by Nicholas Chapman
nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may not use this code for any commercial project.
====================================================================*/
/*====================================================================================
hash table for pointers
-----------------------
coded by Nicholas Chapman in the year 2000
nickamy@paradise.net.nz
onosendai@botepidemic.com
====================================================================================*/
#ifndef __LINKBUCKETHASH_H__
#define __LINKBUCKETHASH_H__

#include <list>
#include <assert.h>


/*====================================================================================
PointerHash<class T>
--------------------

*	template parameter T:
	the type of the object you want to store pointers to, NOT the type of the pointer to 
	the object.

  //NOTE: could use linked list for iteration.
  
====================================================================================*/
//template <class T> class LinkBucketHashIterator;//forwards declaration

template<class T>
class LinkBucketHash
{
	//friend class PointerHashIterator<T>;
public:
	/*====================================================================================
	LinkBucketHash
	--------------------------------------------------------------------------------------
	====================================================================================*/
	LinkBucketHash(int num_buckets);
	~LinkBucketHash();

	
	/*====================================================================================
	insert
	------
	====================================================================================*/
	inline void insert(T* p);

	/*====================================================================================
	remove
	--------------------------------------------------------------------------------------
	====================================================================================*/
	inline void remove(const T* p);

	/*====================================================================================
	removeAll
	--------------------------------------------------------------------------------------
	removes all pointers from the hash table.
	====================================================================================*/
	void removeAll();


	/*====================================================================================
	doesContain
	-----------
	returns true if p is in the hash table, false if not.
	====================================================================================*/
	bool doesContain(const T* p);

	/*====================================================================================
	iterator
	--------------------------------------------------------------------------------------
	use LinkBucketHash::iterator to iterate through the hash table
	====================================================================================*/
//	typedef LinkBucketHash<T> iterator;
	//for iterator//

	/*====================================================================================
	begin
	--------------------------------------------------------------------------------------
	returns an iterator pointing to the first pointer in the hash table
	====================================================================================*/
//	inline iterator begin();
	/*====================================================================================
	end
	--------------------------------------------------------------------------------------
	returns an iterator one past the end of the hash table.
	Don't try to dereference this iterator!
	====================================================================================*/
//	inline const iterator& end() const { return end_iterator; }

	/*====================================================================================
	getNumBuckets
	--------------------------------------------------------------------------------------
	returns the actual number of buckets used.
	====================================================================================*/
	inline int getNumBuckets(){ return num_buckets; }

	/*====================================================================================
	numPointers
	--------------------------------------------------------------------------------------
	returns the number of pointers currently in the hash table
	O(n)!
	====================================================================================*/
	int numPointers(); 

	/*====================================================================================
	hashLoad
	--------------------------------------------------------------------------------------
	num_pointers / num_buckets
	O(n)!
	====================================================================================*/
	float hashLoad();


private:
	inline int getBucketForHash(int hash) const;

//	iterator end_iterator;

	Bucket* bucket;//pointer to an array of buckets
	int num_buckets;
};





/*====================================================================================
LinkBucketHashIterator<class T>
--------------------------------------------------------------------------------------

====================================================================================*/
/*template <class T>
class LinkBucketHashIterator
{
public:
	inline PointerHashIterator();
	inline LinkBucketHashIterator(const LinkBucketHashIterator& rhs);
	inline LinkBucketHashIterator(PointerHash<T>* pointerhash, int bucket_num);
	
	inline void operator ++(int);//make return Entity& or something?	
	inline LinkBucketHashIterator& operator = (const LinkBucketHashIterator& rhs);
	inline bool operator != (const LinkBucketHashIterator& rhs);

	inline T* operator*() const; //dereference operator 
	
private:
	
	int bucket_num;
	PointerHash<T>* pointerhash;
};*/













/*====================================================================================
LinkBucketHash member function definitions
--------------------------------------------------------------------------------------

====================================================================================*/
template<class T>
LinkBucketHash<T>::LinkBucketHash(int num_buckets_)
{
	num_buckets = num_buckets_;
	
//	iterator end_it(this, num_buckets);
//	end_iterator = end_it;


	bucket = new Bucket[num_buckets];
}






template<class T>
PointerHash<T>::~PointerHash()
{
	delete[] bucket;//delete bucket array
}




template<class T>
void PointerHash<T>::add(T* p)
{
	int i = getBucketForHash(p->hash());

	//printf("bucket: %i\n", i);

	bucket[i].insert(p);	
}




template<class T>
void PointerHash<T>::remove(const T* p)
{
	int i = getBucketForHash(p->hash());

	assert(bucket[i].doesContain(p));

	bucket[i].remove(p);

	assert(!bucket[i].doesContain(p));
}



template<class T>
void PointerHash<T>::removeAll()
{
	for(int i=0; i<num_buckets; i++)
		bucket[i].removeAll();
}

template<class T>
bool PointerHash<T>::doesContain(const T* p)
{
	int i = getBucketForHash(p->hash());
	
	return bucket[i].doesContain(p);
}













/*
template<class T>
PointerHashIterator<T> PointerHash<T>::begin()
{
	///find first bucket with something in it:
	for(int i=0; i<num_buckets; i++)
		if(bucket[i])
			return PointerHashIterator<T>(this, i);

	//NOTE: could test against a stored value at object insertion instead of calculating it each time

	//if got here, all buckets must be empty
	return end();
}


template<class T>
int PointerHash<T>::hashPointer(const T* p) const
{
	return ((int)p / 4) % num_buckets;//NOTE: check this is good
	//divide too slow?
	//could use shift right
}
*/

template<class T>
int PointerHash<T>::numPointers()
{
	int count = 0;

	for(int i=0; i<num_buckets; i++)
		count += bucket[i].numPointers();

	return count;
}


template<class T>
float PointerHash<T>::hashLoad()
{
	return (float)numPointers() / (float)num_buckets;
}





/*====================================================================================
Bucket member function definitions
--------------------------------------------------------------------------------------

====================================================================================*/
class Bucket
{
	Bucket(){}
	~Bucket(){}

	void insert(T* p)
	{
		pointers.push_back(p);
	}

	void remove(const T* p)
	{
		pointers.remove(p);
	}

	void doesContain(const T* p) const
	{
		for(std::list<T*>::const_iterator i = pointers.begin(); i != pointers.end(); ++i)
		{
			if((*i) == p)
				return true;
		}

		return false;
	}

	int numPointers() const { return pointers.size(); }



private:
	std::list<T*> pointers;
};












/*====================================================================================
PointerHashIterator member function definitions
--------------------------------------------------------------------------------------

====================================================================================*/
/*
template <class T>
PointerHashIterator<T>::PointerHashIterator()
:	bucket_num(0),
	pointerhash(NULL)
{
}

template <class T>
PointerHashIterator<T>::PointerHashIterator(const PointerHashIterator& rhs)
//:	bucket_num(rhs.bucket_num),
//	pointerhash(rhs.pointerhash)
{
	bucket_num = rhs.bucket_num;
	pointerhash = rhs.pointerhash;
}

template <class T>
PointerHashIterator<T>::PointerHashIterator(PointerHash<T>* pointerhash_, int bucket_num_)
:	pointerhash(pointerhash_),
	bucket_num(bucket_num_)
{
	//entlist = elist;
	//bucket_num = bucket_num_;
}

template <class T>
void PointerHashIterator<T>::operator ++(int)//make return Entity*?
{	
	bucket_num++;//take at least one step through the array

	//now move to the next full bucket
	while(pointerhash->bucket[bucket_num] == NULL)
	{
		bucket_num++;	

		if(bucket_num == pointerhash->num_buckets)
			break;//just stop once moved past end of array
	}
}

template <class T>
PointerHashIterator<T>& PointerHashIterator<T>::operator = (const PointerHashIterator<T>& rhs)
{
	pointerhash = rhs.pointerhash;
	bucket_num = rhs.bucket_num;

	return *this;
}



template <class T>
bool PointerHashIterator<T>::operator != (const PointerHashIterator& rhs)
{
	if(bucket_num != rhs.bucket_num)
		return true;

	return false;
}

template <class T>
T* PointerHashIterator<T>::operator*() const //dereference operator 
{
	return pointerhash->bucket[bucket_num];
}
*/


#endif //__POINTERHASH_H__
