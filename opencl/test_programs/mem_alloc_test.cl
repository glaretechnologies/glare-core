/* Copyright Glare Technologies Limited 2019 - */

__kernel void testKernel(
	__global int* data,
	ulong data_size,
	__global int* result
	)
{
//	printf("AAAdata_size: %i\n", (int)data_size);
	int sum = 0;
	//for(ulong i=0; i<data_size; ++i)
	sum += data[0];
	//sum += data[data_size / 2];
	//sum += data[data_size / 3];
	//sum += data[data_size - 1];
	result[0] = sum;
}
