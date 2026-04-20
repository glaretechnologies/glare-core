/*=====================================================================
StackStringBuilder.h
--------------------
Copyright Glare Technologies Limited 2026 - 
=====================================================================*/
#pragma once


#include "StackAllocator.h"
#include "string_view.h"


struct StackStringBuilder
{
	StackStringBuilder(glare::StackAllocator& stack_allocator_, size_t max_size_ = 8192)
	:	stack_allocator(stack_allocator_),
		size(0),
		max_size(max_size_)
	{
		buf = (char*)stack_allocator_.alloc(max_size_, /*alignment=*/8);
	}

	~StackStringBuilder()
	{
		stack_allocator.free(buf);
	}

	GLARE_DISABLE_COPY(StackStringBuilder);


	void append(const string_view s)
	{
		const size_t num_to_copy = myMin(max_size - size, s.length());
		std::memcpy(buf + size, s.data(), num_to_copy);
		size += num_to_copy;
		assert(size <= max_size);
	}

	string_view getStringView() const { return string_view(buf, size); }

	static void test();

	glare::StackAllocator& stack_allocator;
	size_t size;
	size_t max_size;
	char* buf;
};
