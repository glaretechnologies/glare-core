/*=====================================================================
zlibutils.cpp
-------------
File created by ClassTemplate on Fri Oct 08 02:21:10 2004Code By Nicholas Chapman.
=====================================================================*/
#include "zlibutils.h"


#include "../zlib121/zlib.h"
#include <assert.h>


/*ZLibUtils::ZLibUtils()
{
	
}


ZLibUtils::~ZLibUtils()
{
	
}*/

	//throws ZLibUtilsExcep
void ZLibUtils::compress(const std::vector<char>& data, 
					std::vector<char>& compressed_data_out)
{
	compress(&(*data.begin()), (int)data.size(), compressed_data_out);
}


void ZLibUtils::compress(const void* data, const int datalen, 
					std::vector<char>& compressed_data_out)
{
	//create a vector with enough space to hold compressed data.	
	compressed_data_out.resize(::compressBound(datalen));


	uLongf compressed_len = compressed_data_out.size();
		//this will be set to the actual compressed length by compress()

	const int result = ::compress((Bytef*)&(*compressed_data_out.begin()), &compressed_len, 
									(Bytef*)data, datalen);
	
	if(result != Z_OK)
	{
		throw ZLibUtilsExcep("ZLib compression failed.");
	}

	//lop off any excess data.
	compressed_data_out.resize((int)compressed_len);

}



void ZLibUtils::decompress(const void* compressed_data, 
							const int compressed_len,
							const int decompressed_len,
							std::vector<char>& decompressed_data_out)
{

	decompressed_data_out.resize(decompressed_len);

	uLongf destlen = decompressed_len;
			
	const int result = ::uncompress((Bytef*)&(*decompressed_data_out.begin()), 
									&destlen, 
									(Bytef*)(compressed_data), 
									compressed_len);

	//assert(destlen == decompressed_length);

	if(result != Z_OK)
	{
		throw ZLibUtilsExcep("ZLib decompression failed.");
	}

	if(destlen != decompressed_len)
	{
		throw ZLibUtilsExcep("destlen == decompressed_len.");
	}

}

unsigned int ZLibUtils::calcChecksum(const void* data, const int datalen)
{
	assert(sizeof(uLong) == sizeof(unsigned int));

	uLong adler = adler32(0L, Z_NULL, 0);

	adler = adler32(adler, (const unsigned char*)data, datalen);

	return (unsigned int)adler;
}
