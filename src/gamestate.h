#pragma once

#include "list.h"
#include "quaternion.h"
#include "box.h"
#include "cloneable.h"

struct player {
	int64_t pos[3];
	box *prox;
	int tmp;
};

struct solid : cloneable {
	int64_t pos[3];
	int64_t r;
	int32_t tex;
	iquat rot;
	box *b;
};

struct gamestate {
	list<player> players;
	list<solid*> solids;
	box *vb_root;
};

extern void resetPlayer(gamestate *gs, int i);
extern void setupPlayers(gamestate *gs, int numPlayers);

extern void runTick(gamestate *gs);
extern gamestate* dup(gamestate *orig);
extern void prepareGamestateForLoad(gamestate *gs, char isSync);
extern void init(gamestate *gs);
extern void cleanup(gamestate *gs);

extern void write32(list<char> *data, int32_t v);
extern void serialize(gamestate *gs, list<char> *data);
extern void deserialize(gamestate *gs, list<char> *data, char fullState);

extern void gamestate_init();
extern void gamestate_destroy();
