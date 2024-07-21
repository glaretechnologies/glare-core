/*=====================================================================
SocketInterface.cpp
-------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "SocketInterface.h"


#include "../utils/RuntimeCheck.h"


SocketInterface::~SocketInterface()
{}


size_t SocketInterface::readSomeBytesChecked(MutableArrayRef<uint8> buf, size_t buf_index, size_t max_num_bytes)
{
	if(max_num_bytes > 0)
	{
		runtimeCheck(
			!Maths::unsignedIntAdditionWraps(buf_index, max_num_bytes) &&
			((buf_index + max_num_bytes) <= buf.size())
		);

		return readSomeBytes(buf.data() + buf_index, max_num_bytes);
	}
	else
		return 0;
}
