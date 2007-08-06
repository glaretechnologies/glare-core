#include "bitarray.h"

#include <assert.h>
#include <stdio.h>
#include <algorithm>

//#include "main.h"

BitArray::BitArray()
{
	datasize = 0;
	numbits = 0;
	data = NULL;
	bitindex = 0;
	bitoffset = 0;
	byteindex = 0;
}


BitArray::BitArray(int numbits_)
:	datasize(0)
{
	numbits = numbits_;

	datasize = getNumBytesToHold(numbits);

	data = new BYTE[datasize];

	bitindex = 0;
	bitoffset = 0;
	byteindex = 0;
}

BitArray::~BitArray()
{
	delete[] data;
}






void BitArray::printBits(std::string& bitstring_out) const
{
	for(int i=0; i<getNumBits(); ++i)
	{
		bitstring_out += printBit(i);
	}//SLOW: fixme

	/*int bitnum = 0;
	for(int i=0; i<datasize; i++)
	{
		printf("(");
		for(int b=0; b<8; b++)
		{
			printBit(i*8 + b);
			bitnum++;

			if(bitnum == numbits)
			{
				printf(")");
				return;
			}
			//NOTE: fixme nasty

		}
		printf(")");
	}*/
}
	
char BitArray::printBit(int index) const
{
	if(getBit(index))
	{
		return '1';
	}
	else
	{
		return '0';
	}
}

int	BitArray::writeToBuffer(BYTE* buffertowriteto) const
{
	int numbytestowrite = getNumBytes();

	for(int i=0; i<numbytestowrite; i++)
	{
		buffertowriteto[i] = data[i];
	}

	return numbytestowrite;
}

void BitArray::startWriting()
{
	bitindex = 0;
	bitoffset = 0;
	byteindex = 0;
}
	


void BitArray::writeBits(const BitArray& bitsource)
{
	checkResize(bitindex + bitsource.getNumBits());//NOTE: check this is sufficient

	for(int i=0; i<bitsource.getNumBits(); i++)
	{
		writeBitUnchecked(bitsource.getBit(i));
	}
	//NOTE: this is ultra slow :(
}


void BitArray::writeBits(const BitArray& bitsource, int num_bits_to_copy_from_source)
{
	checkResize(bitindex + myMin(bitsource.getNumBits(), num_bits_to_copy_from_source) );

	for(int i=0; i<bitsource.getNumBits() && i<num_bits_to_copy_from_source; i++)
	{
		writeBit(bitsource.getBit(i));
	}
}


int BitArray::writeBitsFromBuffer(const BYTE* buffer, int num_bits_to_read)
{
	
	//-----------------------------------------------------------------
	//extremely fast special case if bitoffset == 0.
	//In this case the bytes from 'buffer' can just be copied straight across to 'data'.
	//-----------------------------------------------------------------
	if(bitoffset == 0)
	{
		//-----------------------------------------------------------------
		//calculate the number of bytes to copy across
		//-----------------------------------------------------------------
		int numbytestoread = getNumBytesToHold(num_bits_to_read);

		
		//-----------------------------------------------------------------
		//make sure the data array is big enough
		//-----------------------------------------------------------------
		checkResize(bitindex + 8*numbytestoread);

		//-----------------------------------------------------------------
		//get a pointer to the current write position in the data array
		//-----------------------------------------------------------------
		BYTE* datawritepos = &data[byteindex];

		//-----------------------------------------------------------------
		//copy the bytes across.  //NOTE: could copy as ints instead? much faster..
		//-----------------------------------------------------------------
		for(int i=0; i<numbytestoread; i++)
		{
			datawritepos[i] = buffer[i];
		}

		//-----------------------------------------------------------------
		//now set the various write indices 
		//-----------------------------------------------------------------
		bitindex += num_bits_to_read;

		bitoffset = num_bits_to_read % 8;
			//as bitoffset was 0 before this function was called

		byteindex += numbytestoread;//NOTE: check me

		//make sure numbits is consistent
		if(bitindex > numbits)
			numbits = bitindex;

		return numbytestoread;
	}







	//-----------------------------------------------------------------
	//make sure the data array is big enough
	//-----------------------------------------------------------------
	checkResize(bitindex + num_bits_to_read);





	
	//NOTE: this can be speeded up by just copying the bits across
	//-----------------------------------------------------------------
	//write the bits  NOTE: this can be speeded up by avoiding the modulo
	//-----------------------------------------------------------------
	for(int i=0; i<num_bits_to_read; i++)
	{
		int bitoffset = i % 8;//offset in byte


		writeBitUnchecked(getBitFromByte( buffer[i / 8], bitoffset));
	}

	//-----------------------------------------------------------------
	//calc the number of bytes read
	//-----------------------------------------------------------------
	int numbytesread = getNumBytesToHold(num_bits_to_read);


	return numbytesread;	
}








void BitArray::resize(int newnumbits)
{
	int newnumbytes = getNumBytesToHold(newnumbits);


//	debugPrint("resizing bitarray to %i bytes...\n", newnumbytes);

	//-----------------------------------------------------------------
	//allocate storage for new data
	//-----------------------------------------------------------------
	BYTE* newdata = new BYTE[newnumbytes];

	//-----------------------------------------------------------------
	//copy old data across
	//-----------------------------------------------------------------
	int mincommonbytes = myMin(newnumbytes, getNumBytes());

	for(int i=0; i<mincommonbytes; i++)
	{
		newdata[i] = data[i];
	}

	//-----------------------------------------------------------------
	//delete old data and replace with new data
	//-----------------------------------------------------------------
	delete[] data;
	data = newdata;

	datasize = newnumbytes;
	//numbits = newnumbits; numbits shouldn't be changed
}

void BitArray::resizeDataArray(int newdatasize)
{
//	debugPrint("resizing bitarray to %i bytes...\n", newdatasize);
	
	//-----------------------------------------------------------------
	//allocate storage for new data
	//-----------------------------------------------------------------
	BYTE* newdata = new BYTE[newdatasize];

	//-----------------------------------------------------------------
	//copy old data across
	//-----------------------------------------------------------------
	int mincommonbytes = myMin(newdatasize, getNumBytes());

	for(int i=0; i<mincommonbytes; i++)
	{
		newdata[i] = data[i];
	}

	//-----------------------------------------------------------------
	//delete old data and replace with new data
	//-----------------------------------------------------------------
	delete[] data;
	data = newdata;

	datasize = newdatasize;
}










bool BitArray::operator == (const BitArray& rhs) const
{
	if(rhs.getNumBits() != this->getNumBits())
		return false;

	for(int i=0; i<this->getNumBits(); i++)
	{
		if(this->getBit(i) != rhs.getBit(i))
			return false;
	}

	return true;
}

bool BitArray::operator != (const BitArray& rhs) const
{
	if(rhs.getNumBits() != this->getNumBits())
		return true;

	for(int i=0; i<this->getNumBits(); i++)
	{
		if(this->getBit(i) != rhs.getBit(i))
			return true;
	}

	return false;
}



bool BitArray::doesContain(const const_iterator& iter, const BitArray& bitstring) const
{
	const_iterator movingiter(iter);

	for(int i=0; i<bitstring.getNumBits(); i++)
	{
		//-----------------------------------------------------------------
		//if the iterator has moved past the end of this bit array
		//-----------------------------------------------------------------
		if( !(movingiter < this->end()) )
			return false;

		if( (*movingiter) != bitstring.getBit(i) )
			return false;

		movingiter++;
	}
	
	return true;
}


BitArray& BitArray::operator = (const BitArray& rhs)
{
	/*BYTE* data;
	int datasize;//num bytes pointed to by data

	//int numbytesused;//num bytes of data used
	//int maxnumbytes;
	int numbits;//number of bits in the array.

	int bitindex;//== number of bits written by writeBit() since startWriting() was last called
	int bitoffset;
	int byteindex;*/

	if(data)
		delete[] data;


	datasize = rhs.datasize;
	data = new BYTE[rhs.datasize];

	//-----------------------------------------------------------------
	//copy data  NOTE: this could be speeded up
	//-----------------------------------------------------------------
	for(int i=0; i<datasize; i++)
		data[i] = rhs.data[i];

	numbits = rhs.numbits;
	bitindex = rhs.bitindex;
	bitoffset = rhs.bitoffset;
	byteindex = rhs.byteindex;

	return *this;
}



BitArray::BitArray(const BitArray& rhs)
{	
	datasize = rhs.datasize;
	data = new BYTE[rhs.datasize];

	//-----------------------------------------------------------------
	//copy data  NOTE: this could be speeded up
	//-----------------------------------------------------------------
	for(int i=0; i<datasize; i++)
		data[i] = rhs.data[i];

	numbits = rhs.numbits;
	bitindex = rhs.bitindex;
	bitoffset = rhs.bitoffset;
	byteindex = rhs.byteindex;

}













