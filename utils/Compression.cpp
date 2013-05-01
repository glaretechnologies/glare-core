/*=====================================================================
Compression.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Thu Jan 13 16:39:20 +1300 2011
=====================================================================*/
#include "Compression.h"


#include "Exception.h"
#include "platform.h"
#include <zlib.h>


Compression::Compression()
{

}


Compression::~Compression()
{

}


void Compression::compress(const char* data, size_t size, std::vector<char>& data_out)
{
	const uLong bound = compressBound((uLong)size);
	
	data_out.resize(bound + 4);

	// Write decompressed size to data_out
	union intchar
	{
		char c[4];
		uint32 i;
	};

	intchar d;
	d.i = (uint32)size;

	data_out[0] = d.c[0];
	data_out[1] = d.c[1];
	data_out[2] = d.c[2];
	data_out[3] = d.c[3];

	uLong dest_len = bound;
	
	int result = ::compress2(
		(Bytef*)&data_out[4], // dest
		&dest_len, // dest len
		(Bytef*)data, // source
		(uLong)size, // source len
		Z_BEST_COMPRESSION // Compression level
	);

	if(result != Z_OK)
		throw Indigo::Exception("Compression failed.");

	data_out.resize(dest_len + 4);


	
	
	/*
	z_stream stream;

	// Allocate deflate state
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	int ret = deflateInit(&stream, Z_DEFAULT_COMPRESSION);
	if(ret != Z_OK)
		throw Indigo::Exception("Compression failed.");

	stream.next_in = (Bytef*)data;
	stream.avail_in = size;

	const unsigned int bounded_size = deflateBound(&stream, size);

	data_out.resize(bounded_size);


	int result = deflate(&stream, Z_FINISH);
	if(result != Z_STREAM_END)
		throw Indigo::Exception("Compression failed.");
	*/

/*
	char buf[2048];

	while(stream.next_in > 0)
	{
		//data_out.resize(data_out.size() + 1024);
		
		stream.next_out = (Bytef*)buf;
		stream.avail_out = sizeof(buf);

		const uLong prev_total_out = stream.total_out;

		deflate(&stream, Z_NO_FLUSH);

		deflateBound

		const unsigned int num_new_output = stream.total_out - prev_total_out;

		data_out.resize(data_out.size() + num_new_output);
		for(unsigned int i=0; i<num_new_output; ++i)
			data_out[prev_total_out + i] = buf[i];
	}*/

	//deflateEnd(&stream);
}


size_t Compression::decompressedSize(const char* data, size_t size)
{
	// Read decompressed size
	union intchar
	{
		char c[4];
		uint32 i;
	};

	intchar d;

	d.c[0] = data[0];
	d.c[1] = data[1];
	d.c[2] = data[2];
	d.c[3] = data[3];


	return d.i;
}


void Compression::decompress(const char* data, size_t size, char* data_out, size_t data_out_size)
{
	//data_out.resize(decompressed_size);

	uLong decompressed_size = (uLong)decompressedSize(data, size);

	if(decompressed_size != data_out_size)
		throw Indigo::Exception("Decompression failed: decompressed_size != data_out_size");

	int result = ::uncompress(
		(Bytef*)data_out, // dest
		&decompressed_size, // dest len
		(Bytef*)data + 4, // source
		(uLong)size - 4 // source len
	);

	if(result != Z_OK)
		throw Indigo::Exception("Decompression failed.");

	/*z_stream stream;

	// Allocate deflate state
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;

	stream.next_in = (Bytef*)data;
	stream.avail_in = size;


	int ret = deflateInit(&stream, Z_DEFAULT_COMPRESSION);
	if(ret != Z_OK)
		throw Indigo::Exception("Compression failed.");


	inflateEnd(&stream);*/
}


#if BUILD_TESTS


#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"
#include "../utils/timer.h"


void Compression::test()
{
	{
		size_t n = 100000;
		std::vector<char> d(n);
		for(size_t i=0; i<n; ++i)
		{
			d[i] = i % 128;
		}

		std::vector<char> compressed;


		Compression::compress((const char*)&d[0], d.size(), compressed);

		testAssert(Compression::decompressedSize(&compressed[0], compressed.size()) == n);

		// Alloc buffer for decompressed data
		std::vector<char> decompressed(
			Compression::decompressedSize(&compressed[0], compressed.size())
		);


		Compression::decompress(&compressed[0], compressed.size(), &decompressed[0], decompressed.size());

		testAssert(decompressed.size() == n);
		for(size_t i=0; i<n; ++i)
		{
			testAssert(decompressed[i] == (char)(i % 128));
		}
	}


	// Do performance test
	{
		
		size_t n = 100000;
		std::vector<char> d(n);
		for(size_t i=0; i<n; ++i)
		{
			d[i] = i % 128;
		}

		std::vector<char> compressed;

		Timer timer;

		Compression::compress((const char*)&d[0], d.size(), compressed);

		testAssert(Compression::decompressedSize(&compressed[0], compressed.size()) == n);

		// Alloc buffer for decompressed data
		std::vector<char> decompressed(
			Compression::decompressedSize(&compressed[0], compressed.size())
		);

		Compression::decompress(&compressed[0], compressed.size(), &decompressed[0], decompressed.size());

		conPrint(timer.elapsedString());
	}
}


#endif
