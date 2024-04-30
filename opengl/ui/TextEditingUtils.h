/*=====================================================================
TextEditingUtils.h
------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include <string>


/*=====================================================================
TextEditingUtils
----------------
Utils for editing UTF-8 encoded Unicode text.
=====================================================================*/
class TextEditingUtils
{
public:
	static size_t getCursorByteIndex(const std::string& text, int cursor_pos);

	static int getNextStartOfNonWhitespaceCursorPos(const std::string& text, int cursor_pos);

	static int getPrevStartOfNonWhitespaceCursorPos(const std::string& text, int cursor_pos);

	static void test();
};
