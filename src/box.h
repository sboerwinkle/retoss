#pragma once

#include "util.h"
#include "cloneable.h"
#include "list.h"

#include "gamestate_box.h"


// We only do one #define here.
#define VELBOX_DATA_LIST dummyVelboxSerizList
//
// The idea behind velbox/box.h is that
// you can override some of the defaults
// (like number of dimensions, or bitwidth of positions)
// without modifying the file.
// I wound up re-writing it for this project anyway,
// so the "defaults" are all already what I want.
#include "velbox/box.h"
