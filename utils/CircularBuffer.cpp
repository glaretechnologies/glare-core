/*=====================================================================
CircularBuffer.cpp
------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "CircularBuffer.h"


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../maths/PCG32.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include <numeric>


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

	virtual std::string getDiagnostics() const { return std::string(); }

	int* i;
};


void circularBufferTest()
{
	conPrint("circularBufferTest()");

	//======================== Test pushBackNItems ========================
	{
		CircularBuffer<int> buf;
		
		const int items[] = { 0, 1, 2, 3 };
		buf.pushBackNItems(items, 4);
		
		testAssert(buf.size() == 4);
		int i = 0;
		for(CircularBuffer<int>::iterator it = buf.beginIt(); it != buf.endIt(); ++it, ++i)
			testAssert(*it == i);

		buf.pushBackNItems(items, 4);

		testAssert(buf.size() == 8);
		i = 0;
		for(CircularBuffer<int>::iterator it = buf.beginIt(); it != buf.endIt(); ++it, ++i)
		{
			if(i < 4)
				testAssert(*it == i);
			else
				testAssert(*it == i - 4);
		}
	}

	// Test pushBackNItems that wraps when inserting
	{
		CircularBuffer<int> buf;
		std::vector<int> items(32);
		const int N = 32;
		for(int i=0; i<N; ++i)
			items[i] = i;
		
		buf.pushBackNItems(items.data(), N);
		testAssert(buf.size() == N);

		for(int i=0; i<32; ++i)
		{
			testAssert(buf.front() == i);
			buf.pop_front();
		}

		testAssert(buf.size() == 0);

		buf.pushBackNItems(items.data(), N); // This should wrap around while inserting
		testAssert(buf.size() == N);
		testAssert(buf.end < buf.begin); // Check it wrapped

		for(int i=0; i<32; ++i)
		{
			testAssert(buf.front() == i);
			buf.pop_front();
		}

		testAssert(buf.size() == 0);
	}

	// Test pushBackNItems where we end up with exactly end == data_size
	{
		CircularBuffer<int> buf;
		std::vector<int> items(32);
		const int N = 32;
		for(int i=0; i<N; ++i)
			items[i] = i;

		buf.pushBackNItems(items.data(), N);
		testAssert(buf.size() == N);

		for(int i=0; i<N; ++i)
		{
			testAssert(buf.front() == i);
			buf.pop_front();
		}

		testAssert(buf.size() == 0);

		const size_t remaining_before_wrap = buf.data_size - buf.begin;

		items.resize(remaining_before_wrap);

		buf.pushBackNItems(items.data(), remaining_before_wrap); // This should wrap around while inserting
		testAssert(buf.size() == remaining_before_wrap);
		testAssert(buf.end < buf.begin); // Check it wrapped
	}

	//======================== Test popFrontNItems ========================
	{
		CircularBuffer<int> buf;

		const int items[] = { 0, 1, 2, 3 };
		buf.pushBackNItems(items, 4);

		int popped_items[4];
		buf.popFrontNItems(popped_items, 4);

		for(int i=0; i<4; ++i)
			testAssert(items[i] == popped_items[i]);

		testAssert(buf.size() == 0);
	}

	{
		CircularBuffer<int> buf;

		const int N = 10000;
		std::vector<int> items(N);
		std::iota(items.begin(), items.end(), 0);
		buf.pushBackNItems(items.data(), N);

		for(int z=0; z<N/10; ++z)
		{
			int popped_items[10];
			buf.popFrontNItems(popped_items, 10);
			for(int i=0; i<10; ++i)
				testEqual(popped_items[i], z*10 + i);
		}

		testAssert(buf.size() == 0);
	}

	// Test where we wrap while popping, e.g. popping 5 items from the front of:
	// 
	// |-3-|-4-|-5-|   |   |   |   |   |   |   |-1-|-2-|
	//               ^                           ^
	//              end                        begin
	{
		CircularBuffer<int> buf;

		const int N = 100;
		std::vector<int> items(N);
		// Make sure capacity is large
		buf.pushBackNItems(items.data(), N);
		for(int i=0; i<N; ++i)
			buf.pop_back();
		testAssert(buf.size() == 0);
		testAssert(buf.begin == 0);

		buf.push_front(2);
		buf.push_front(1);
		buf.push_back(3);
		buf.push_back(4);
		buf.push_back(5);

		testAssert(buf.size() == 5);
		testAssert(buf.end < buf.begin); // Check wrapped

		buf.popFrontNItems(items.data(), 5);
		for(int i=0; i<5; ++i)
			testAssert(items[i] == i + 1);

		testAssert(buf.size() == 0);
		testAssert(buf.begin == 3);
	}


	// Test where we wrap while popping, where we wend up with exactly begin == data_size, which then wraps to 0.
	// e.g. popping 3 items from the front of:
	// 
	// |-3-|-4-|-5-|   |   |   |   |   |   |   |-1-|-2-|
	//               ^                           ^
	//              end                        begin
	{
		CircularBuffer<int> buf;

		const int N = 100;
		std::vector<int> items(N);
		// Make sure capacity is large
		buf.pushBackNItems(items.data(), N);
		for(int i=0; i<N; ++i)
			buf.pop_back();
		testAssert(buf.size() == 0);
		testAssert(buf.begin == 0);

		buf.push_front(2);
		buf.push_front(1);
		buf.push_back(3);
		buf.push_back(4);
		buf.push_back(5);

		testAssert(buf.size() == 5);
		testAssert(buf.end < buf.begin); // Check wrapped

		buf.popFrontNItems(items.data(), 2);
		for(int i=0; i<2; ++i)
			testAssert(items[i] == i + 1);

		testAssert(buf.size() == 3);
		testAssert(buf.begin == 0);
	}

	


	//======================== Test construction and destruction ========================
	{
		CircularBuffer<int> buf;
		testAssert(buf.empty());
		testAssert(!buf.nonEmpty());
	}

	//======================= Test operator = ========================
	{
		{
			CircularBuffer<int> buf;
			CircularBuffer<int> buf2;
			buf2 = buf;
			testAssert(buf.empty());
			testAssert(buf2.empty());
		}

		{
			CircularBuffer<int> buf;

			for(size_t i=0; i<32; ++i)
				buf.push_back((int)i);

			for(size_t i=0; i<10; ++i)
				buf.pop_front();

			CircularBuffer<int> buf2;
			buf2 = buf;

			testAssert(buf.size() == 32 - 10);
			testAssert(buf2.size() == 32 - 10);
			for(int i=10; i<32; ++i)
			{
				testAssert(buf2.front() == i);
				buf2.pop_front();
			}

			buf.push_back(100); // Should wrap around in buf

			buf2 = buf;
			testAssert(buf2.size() == 32 - 10 + 1);
			for(int i=10; i<32; ++i)
			{
				testAssert(buf2.front() == i);
				buf2.pop_front();
			}
			testAssert(buf2.front() == 100);
			buf2.pop_front();
		}

		// Test operator = with references (tests non-trivial constructors and destructors)
		{
			Reference<TestCircBufferStruct> t = new TestCircBufferStruct();

			testAssert(t->getRefCount() == 1);

			CircularBuffer<Reference<TestCircBufferStruct>> buf;

			for(int i=0; i<100; ++i)
				buf.push_back(t);

			testAssert(t->getRefCount() == 101);

			CircularBuffer<Reference<TestCircBufferStruct>> buf2;

			for(int i=0; i<10; ++i)
				buf2.push_back(t);

			testAssert(t->getRefCount() == 111);

			buf2 = buf;

			testAssert(t->getRefCount() == 201);
		}

		// Test operator = with references (tests non-trivial constructors and destructors), with a wrapped buffer
		{
			Reference<TestCircBufferStruct> t = new TestCircBufferStruct();

			testAssert(t->getRefCount() == 1);

			CircularBuffer<Reference<TestCircBufferStruct>> buf;

			for(int i=0; i<100; ++i)
				buf.push_back(t);

			testAssert(t->getRefCount() == 101);

			for(int i=0; i<50; ++i)
				buf.pop_front(); // data [0, 50) should be free now.

			for(int i=0; i<50; ++i)
				buf.push_back(t);

			testAssert(t->getRefCount() == 101);
			testAssert(&(*buf.beginIt()) > &(*buf.endIt())); // Test buf is wrapped


			CircularBuffer<Reference<TestCircBufferStruct>> buf2;

			for(int i=0; i<100; ++i)
				buf2.push_back(t);
			for(int i=0; i<50; ++i)
				buf2.pop_front(); // data [0, 50) should be free now.
			for(int i=0; i<50; ++i)
				buf2.push_back(t);

			testAssert(&(*buf2.beginIt()) > &(*buf2.endIt())); // Test buf2 is wrapped

			testAssert(t->getRefCount() == 201);

			buf2 = buf;

			testAssert(t->getRefCount() == 201);
		}
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
			testAssert(buf.back() == (int)i);
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
			testAssert(buf.front() == (int)i);
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
			testAssert(buf.front() == (int)i);
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
			testAssert(buf.front() == (int)i);
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
			testAssert((*it) == (int)i);
			i++;
		}
		testAssert(i == N);
	}

	{
		for(int n=0; n<100; ++n)
		{
			CircularBuffer<int> buf;

			// Make a queue (n-1, n-2, ...., 2, 1, 0)
			for(int i=0; i<n; ++i)
				buf.push_front(i);

			size_t i = 0;
			for(CircularBuffer<int>::iterator it = buf.beginIt(); it != buf.endIt(); ++it)
			{
				testAssert((*it) == n - 1 - (int)i);
				i++;
			}
			testAssert((int)i == n);
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
				int expected_size = 0;
				
				for(int i=0; i<1000; ++i)
				{
					const float r = rng.unitRandom();
					if(r < 0.1f)
					{
						// Test push_back
						buf.push_back(test_struct);
						expected_size++;
					}
					else if(r < 0.2f)
					{
						// Test push_front
						buf.push_front(test_struct);
						expected_size++;
					}
					else if(r < 0.3f)
					{
						// Test pop_back
						if(!buf.empty())
						{
							buf.pop_back();
							expected_size--;
						}
					}
					else if(r < 0.4f)
					{
						// Test pop_front
						if(!buf.empty())
						{
							buf.pop_front();
							expected_size--;
						}
					}
					else if(r < 0.5f)
					{
						// Test buf.front()
						if(!buf.empty())
							sum += buf.front()->getRefCount();
					}
					else if(r < 0.6f)
					{
						// Test buf.back()
						if(!buf.empty())
							sum += buf.back()->getRefCount();
					}
					else if(r < 0.7f)
					{
						// Test buf.size()
						sum += buf.size();
					}
					else if(r < 0.8f)
					{
						// Test popFrontNItems
						const int num = rng.nextUInt((uint32)buf.size() + 1);
						std::vector<Reference<TestCircBufferStruct>> popped(num);
						buf.popFrontNItems(popped.data(), num);
						expected_size -= num;
					}
					else if(r < 0.9f)
					{
						// Test pushBackNItems
						const int num = rng.nextUInt(10);
						std::vector<Reference<TestCircBufferStruct>> to_push(num, test_struct);
						buf.pushBackNItems(to_push.data(), num);
						expected_size += num;
					}
					else if(r < 0.96f)
					{
						// Test operator = 
						{
							CircularBuffer<Reference<TestCircBufferStruct> > buf2;
							buf2 = buf;

							testAssert(buf2.size() == buf.size());

							testAssert(test_struct->getRefCount() == (int64)(1 + 2 * buf.size()));
						}

						testAssert(test_struct->getRefCount() == (int64)(1 + buf.size()));
					}
					else if(r < 0.97f)
					{
						// Test iteration
						int c = 0;
						for(auto it = buf.beginIt(); it != buf.endIt(); ++it)
						{
							sum += (*it)->getRefCount();
							c++;
						}
						testAssert(c == expected_size);
					}
					else if(r < 0.98f)
					{
						// Test clear();
						buf.clear();
						expected_size = 0;
					}
					else
					{
						// Test clearAndFreeMem();
						buf.clearAndFreeMem();
						expected_size = 0;
					}

					testAssert(expected_size == (int)buf.size());
					testAssert(test_struct->getRefCount() == (int64)(1 + buf.size()));
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
