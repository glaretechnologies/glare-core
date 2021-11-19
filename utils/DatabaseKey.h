/*=====================================================================
DatabaseKey.h
-------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <limits>


struct DatabaseKey
{
	DatabaseKey() : val(std::numeric_limits<uint64>::max()) {}
	DatabaseKey(uint64 val_) : val(val_) {}

	bool operator < (const DatabaseKey& other) const { return val < other.val; }
	bool operator == (const DatabaseKey& other) const { return val == other.val; }

	uint64 value() const { return val; }

	static DatabaseKey invalidkey() { return DatabaseKey(std::numeric_limits<uint64>::max()); }

	bool valid() const { return val != invalidkey().value(); }

	uint64 val;
};
