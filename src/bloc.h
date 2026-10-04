#pragma once

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Originally this was built to accomodate `queue`, which expects to be able to
// init/destroy things.
// But then we no longer needed that, so it got reworked to just be an array (basically).
// I think this is actually necessary (see `offset_t` in matrix.h),
// but the full truth is I'm too lazy to figure out if it's necessary.
template <typename T, size_t S> class bloc {
public:
	T items[S];
};
