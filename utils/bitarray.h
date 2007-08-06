#ifndef __BITARRAY_H__
#define __BITARRAY_H__

//#include "huffmantypes.h"

#include <assert.h>
#include <string>
#include <vector>


class BitArrayConstIterator;

//NOTE: could REPRAZENT as vector of bools

//NOTE: add an writeBit that doesn't do a checkResize()

//NOTE: fix this class so bit 0 is most significant bit

//* need some kind of capcity reserver for streaming operations.


class BitArray
{
public:
		typedef unsigned char BYTE;

	/*=========================================================================
	BitArray
	--------
	construct a new empty bitarray.
	postcondition:
		getNumBits() == 0	
	=========================================================================*/
	BitArray();

	/*=========================================================================
	BitArray
	--------
	construct a new bitarray with 'numbits' bits.
	postcondition:
		getNumBits() == numbits	
	=========================================================================*/
	BitArray(int numbits);
	~BitArray();

	BitArray(const BitArray& rhs);
	BitArray& operator = (const BitArray& rhs);


	/*=========================================================================
	setBit
	------
	set the bit at 'index' to 1 if newvalue==true, or 0 if newvalue==false

	Behaviour is undefined if 'index' >= getNumBits()
	=========================================================================*/
	inline void setBit(int index, bool newvalue);
	/*=========================================================================
	getBit
	------
	return the boolean value of the bit at 'index'

	Behaviour is undefined if 'index' >= getNumBits()
	=========================================================================*/
	inline bool getBit(int index) const;

	/*=========================================================================
	printBits
	---------
	prints the bits 'bitstring_out' as a sequence like 1110001100010
	=========================================================================*/
	void printBits(std::string& bitstring_out) const;

	/*=========================================================================
	printBit
	--------
	turn the bit into a char
	=========================================================================*/
	char printBit(int index) const;

	/*=========================================================================
	getNumBits
	----------
	Return the number of bits in the bit array
	=========================================================================*/
	int getNumBits() const { return numbits; }

	/*=========================================================================
	getNumBytes
	-----------
	Returns the number of (total and partial)bytes needed to write the bit array.
	Should be equal to or just above getNumBits() / 8
	== ceil(getNumBits() / 8)
	=========================================================================*/
	inline int getNumBytes() const;

	/*=========================================================================
	writeToBuffer
	-------------
	writes the bitarray to 'buffertowriteto', returns an integer which is the
	number of (total and partial) BYTES written.
	
	the return value should equal getNumBytes().
	=========================================================================*/
	int	writeToBuffer(BYTE* buffertowriteto) const;

	/*=========================================================================
	operator ==
	-----------
	returns true if getNumBits() == rhs.getNumBits() 
		and all bits in this are equal to all bits in rhs.
	=========================================================================*/
	bool operator == (const BitArray& rhs) const;

	bool operator != (const BitArray& rhs) const;


	void pushBack(bool bitvalue); 
	int pushBackFromBuffer(const BYTE* buffer, int num_bits_to_read);

	/*=========================================================================
	--------Streaming operations----------
	=========================================================================*/



	/*=========================================================================
	set the write index to 0
	=========================================================================*/
	//void startWriting();

	/*=========================================================================
	writeBit
	--------
	write to the bit at writeindex, increments writeindex.
	Also the data array is automagically resized so you will never write past
	the end of the array.

	postcondition: getNumBits() will be at least as large as the write index

	=========================================================================*/
	//inline void writeBit(bool bitvalue);




	//inline void writeBitUnchecked(bool bitvalue);

	/*=========================================================================
	writeBits
	---------
	writeBit()'s the bits from the BitArray 'bitsource' to this bitarray.

	The write index will be increased by bitsource.getNumBits()
	=========================================================================*/
	//void writeBits(const BitArray& bitsource);

	/*=========================================================================
	writeBits
	---------
	writeBit()'s bits from the BitArray 'bitsource', until either 'num_bits_to_copy_from_source'
	bits have been written, or we run out of bits to copy from 'bitsource'.

	The write index will be increased by min(bitsource.getNumBits(), num_bits_to_copy_from_source)
	=========================================================================*/
	//void writeBits(const BitArray& bitsource, int num_bits_to_copy_from_source);



	//int writeBitsFromBuffer(const BYTE* buffer, int num_bits_to_read);

	/*=========================================================================
	checkResize
	-----------
	postcondition:  you will be able to call setBit(index, some_boolean_val)
	without writing past the end of the internal data representation.

	Does nothing if bitindex < getNumBits()
	=========================================================================*/
	//inline void checkResize(int bitindex);

	/*=========================================================================
	postcondition:  you will be able to call setBit(index, some_boolean_val)
	without writing past the end of the internal data representation.

	Note that getNumBits() will not be changed.

	It is possible to resize to a smaller size.
	=========================================================================*/
	void resize(int newnumbits);




	typedef BitArrayConstIterator const_iterator;

	inline const_iterator begin() const;
	inline const_iterator end() const;

	bool doesContain(const const_iterator& iter, const BitArray& bitstring) const;

	inline int getNumBytesToHold(int numbits) const;


private:

	//-----------------------------------------------------------------
	//
	//-----------------------------------------------------------------
	template<class T>
	inline const T& myMin(const T& x, const T& y)
	{
		if(y < x)
			return y;

		return x;
	}

	template<class T>
	inline const T& myMax(const T& x, const T& y)
	{
		if(x < y)
			return y;

		return x;
	}

	//-----------------------------------------------------------------
	//
	//-----------------------------------------------------------------

	/*inline void incrementWriteIndices()
	{	
		bitindex++;

		bitoffset++;
		if(bitoffset == 8)
		{
			byteindex++;
			bitoffset = 0;
		}
	}*/

	inline void setBit(int byteindex, int bitoffset, bool newvalue);



	inline bool getBitFromByte(BYTE thebyte, int bitoffset) const
	{
		assert(bitoffset >= 0 && bitoffset < 8);

		//NOTE: change this so tha bit 0 is at the left and bit 7 is at the right?
		//-----------------------------------------------------------------
		//get a byte with all zeros apart from at bit 'offset'
		//-----------------------------------------------------------------
		/*BYTE bitmask = 1;
		//for(int i=0; i<bitoffset; i++)
		//	bitmask <<= 1;	//move the 1 bit 1 place to the left

		bitmask <<= bitoffset;

		return (bool)(thebyte & bitmask);*/
		return (bool)(thebyte & (1 << bitoffset));
	}


	std::vector<BYTE> data;

	int numbits;//number of bits in the array.

	//int bitindex;//== number of bits written by writeBit() since startWriting() was last called
	//int bitoffset;
	//int byteindex;
};


class BitArrayConstIterator
{
public:
	BitArrayConstIterator(const BitArray* bitarray_, int bitindex_)
		:	bitarray(bitarray_), bitindex(bitindex_)
	{}

	BitArrayConstIterator(const BitArrayConstIterator& rhs)
	{
		bitarray = rhs.bitarray;
		bitindex = rhs.bitindex;
	}

	bool operator < (const BitArrayConstIterator& rhs) const
	{
		return bitindex < rhs.bitindex;
	}

	bool operator * () const
	{
		return bitarray->getBit(bitindex);
	}

	BitArrayConstIterator& operator += (int amount)
	{
		bitindex += amount;
		return *this;
	}
	BitArrayConstIterator& operator ++ (int)
	{
		bitindex ++;
		return *this;
	}

private:
	const BitArray* bitarray;
	int bitindex;
};




















BitArray::const_iterator BitArray::begin() const
{
	return BitArrayConstIterator(this, 0);
}

BitArray::const_iterator BitArray::end() const
{
	return BitArrayConstIterator(this, getNumBits());
}



void BitArray::setBit(int index, bool newvalue)
{
	assert(index >= 0 && index < getNumBits());

	//-----------------------------------------------------------------
	//get the byte index
	//-----------------------------------------------------------------
	//int bytei = index / 8;
	/*const int bytei = index >> 3;

	//assert(chari < datasize);
				
	//-----------------------------------------------------------------
	//get the bit index in the byte
	//-----------------------------------------------------------------
	//int offset = index % 8;
	//NOTE: could avoid this modulo here
	const int offset = index & 7;	//7 = ...0000000111


	setBit(bytei, offset, newvalue);*/

	//tersly written as this:
	setBit(index >> 3, index & 7, newvalue);
}

void BitArray::setBit(int byteindex, int bitoffset, bool newvalue)
{
	//NOTE: this assert needed?
	assert(byteindex >= 0 && byteindex < data.size());//TEMP!
	assert(bitoffset >= 0 && bitoffset < 8);


	if(newvalue)
	{
		BYTE bitmask = 1;

		bitmask <<= bitoffset;

		data[byteindex] |= bitmask;//set the bit to 1
	}
	else
	{
		//-----------------------------------------------------------------
		//so must set the bit to 0
		//-----------------------------------------------------------------
		const BYTE all1s = 255;

		BYTE bitsetatoffset = 1;

		bitsetatoffset <<= bitoffset;


		BYTE bitmask = all1s - bitsetatoffset;

		//-----------------------------------------------------------------
		//now bitmask will be all 1s apart from at bit 'offset'
		//-----------------------------------------------------------------
		data[byteindex] &= bitmask;
	}
}
	




bool BitArray::getBit(int index) const
{
	assert(index >= 0 && index < getNumBits());


	//-----------------------------------------------------------------
	//get the byte index
	//-----------------------------------------------------------------
	//int bytei = index / 8;
	const int bytei = index >> 3;

	//assert(chari < datasize);
				
	//-----------------------------------------------------------------
	//get the bit index in the byte
	//-----------------------------------------------------------------
	//int offset = index % 8;
	const int offset = index & 7;	//7 = ...0000000111

	return getBitFromByte(data[bytei], offset);
}


/*void BitArray::writeBit(bool bitvalue)
{
	//-----------------------------------------------------------------
	//make the data array bigger if neccesary to accomodate this write
	//-----------------------------------------------------------------
	checkResize(bitindex);

	setBit(byteindex, bitoffset, bitvalue);

	incrementWriteIndices();

	if(bitindex > numbits)
		numbits = bitindex;
}

void BitArray::writeBitUnchecked(bool bitvalue)
{
	setBit(byteindex, bitoffset, bitvalue);

	incrementWriteIndices();

	if(bitindex > numbits)
		numbits = bitindex;
}*/

/*void BitArray::checkResize(int index)
{
	const int numbits_needed = index + 1;
	//-----------------------------------------------------------------
	//get the byte index for the bit index
	//-----------------------------------------------------------------
	int bytenum = getNumBytesToHold(numbits_needed);

	if(bytenum < datasize)
		return;

	int new_numbits = myMax(numbits_needed + 1, (numbits*2) + 128);

	resize(new_numbits);
}*/

//NOTE: speed me up if possible
int BitArray::getNumBytesToHold(int numbits) const
{
	int numbytesneeded;

	if(numbits & 0x000007 == 0)//numbits % 8 == 0)
	{
		numbytesneeded = numbits >> 3;
					// = numbits / 8
	}
	else
	{
		numbytesneeded = (numbits >> 3) + 1;
	}

	return numbytesneeded;
}

int BitArray::getNumBytes() const
{
	return getNumBytesToHold(numbits);
}









#endif //__BITARRAY_H__