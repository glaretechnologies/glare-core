/*=====================================================================
TextEditingUtils.cpp
--------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "TextEditingUtils.h"


#include "../../utils/UTF8Utils.h"
#include "../../utils/StringUtils.h"
#include <cassert>


size_t TextEditingUtils::getCursorByteIndex(const std::string& text, int cursor_pos)
{
	if(cursor_pos < 0)
		return 0;
	else if(cursor_pos >= (int)UTF8Utils::numCodePointsInString(text)) // If cursor is at end of text:
	{
		return text.size();
	}
	else
	{
		return UTF8Utils::byteIndex((const uint8*)text.data(), text.size(), (size_t)cursor_pos);
	}
}


// aaaaaaaaaa bbbbbbbb cccccccc
//   ^        ^
// start     new
int TextEditingUtils::getNextStartOfNonWhitespaceCursorPos(const std::string& text, int cursor_pos)
{
	size_t cur_byte_index = getCursorByteIndex(text, cursor_pos);
	int cur_cursor_pos = cursor_pos;

	// Advance until we get to whitespace
	while(cur_byte_index < text.size() && !isWhitespace(text[cur_byte_index]))
	{
		cur_byte_index += UTF8Utils::numBytesForChar(text[cur_byte_index]);
		cur_cursor_pos++;
	}

	// Advance past whitespace
	while(cur_byte_index < text.size() && isWhitespace(text[cur_byte_index]))
	{
		cur_byte_index++;
		cur_cursor_pos++;
	}

	return cur_cursor_pos;
}


static inline bool isByteStartOfCharacterSeq(const std::string& text, int byte_index)
{
	assert(byte_index >= 0 && byte_index < (int)text.size());

	// "Every byte that does not start 10xxxxxx is the start of a UCS character sequence." - http://www.cl.cam.ac.uk/~mgk25/ucs/utf-8-history.txt
	const uint8* data = (const uint8*)text.data();
	return (data[byte_index] & 0xC0) != 0x80; // If leftmost two bits are != 10, then it's the start of a character sequence.
}


// aaaaaaaaaa bbbbbbbb   cccccccc
//            ^          ^ 
//           new         start
int TextEditingUtils::getPrevStartOfNonWhitespaceCursorPos(const std::string& text, int cursor_pos)
{
	int prev_cursor_pos = cursor_pos - 1;
	if(prev_cursor_pos < 0)
		return 0;

	int prev_byte_index = (int)getCursorByteIndex(text, prev_cursor_pos);

	// Advance left while prev is whitespace.
	while(prev_byte_index >= 0 && prev_byte_index < text.size() && isWhitespace(text[prev_byte_index]))
	{
		prev_byte_index--;

		// Decrement prev_byte_index until we get to the start byte of a character (high bit zero)
		while(prev_byte_index >= 0 && prev_byte_index < (int)text.size() && !isByteStartOfCharacterSeq(text, prev_byte_index) != 0)
			prev_byte_index--;

		prev_cursor_pos--;
	}

	// Advance left while prev is not whitespace
	while(prev_byte_index >= 0 && prev_byte_index < text.size() && !isWhitespace(text[prev_byte_index]))
	{
		prev_byte_index--;

		// Decrement prev_byte_index until we get to the start byte of a character (high bit zero)
		while(prev_byte_index >= 0 && prev_byte_index < (int)text.size() && !isByteStartOfCharacterSeq(text, prev_byte_index) != 0)
			prev_byte_index--;

		prev_cursor_pos--;
	}

	return prev_cursor_pos + 1;
}




#if BUILD_TESTS


#include "../../utils/TestUtils.h"


void TextEditingUtils::test()
{
	const std::string gamma = "\xCE\x93"; // Greek capital letter gamma, U+393, http://unicodelookup.com/#gamma/1, 

	//--------------------------------- getCursorByteIndex ---------------------------------
	testEqual((int)getCursorByteIndex("abcd", -1), 0);
	testEqual((int)getCursorByteIndex("abcd", 0), 0);
	testEqual((int)getCursorByteIndex("abcd", 1), 1);
	testEqual((int)getCursorByteIndex("abcd", 2), 2);
	testEqual((int)getCursorByteIndex("abcd", 3), 3);
	testEqual((int)getCursorByteIndex("abcd", 4), 4);
	testEqual((int)getCursorByteIndex("abcd", 5), 4);

	testEqual((int)getCursorByteIndex(gamma + gamma + gamma + gamma, -1), 0);
	testEqual((int)getCursorByteIndex(gamma + gamma + gamma + gamma, 0), 0);
	testEqual((int)getCursorByteIndex(gamma + gamma + gamma + gamma, 1), 2);
	testEqual((int)getCursorByteIndex(gamma + gamma + gamma + gamma, 2), 4);
	testEqual((int)getCursorByteIndex(gamma + gamma + gamma + gamma, 3), 6);
	testEqual((int)getCursorByteIndex(gamma + gamma + gamma + gamma, 4), 8);
	testEqual((int)getCursorByteIndex(gamma + gamma + gamma + gamma, 5), 8);

	//--------------------------------- getNextStartOfNonWhitespaceCursorPos ---------------------------------
	testEqual(getNextStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/0), 3);
	testEqual(getNextStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/1), 3);
	testEqual(getNextStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/2), 3);
	testEqual(getNextStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/3), 6);
	testEqual(getNextStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/4), 6);
	testEqual(getNextStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/5), 6);
	testEqual(getNextStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/6), 8);
	testEqual(getNextStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/7), 8);
	testEqual(getNextStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/8), 8);

	// Test with some multi-byte characters
	const std::string test_gamma_str = gamma + gamma + " " + gamma + gamma + " " + gamma + gamma;
	testEqual(getNextStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/0), 3);
	testEqual(getNextStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/1), 3);
	testEqual(getNextStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/2), 3);
	testEqual(getNextStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/3), 6);
	testEqual(getNextStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/4), 6);
	testEqual(getNextStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/5), 6);
	testEqual(getNextStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/6), 8);
	testEqual(getNextStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/7), 8);
	testEqual(getNextStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/8), 8);

	//--------------------------------- getPrevStartOfNonWhitespaceCursorPos ---------------------------------
	testEqual(getPrevStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/0), 0);
	testEqual(getPrevStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/1), 0);
	testEqual(getPrevStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/2), 0);
	testEqual(getPrevStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/3), 0);
	testEqual(getPrevStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/4), 3);
	testEqual(getPrevStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/5), 3);
	testEqual(getPrevStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/6), 3);
	testEqual(getPrevStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/7), 6);
	testEqual(getPrevStartOfNonWhitespaceCursorPos("aa bb cc", /*cursor pos=*/8), 6);

	// Test with some multi-byte characters
	testEqual(getPrevStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/0), 0);
	testEqual(getPrevStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/1), 0);
	testEqual(getPrevStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/2), 0);
	testEqual(getPrevStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/3), 0);
	testEqual(getPrevStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/4), 3);
	testEqual(getPrevStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/5), 3);
	testEqual(getPrevStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/6), 3);
	testEqual(getPrevStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/7), 6);
	testEqual(getPrevStartOfNonWhitespaceCursorPos(test_gamma_str, /*cursor pos=*/8), 6);

}


#endif
