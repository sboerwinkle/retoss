#pragma once

#include "util.h"
#include "cloneable.h"
#include "list.h"


// The idea behind velbox/box.h is that
// you can override some of the defaults
// (like number of dimensions, or bitwidth of positions)
// without modifying the file.
// I wound up re-writing it for this project anyway,
// so the "defaults" are all already what I want.
#include "velbox/box.h"
