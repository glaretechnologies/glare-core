/*=====================================================================
CircularBuffer.cpp
-------------------
Copyright Glare Technologies Limited 2017 -
Generated at 2013-05-16 16:42:23 +0100
=====================================================================*/
#include "CircularBuffer.h"


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../maths/PCG32.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"


struct TestCircBufferStruct : public RefCounted
{
};



class CircBufTestAllocator : public glare::Allocator
{
public:
	virtual void* alloc(size_t size, size_t alignment)
	{
		(*i)++;
		return MemAlloc::alignedMalloc(size, alignment);
	}

	virtual void free(void* ptr)
	{
		if(ptr)
		{
			MemAlloc::alignedFree(ptr);
			(*i)--;
		}
	}

	int* i;
};


void circularBufferTest()
{
	conPrint("circularBufferTest()");

	//======================== Test construction and destruction ========================
	{
		CircularBuffer<int> buf;
		testAssert(buf.empty());
	}

	//======================== Test push_back ========================
	{
		CircularBuffer<int> buf;
		buf.push_back(1);

		testAssert(buf.begin == 0);
		testAssert(buf.size() == 1);
		testAssert(buf.front() == 1);
		testAssert(buf.back() == 1);
		testAssert(!buf.empty());

		buf.push_back(2);

		testAssert(buf.begin == 0);
		testAssert(buf.size() == 2);
		testAssert(buf.front() == 1);
		testAssert(buf.back() == 2);

		buf.push_back(3);

		testAssert(buf.begin == 0);
		testAssert(buf.size() == 3);
		testAssert(buf.front() == 1);
		testAssert(buf.back() == 3);
	}

	{
		CircularBuffer<int> buf;

		for(size_t i=0; i<1000; ++i)
		{
			buf.push_back((int)i);

			testAssert(buf.begin == 0);
			testAssert(buf.size() == i + 1);
			testAssert(buf.front() == 0);
			testAssert(buf.back() == i);
		}
	}

	// Test with non-trivial construtor and destructor
	{
		CircularBuffer<Reference<TestCircBufferStruct> > buf;
		Reference<TestCircBufferStruct> t = new TestCircBufferStruct();
		Reference<TestCircBufferStruct> t2 = new TestCircBufferStruct();
		Reference<TestCircBufferStruct> t3 = new TestCircBufferStruct();
		Reference<TestCircBufferStruct> t4 = new TestCircBufferStruct();
		Reference<TestCircBufferStruct> t5 = new TestCircBufferStruct();
		buf.push_back(t);

		testAssert(buf.begin == 0);
		testAssert(buf.size() == 1);
		testAssert(buf.front().getPointer() == t.getPointer());
		testAssert(buf.back().getPointer() == t.getPointer());
		testAssert(!buf.empty());
		testAssert(t->getRefCount() == 2);

		buf.push_back(t2);

		testAssert(buf.begin == 0);
		testAssert(buf.size() == 2);
		testAssert(buf.front().getPointer() == t.getPointer());
		testAssert(buf.back().getPointer() == t2.getPointer());
		testAssert(t2->getRefCount() == 2);

		buf.push_back(t3);

		testAssert(buf.begin == 0);
		testAssert(buf.size() == 3);
		testAssert(buf.front().getPointer() == t.getPointer());
		testAssert(buf.back().getPointer() == t3.getPointer());
		testAssert(t3->getRefCount() == 2);

		buf.push_back(t4);

		testAssert(buf.begin == 0);
		testAssert(buf.size() == 4);
		testAssert(buf.front().getPointer() == t.getPointer());
		testAssert(buf.back().getPointer() == t4.getPointer());
		testAssert(t4->getRefCount() == 2);

		buf.push_back(t5);

		testAssert(buf.begin == 0);
		testAssert(buf.size() == 5);
		testAssert(buf.front().getPointer() == t.getPointer());
		testAssert(buf.back().getPointer() == t5.getPointer());
		testAssert(t5->getRefCount() == 2);

		testAssert(t ->getRefCount() == 2);
		testAssert(t2->getRefCount() == 2);
		testAssert(t3->getRefCount() == 2);
		testAssert(t4->getRefCount() == 2);
		testAssert(t5->getRefCount() == 2);

		buf.clear();

		testAssert(t ->getRefCount() == 1);
		testAssert(t2->getRefCount() == 1);
		testAssert(t3->getRefCount() == 1);
		testAssert(t4->getRefCount() == 1);
		testAssert(t5->getRefCount() == 1);
	}

	
	{
		Reference<TestCircBufferStruct> t = new TestCircBufferStruct();
		{
			CircularBuffer<Reference<TestCircBufferStruct> > buf;
			buf.push_back(t);
			testAssert(t->getRefCount() == 2);
		}
		testAssert(t->getRefCount() == 1);
	}

	//======================== Test push_front ========================
	{
		CircularBuffer<int> buf;
		buf.push_front(100);

		testAssert(buf.begin == 3);
		testAssert(buf.end == 0);
		testAssert(buf.size() == 1);
		testAssert(buf.front() == 100);
		testAssert(buf.back() == 100);

		buf.push_front(99);

		testAssert(buf.begin == 2);
		testAssert(buf.end == 0);
		testAssert(buf.size() == 2);
		testAssert(buf.front() == 99);
		testAssert(buf.back() == 100);

		buf.push_front(98);

		testAssert(buf.begin == 1);
		testAssert(buf.end == 0);
		testAssert(buf.size() == 3);
		testAssert(buf.front() == 98);
		testAssert(buf.back() == 100);

		buf.push_front(97);
/*
		testAssert(buf.begin == 0);
		testAssert(buf.end == 0);
		testAssert(buf.size() == 4);
		testAssert(buf.front() == 97);
		testAssert(buf.back() == 100);

		buf.push_front(96);

		testAssert(buf.begin == 7);
		testAssert(buf.end == 0);
		testAssert(buf.size() == 5);
		testAssert(buf.front() == 96);
		testAssert(buf.back() == 100);

		buf.push_front(95);

		testAssert(buf.begin == 0);
		testAssert(buf.end == 6);
		testAssert(buf.size() == 6);
		testAssert(buf.front() == 95);
		testAssert(buf.back() == 100);

		buf.push_front(94);

		testAssert(buf.begin == 7);
		testAssert(buf.end == 6);
		testAssert(buf.size() == 7);
		testAssert(buf.front() == 94);
		testAssert(buf.back() == 100);

		buf.push_front(93);

		testAssert(buf.begin == 6);
		testAssert(buf.end == 6);
		testAssert(buf.size() == 8);
		testAssert(buf.front() == 93);
		testAssert(buf.back() == 100);*/
	}

	{
		CircularBuffer<int> buf;

		for(size_t i=0; i<1000; ++i)
		{
			buf.push_front((int)i);

			testAssert(buf.size() == i + 1);
			testAssert(buf.front() == i);
			testAssert(buf.back() == 0);
		}
	}


	//======================== Test pop_back ========================
	{
		CircularBuffer<int> buf;
		buf.push_back(1);
		buf.pop_back();
		testAssert(buf.size() == 0);
	}


	{
		CircularBuffer<int> buf;

		// Make a queue (0, 1, 2, ... n-2, n-1)
		const size_t N = 1000;
		for(size_t i=0; i<N; ++i)
		{
			buf.push_back((int)i);

			testAssert(buf.front() == 0);
			testAssert(buf.back() == (int)i);
			testAssert(buf.size() == i + 1);
		}

		for(int i=(int)N-1; i>=0; --i)
		{
			testAssert(buf.back() == i);
			buf.pop_back();
		}

		testAssert(buf.size() == 0);
	}

	//======================== Test pop_front ========================
	{
		CircularBuffer<int> buf;
		buf.push_front(1);
		buf.pop_front();
		testAssert(buf.size() == 0);
	}


	{
		CircularBuffer<int> buf;

		// Make a queue (0, 1, 2, ... n-2, n-1)
		const size_t N = 1000;
		for(size_t i=0; i<N; ++i)
		{
			buf.push_back((int)i);
			testAssert(buf.size() == i + 1);
		}

		for(size_t i=0; i<N; ++i)
		{
			testAssert(buf.front() == i);
			buf.pop_front();
		}

		testAssert(buf.size() == 0);
	}


	//======================== Test a mix of push_front and push_back ========================
	{
		CircularBuffer<int> buf;

		// Make a queue (n-1, n-2, ...., 2, 1, 0, 0, 1, 2, ... n-2, n-1)
		const size_t N = 1000;
		for(size_t i=0; i<N; ++i)
		{
			buf.push_front((int)i);
			buf.push_back((int)i);

			testAssert(buf.front() == (int)i);
			testAssert(buf.back() == (int)i);
			testAssert(buf.size() == 2*(i+1));
		}

		testAssert(buf.size() == 2*N);

		// Remove (n-1, n-2, ...., 2, 1, 0) prefix
		for(int i=(int)N-1; i>=0; --i)
		{
			testAssert(buf.front() == i);
			buf.pop_front();
		}

		testAssert(buf.size() == N);

		// Remove (0, 1, 2, ... n-2, n-1)
		for(size_t i=0; i<N; ++i)
		{
			testAssert(buf.front() == i);
			buf.pop_front();
		}

		testAssert(buf.size() == 0);
	}


	//======================== CircularBufferIterator ========================
	{
		CircularBuffer<int> buf;

		testAssert(buf.beginIt() == buf.endIt());

		// Make sure iteration over an empty buffer does nothing.
		size_t i = 0;
		for(CircularBuffer<int>::iterator it = buf.beginIt(); it != buf.endIt(); ++it)
			i++;
		testAssert(i == 0);

		buf.push_back(1);
		buf.pop_back();

		// Make sure iteration over an empty buffer (with data.size() > 0) does nothing.
		i = 0;
		for(CircularBuffer<int>::iterator it = buf.beginIt(); it != buf.endIt(); ++it)
			i++;
		testAssert(i == 0);



		// Make a queue (0, 1, 2, ... n-2, n-1)
		const size_t N = 1000;
		for(size_t z=0; z<N; ++z)
			buf.push_back((int)z);

		i = 0;
		for(CircularBuffer<int>::iterator it = buf.beginIt(); it != buf.endIt(); ++it)
		{
			testAssert((*it) == i);
			i++;
		}
		testAssert(i == N);
	}

	{
		for(int n=0; n<100; ++n)
		{
			CircularBuffer<int> buf;

			// Make a queue (n-1, n-2, ...., 2, 1, 0)
			for(size_t i=0; i<n; ++i)
				buf.push_front((int)i);

			size_t i = 0;
			for(CircularBuffer<int>::iterator it = buf.beginIt(); it != buf.endIt(); ++it)
			{
				testAssert((*it) == (int) n - 1 - i);
				i++;
			}
			testAssert(i == n);
		}
	}

	//======================== Try stress testing with random operations ========================
	{
		int64 sum = 0;
		for(int t=0; t<1000; ++t)
		{
			PCG32 rng(t);
			if(t % 10000 == 0)
				printVar(t);

			Reference<TestCircBufferStruct> test_struct = new TestCircBufferStruct();
			
			{
				CircularBuffer<Reference<TestCircBufferStruct> > buf;
				
				for(int i=0; i<1000; ++i)
				{
					const float r = rng.unitRandom();
					if(r < 0.2f)
					{
						buf.push_back(test_struct);
					}
					else if(r < 0.4f)
					{
						buf.push_front(test_struct);
					}
					else if(r < 0.6f)
					{
						if(!buf.empty())
							buf.pop_back();
					}
					else if(r < 0.8f)
					{
						if(!buf.empty())
							buf.pop_front();
					}
					else if(r < 0.85f)
					{
						if(!buf.empty())
							sum += buf.front()->getRefCount();
					}
					else if(r < 0.9f)
					{
						if(!buf.empty())
							sum += buf.back()->getRefCount();
					}
					else if(r < 0.95f)
					{
						sum += buf.size();
					}
					else
					{
						buf.clear();
					}

					testAssert(test_struct->getRefCount() == (int64)(1 + buf.size()));

					// Test iterating over resulting buffer
					size_t c = 0;
					for(CircularBuffer<Reference<TestCircBufferStruct> >::iterator it = buf.beginIt(); it != buf.endIt(); ++it)
						c++;
					testAssert(c == buf.size());
				}
			}

			testAssert(test_struct->getRefCount() == 1);
			
		}

		conPrint("sum: " + toString(sum));
	}


	//========================= Test with allocator =========================
	{
		int i = 0;
		{
			CircularBuffer<int> v;

			Reference<CircBufTestAllocator> al = new CircBufTestAllocator();
			al->i = &i;
			v.setAllocator(al);

			v.push_back(1);
			v.push_back(2);
			v.push_back(3);
		}
		testAssert(i == 0);
	}

	conPrint("circularBufferTest() done.");
}


#endif // BUILD_TESTS
