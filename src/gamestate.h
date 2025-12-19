#pragma once

#include "list.h"
#include "matrix.h"
#include "box.h"
#include "cloneable.h"

struct player {
	int64_t pos[3];
	int64_t vel[3];
	int32_t inputs[3];
	box *prox;
	int tmp;
};

struct solid : cloneable {
	int64_t pos[3];
	int64_t oldPos[3];
	int64_t vel[3];
	int64_t r;
	int32_t tex;
	iquat rot;
	iquat oldRot; // This isn't serialized, and we don't care if it's copied. Should avoid using this until this solid has ticked!
	box *b;
};

struct gamestate {
	list<player> players;
	list<solid*> solids;
	box *vb_root;
};

extern void resetPlayer(gamestate *gs, int i);
extern void setupPlayers(gamestate *gs, int numPlayers);

extern solid* addSolid(gamestate *gs, box *b, int64_t x, int64_t y, int64_t z, int64_t r, int32_t tex);

extern void runTick(gamestate *gs);

extern void mkSolidAtPlayer(gamestate *gs, player *p, iquat r);

extern gamestate* dup(gamestate *orig);
extern void prepareGamestateForLoad(gamestate *gs, char isSync);
extern void init(gamestate *gs);
extern void cleanup(gamestate *gs);

extern void write32(list<char> *data, int32_t v);
extern void serialize(gamestate *gs, list<char> *data);
extern void deserialize(gamestate *gs, list<char> *data, char fullState);

extern void gamestate_init();
extern void gamestate_destroy();
