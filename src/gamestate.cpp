#include "util.h"
#include "cloneable.h"

#include "random.h"
#include "serialize.h"

#include "gamestate.h"

#include "collision.h"
#include "player.h"

static list<void*> queryResults;

void resetPlayer(gamestate *gs, int i) {
	gs->players[i] = {
		.m={
			.pos={0,0,0},
			.oldPos={-1,-1,-1},
			.rot={FIXP,0,0,0},
		},
		.vel={0,0,0},
		.inputs={0,0,0},
		.prox=gs->vb_root,
	};
}

void setupPlayers(gamestate *gs, int numPlayers) {
	gs->players.setMaxUp(numPlayers);
	gs->players.num = numPlayers;
	range(i, gs->players.num) resetPlayer(gs, i);
}

// cube diagonal is sqrt(3), or approx 1.75 (=7/4)
// slab diagonal is sqrt(2), even with a bit of thickenss it's comfortably within 1.5 (=3/2)
// pole diagonal is like really close to 1 once you do the math
static double const shapeDiagonalMultipliers[NUM_SHAPES] = {7.0f/4, 3.0f/2, 33.0f/32};

static void solidValidate(solid *s) {
	if (s->shape < 0 || s->shape >= NUM_SHAPES) {
		printf("Invalid shape %d!!\n", s->shape);
		s->shape = 0;
	}
}

static void solidPutVb(solid *s, box *guess) {
	box *tmp = velbox_alloc();
	s->b = tmp;
	memcpy(tmp->pos, s->m.pos, sizeof(s->m.pos));
	tmp->vel[0] = 0;
	tmp->vel[1] = 0;
	tmp->vel[2] = 0;
	tmp->r = s->r*shapeDiagonalMultipliers[s->shape];
	tmp->end = tmp->start + 15; // 1 second
	tmp->data = s;
	velbox_insert(guess, tmp);

}

static void solidUpdate(gamestate *gs, solid *s) {
	memcpy(s->m.oldPos, s->m.pos, sizeof(s->m.pos));
	// For stuff that doesn't spin, we could maybe just set this up after de-seriz and cloning.
	memcpy(s->oldRot, s->m.rot, sizeof(s->m.rot));

	// The only other thing we're doing is updating our velbox,
	// so if it's still valid for long enough we leave it alone.
	if (s->b->end > gs->vb_root->end+1) return;
	box *old = s->b;
	box *p = old->parent;
	// TODO If this keeps up, then we're actually not expecting anything to naturally expire.
	//      Not sure if this will keep up, though.
	velbox_remove(old);
	solidPutVb(s, p);
}

static solid* solidDup(solid *s) {
	solid *t = new solid();
	*t = *s;
	// Not stictly necessary - dup'd states are never the canon state,
	// but the idea is that we don't want to rely on oldPos until the
	// thing in question has ticked this frame.
	t->m.oldPos[0] = t->m.oldPos[1] = t->m.oldPos[2] = -1;

	s->clone.ptr = t;
	t->b = (box*)(s->b->clone.ptr);
	t->b->data = t;

	return t;
}

solid* addSolid(gamestate *gs, box *b, int64_t x, int64_t y, int64_t z, int64_t r, int32_t shape, int32_t tex) {
	solid *s = new solid();
	gs->solids.add(s);
	s->m.pos[0] = x;
	s->m.pos[1] = y;
	s->m.pos[2] = z;
	s->m.oldPos[0] = s->m.oldPos[1] = s->m.oldPos[2] = -1;
	s->vel[0] = s->vel[1] = s->vel[2] = 0;
	s->r = r;
	s->shape = shape;
	s->tex = tex;
	s->m.rot[0] = FIXP;
	s->m.rot[1] = 0;
	s->m.rot[2] = 0;
	s->m.rot[3] = 0;

	solidValidate(s);

	solidPutVb(s, b);
	return s;
}

// Would be slightly more efficient if we had an index (hypothetical `rmSolidAt`),
// but I'm not sure if that's ever something we'll have.
void rmSolid(gamestate *gs, solid *s) {
	gs->solids.rm(s);
	gs->selection.rm(s);
	velbox_remove(s->b);
	delete s;
}

static void playerUpdate(gamestate *gs, player *p) {
	memcpy(p->m.oldPos, p->m.pos, sizeof(p->m.pos));
	// range(i, 3) p->vel[i] += p->inputs[i]; // We moved this to collision physics!
	p->vel[2] -= 8; // gravity

	offset dest;
	range(i, 3) dest[i] = p->m.pos[i] + p->vel[i];

	queryResults.num = 0;
	p->prox = velbox_query(p->prox, p->m.pos, p->vel, 2000, &queryResults);
	unitvec forceDir;
	offset contactVel;
	rangeconst(j, queryResults.num) {
		solid *s = (solid*) queryResults[j];
		int64_t dist = collide_check(p, dest, 800, s, forceDir, contactVel);
		if (dist) pl_phys_standard(forceDir, contactVel, dist, dest, p);
	}

	memcpy(p->m.pos, dest, sizeof(dest));
}

void runTick(gamestate *gs) {
	velbox_refresh(gs->vb_root);
	rangeconst(i, gs->solids.num) {
		solidUpdate(gs, gs->solids[i]);
	}

	range(i, gs->players.num) {
		playerUpdate(gs, &gs->players[i]);
	}

	velbox_completeTick(gs->vb_root);
}

void mkSolidAtPlayer(gamestate *gs, player *p) {
	solid *s = addSolid(gs, p->prox, p->m.pos[0], p->m.pos[1], p->m.pos[2], 1000, 0, 4);
	memcpy(s->m.rot, p->m.rot, sizeof(iquat)); // Array types are weird in C
}

// I'm thinking `isSync` may be unused forever, but we can leave it for now (forever)
void prepareGamestateForLoad(gamestate *gs, char isSync) {
	int numPlayers = gs->players.num;

	cleanup(gs);
	// Re-initialize with valid (but empty) data
	init(gs);

	// Setup any data that might carry over (right now, just player count)
	setupPlayers(gs, numPlayers);
}

static void playerDupCleanup(player *p) {
	p->prox = (box*)p->prox->clone.ptr;
	range(i, 3) p->m.oldPos[i] = -1;
}

gamestate* dup(gamestate *orig) {
	gamestate *ret = (gamestate*)malloc(sizeof(gamestate));
	ret->players.init(orig->players);

	ret->vb_root = velbox_dup(orig->vb_root);

	ret->solids.init(orig->solids.num);
	ret->solids.num = orig->solids.num;
	rangeconst(i, ret->solids.num) {
		ret->solids[i] = solidDup(orig->solids[i]);
	}

	ret->selection.init(orig->selection.num);
	ret->selection.num = orig->selection.num;
	rangeconst(i, ret->selection.num) {
		ret->selection[i] = (solid*) orig->selection[i]->clone.ptr;
	}

	rangeconst(i, ret->players.num) {
		player *p = &ret->players[i];
		playerDupCleanup(p);
	}

	return ret;
}

void init(gamestate *gs) {
	gs->players.init();
	gs->selection.init();
	gs->solids.init();
	gs->vb_root = velbox_getRoot();
}

void cleanup(gamestate *gs) {
	velbox_freeRoot(gs->vb_root);
	rangeconst(i, gs->solids.num) {
		delete gs->solids[i];
	}
	gs->solids.destroy();
	gs->selection.destroy();
	gs->players.destroy();
}

// Seriz / Deser stuff

static void writeBlock(void *mem, int len) {
	// Could probably rewrite this to use `data->addAll`,
	// but right now that uses `setMax` instead of `setMaxUp` internally.
	int n = seriz_data->num;
	seriz_data->setMaxUp(n + len);
	memcpy(seriz_data->items + n, mem, len);
	seriz_data->num = n + len;
}

static void readBlock(void *mem, int len) {
	int i = seriz_index;
	if (i + len > seriz_data->num) {
		memset(mem, 0, len);
		return;
	}
	memcpy(mem, seriz_data->items+i, len);
	seriz_index += len;
}

static void transBlock(void *mem, int len) {
	if (seriz_reading) readBlock(mem, len);
	else writeBlock(mem, len);
}

static void transSolid(solid *s) {
	transBlock(s->m.pos, sizeof(s->m.pos));
	transBlock(s->vel, sizeof(s->vel));
	trans64(&s->r);
	trans32(&s->shape);
	trans32(&s->tex);
	transBlock(s->m.rot, sizeof(s->m.rot));
	transWeakRef(&s->b, &boxSerizPtrs);
	if (seriz_reading) {
		// Some fields that we want consistently initialized,
		// but ideally nothing will need them before they are reset.
		memset(s->m.oldPos, 0, sizeof(s->m.oldPos));
		memset(s->oldRot, 0, sizeof(s->oldRot));

		// Sanity / malicious data check.
		// Doing our own serialization means we're responsible for avoiding array access issues...
		solidValidate(s);

		// Box and Solid have ptrs to each other, but only one dir gets
		// explicitly serialized. Other has to be handled by hand during de-seriz.
		s->b->data = s;
	}
}

static void transAllSolids(gamestate *gs) {
	transItemCount(&gs->solids);
	rangeconst(i, gs->solids.num) {
		if (seriz_reading) gs->solids[i] = new solid();
		transSolid(gs->solids[i]);
	}
}

static void transPlayer(player *p) {
	range(i, 3) trans64(&p->m.pos[i]);
	range(i, 3) trans64(&p->vel[i]);
	range(i, 4) trans32(&p->m.rot[i]);
	if (seriz_reading) {
		range(i, 3) p->m.oldPos[i] = -1;
	}
}

/* Unused
static void writeStr(list<char> *data, const char *str) {
	int len = strlen(str);
	if (len > 255) len = 255;
	data->add((unsigned char) len);
	int end = data->num + len;
	data->setMaxUp(end);
	memcpy(data->items + data->num, str, len);
	data->num = end;
}

static void readStr(const list<const char> *data, int *ix, char *dest, int limit) {
	int reportedLen = (unsigned char) read8(data, ix);
	int i = *ix;
	int avail = data->num - i;
	if (avail < reportedLen || limit - 1 < reportedLen) {
		// Don't mess around even trying to read this.
		// `avail` could be negative, e.g.
		*ix = data->num;
		*dest = '\0';
		return;
	}

	memcpy(dest, data->items + i, reportedLen);
	dest[reportedLen] = '\0';
	*ix = i + reportedLen;
}
*/

void serialize(gamestate *gs, list<char> *data) {
	seriz_data = data;
	seriz_reading = 0;
	seriz_version = seriz_latestVersion;

	seriz_writeHeader();

	velbox_trans(gs->vb_root);
	transAllSolids(gs);
	// We don't bother with the selection for now

	write8(gs->players.num);
	range(i, gs->players.num) {
		transPlayer(&gs->players[i]);
	}
}

void deserialize(gamestate *gs, list<char> *data, char fullState) {
	seriz_data = data;
	seriz_reading = 1;
	// This will set seriz_version and seriz_index
	// (if no error)
	if (seriz_verifyHeader()) return;

	velbox_trans(gs->vb_root);
	transAllSolids(gs);
	// We don't bother with the selection for now

	int players = read8();
	// If there are fewer players in the game than the file, ignore extras.
	if (gs->players.num < players) players = gs->players.num;
	// If there are more  players in the game than the file, we rely on
	// `prepareGamestateForLoad` to have consistently initialized them.
	
	range(i, players) transPlayer(&gs->players[i]);
	// Maybe extra data here corresponding to extra players we don't have seated currently
}

void gamestate_init() {
	queryResults.init();
}

void gamestate_destroy() {
	queryResults.destroy();
}
