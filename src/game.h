#pragma once

#include <stdint.h>

#include "list.h"
#include "quaternion.h"

#define WINDOW_TITLE "Retoss"
#define MAGIC_FIRST_BYTE 0x90

struct player {
	char unused;
};

struct gamestate {
	int32_t nonsense;
	list<player> players;
};

extern quat tmpGameRotation;
