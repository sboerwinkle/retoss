#pragma once

#include <stddef.h>

#include "util.h"
#include "list.h"
#include "matrix.h"
struct mover; // "box" and "gamestate" reference each other's types
#include "box.h"
#include "task.h"

#define PLAYER_SHAPE_RADIUS 800
#define TRAIL_LIFETIME 45
#define NUM_TEXS 13
#define TEX_SNAKE 12
#define TEX_TEAM_SHIRT 10

#define NUM_SHAPES 3
#define T_PLAYER 32

extern int32_t gs_gravity;
extern double const shapeDiagonalMultipliers[NUM_SHAPES];

struct mover { // This is kind of just a grouping of fields; we use it for e.g. rendering
	int64_t pos[3];
	int64_t oldPos[3];
	iquat rot, oldRot;
	int type;
};

#define DYNTEX_BUF_LEN 8
struct dyntex_description {
	int32_t baseTex;
	char str[DYNTEX_BUF_LEN];
};

struct dyntex_holder {
	uint32_t tex; // graphics thread reads/writes this
	dyntex_description descr;
	int refs;

	void decr();
};

#define playerFromMover(x) ((player*)((char*)(x) - offsetof(player, m)))
struct player {
	mover m;
	int64_t vel[3];
	int32_t inputs[3];
	char jump, shoot, alive;
	char team;
	int32_t cooldown;
	u8 hits, hitsCooldown;
	box *prox;
	dyntex_holder *skin;
};

#define solidFromMover(x) ((solid*)((char*)(x) - offsetof(solid, m)))
struct solid {
	mover m;
	//int64_t vel[3];
	int64_t r;
	int32_t tex;
	box *b;

	// Currently this is only being used for `gs->selection`.
	clone_t clone;
};

// Constellations.
// A set of solids, fixed relative to each other, that may appear multiple times (like a blueprint).
// Eventually the idea is that each instance won't always be "explicit" in the world,
// and can be "implicit" if nothing's colliding with it.

struct constelPt {
	offset o;
	iquat rot;
	int64_t r;
	int32_t type;
	int32_t tex;
};

// `constel`s are COW (copy-on-write), to reduce gamestate copy overhead.
struct constel {
	list<constelPt> points;
	int64_t r;
	int refCount;
	int serizIx;

	void incr();
	void decr();
	// Not super smart, will likely be larger than necessary.
	// Still, quick-and-easy if you don't have a custom `r`
	// to populate.
	void estimateRadius();
};

struct constelInst {
	constel *c;
	mover m;
	// box *prox; // Will need this once it's ever implicit.
	int32_t duration;
	list<solid> solids;
	clone_t clone;
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
	list<constelInst*> constels;
	list<solid*> solids;
	list<solid*> selection;
	list<trail> trails;
	taskInstance tasks;
	box *vb_root;
	int32_t clock;
	uint32_t seed;
};

extern void resetPlayer(gamestate *gs, int i);
extern void softResetPlayer(player *p);
extern void setupPlayers(gamestate *gs, int numPlayers);
extern void killPlayer(player *p);

extern void validateSize(int64_t *_size);
extern void validateShape(int32_t *_shape);
extern void validateTex(int32_t *_tex);

extern void solidPutVb(solid *s, box *guess, int duration);
extern void cpSolid(solid *t, solid *s);
extern solid* addSolid(gamestate *gs, box *b, int64_t x, int64_t y, int64_t z, int64_t r, int32_t shape, int32_t tex);
extern void rmSolid(gamestate *gs, solid *s);

extern constelInst* mkConstelInst(constel *c, int32_t duration);
extern void addConstelInst(gamestate *gs, constelInst *ci);
extern void deleteConstelInst(constelInst *ci);

extern void addTask(gamestate *gs, int taskId, void *data);

extern void runTick(gamestate *gs);

extern void mkSolidAtPlayer(gamestate *gs, player *p);

extern gamestate* dup(gamestate *orig);
extern void prepareGamestateForLoad(gamestate *gs, char isSync);
extern void init(gamestate *gs);
extern void cleanup(gamestate *gs);

extern void write32(list<char> *data, int32_t v);
extern void transSolid(solid *s);
extern void serialize(gamestate *gs, list<char> *data);
extern void deserialize(gamestate *gs, list<char> *data, char fullState);

extern void gamestate_init();
extern void gamestate_destroy();
