/*=====================================================================
OpenGLTextureKey.h
------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include <utils/STLArenaAllocator.h>
#include <string>


typedef std::basic_string<char, std::char_traits<char>, glare::STLArenaAllocator<char>> OpenGLTextureKey;


struct OpenGLTextureKeyHasher
{
	size_t operator () (const OpenGLTextureKey& s) const
	{
		std::hash<std::string_view> h;
		return h(s);
	}
};
