#pragma once
#include <stdint.h>

struct cloneable {
	union {
		int32_t idx;
		cloneable *ptr;
	} clone;
};
