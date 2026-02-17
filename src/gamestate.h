#pragma once

#include <stddef.h>

#include "list.h"
#include "matrix.h"
struct mover; // "box" and "gamestate" reference each other's types
#include "box.h"
#include "cloneable.h"

#define PLAYER_SHAPE_RADIUS 800
#define NUM_TEXS 7

extern int32_t gs_gravity;

struct mover { // This is kind of just a grouping of fields; we use it for e.g. rendering
	int64_t pos[3];
	int64_t oldPos[3];
	iquat rot, oldRot;
	int type;
};

struct player {
	mover m;
	int64_t vel[3];
	int32_t inputs[3];
	char jump;
	box *prox;
};

#define NUM_SHAPES 3
#define T_PLAYER 32

#define solidFromMover(x) ((solid*)((char*)(x) - offsetof(solid, m)))
struct solid : cloneable {
	mover m;
	int64_t vel[3];
	int64_t r;
	int32_t tex;
	box *b;
};

// Todo Could make this copy-on-write (since we never write it) to save a little effort on gamestate duplication
struct trail {
	offset origin;
	unitvec dir;
	int64_t len;
	int32_t expiry;
};

struct gamestate {
	list<player> players;
	list<solid*> solids;
	list<solid*> selection;
	list<trail> trails;
	box *vb_root;
};

extern void resetPlayer(gamestate *gs, int i);
extern void setupPlayers(gamestate *gs, int numPlayers);

extern solid* addSolid(gamestate *gs, box *b, int64_t x, int64_t y, int64_t z, int64_t r, int32_t shape, int32_t tex);
extern void rmSolid(gamestate *gs, solid *s);

extern void runTick(gamestate *gs);

extern void mkSolidAtPlayer(gamestate *gs, player *p);

extern gamestate* dup(gamestate *orig);
extern void prepareGamestateForLoad(gamestate *gs, char isSync);
extern void init(gamestate *gs);
extern void cleanup(gamestate *gs);

extern void write32(list<char> *data, int32_t v);
extern void serialize(gamestate *gs, list<char> *data);
extern void deserialize(gamestate *gs, list<char> *data, char fullState);

extern void gamestate_init();
extern void gamestate_destroy();
