#pragma once

#include "util.h"
#include "cloneable.h"
#include "list.h"

// "gamestate.h" and "box.h" require each other;
// you should be including "gamestate.h" instead
// of directly including this file.


// The idea behind velbox/box.h is that
// you can override some of the defaults
// (like number of dimensions, or bitwidth of positions)
// without modifying the file.
// I wound up re-writing it for this project anyway,
// so the "defaults" are all already what I want.
#define LEAF mover
#include "velbox/box.h"
