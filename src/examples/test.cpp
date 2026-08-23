#include <stdio.h>

#include "../gamestate.h"
#include "../dl_helpers.h"

extern "C" void lvlUpd(gamestate *gs) {
	printf("Hello world at (%ld, %ld)\n", var("x"), var("y", 10));
}
