#include "util.h"
#include "cloneable.h"

#include "random.h"
#include "serialize.h"

#include "gamestate.h"

#include "collision.h"
#include "player.h"

static list<mover*> queryResults;
static list<box*> tmpPlayerBoxes;

int32_t gs_gravity = 30;

void resetPlayer(gamestate *gs, int i) {
	gs->players[i] = {
		.m={
			.pos={0,0,0},
			.oldPos={-1,-1,-1},
			.rot={FIXP,0,0,0},
			.oldRot={0,0,0,0},
			.type=T_PLAYER,
		},
		.vel={0,0,0},
		.inputs={0,0,0},
		.jump=0,
		.shoot=0,
		.cooldown=0,
		.hits=0,
		.hitsCooldown=0,
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
	if (s->m.type < 0 || s->m.type >= NUM_SHAPES) {
		printf("Invalid shape type %d!!\n", s->m.type);
		s->m.type = 0;
	}
	// `s->tex & 31` is used in drawing code in main.cpp
	int tex = s->tex & 31;
	if (tex < 0 || tex >= NUM_TEXS) {
		printf("Invalid shape texture %d!!\n", s->tex);
		s->tex = 0;
	}
}

static void solidPutVb(solid *s, box *guess) {
	box *tmp = velbox_alloc();
	s->b = tmp;
	memcpy(tmp->pos, s->m.pos, sizeof(s->m.pos));
	tmp->vel[0] = 0;
	tmp->vel[1] = 0;
	tmp->vel[2] = 0;
	tmp->r = s->r*shapeDiagonalMultipliers[s->m.type];
	tmp->end = tmp->start + 15; // 1 second
	tmp->data = &s->m;
	velbox_insert(guess, tmp);

}

static void solidUpdate(gamestate *gs, solid *s) {
	memcpy(s->m.oldPos, s->m.pos, sizeof(s->m.pos));
	// For stuff that doesn't spin, we could maybe just set this up after de-seriz and cloning.
	memcpy(s->m.oldRot, s->m.rot, sizeof(s->m.rot));

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
	// I don't care enough to reset oldRot, but the same logic applies.

	s->clone.ptr = t;
	t->b = (box*)(s->b->clone.ptr);
	t->b->data = &t->m;

	return t;
}

solid* addSolid(gamestate *gs, box *b, int64_t x, int64_t y, int64_t z, int64_t r, int32_t shape, int32_t tex) {
	solid *s = new solid();
	gs->solids.add(s);
	s->m.pos[0] = x;
	s->m.pos[1] = y;
	s->m.pos[2] = z;
	s->m.oldPos[0] = s->m.oldPos[1] = s->m.oldPos[2] = -1;
	s->m.type = shape;
	s->vel[0] = s->vel[1] = s->vel[2] = 0;
	s->r = r;
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
	gs->solids.stableRm(s);
	// May not exist in selection, need to check first.
	int ix;
	if (-1 != (ix = gs->selection.find(s))) gs->selection.stableRmAt(ix);
	velbox_remove(s->b);
	delete s;
}

static char playerPhysLe(mover* const &a, mover* const &b) {
	// Simple for now. Lower Z => floor-like => check first
	return a->pos[2] <= b->pos[2];
}

static void playerUpdate(gamestate *gs, player *p) {
	// We copy `rot`=>`oldRot` when player input happens.
	memcpy(p->m.oldPos, p->m.pos, sizeof(p->m.pos));
	p->vel[2] -= gs_gravity; // gravity

	offset dest;
	range(i, 3) dest[i] = p->m.pos[i] + p->vel[i];

	queryResults.num = 0;
	p->prox = velbox_query(p->prox, p->m.pos, p->vel, 2000, &queryResults);
	unitvec forceDir;
	offset contactVel;
	queryResults.qsort(playerPhysLe);
	rangeconst(j, queryResults.num) {
		// Todo: We are blindly assuming this mover is part of a solid.
		//       It's a safe bet for now, since we only keep players in the
		//       velbox space briefly (and not right now), but it's brittle.
		solid *s = solidFromMover(queryResults[j]);
		int64_t dist = collide_check(p, dest, PLAYER_SHAPE_RADIUS, s, forceDir, contactVel);
		if (dist) pl_phys_standard(forceDir, contactVel, dist, dest, p);
	}

	memcpy(p->m.pos, dest, sizeof(dest));

	if (p->hitsCooldown) {
		p->hitsCooldown--;
		if (!p->hitsCooldown) {
			if (p->hits >= 3) {
				// TODO: death
			} else {
				p->hits = 0;
			}
		}
	}
}

static void playerAddBoxes(gamestate *gs) {
	tmpPlayerBoxes.num = 0;
	rangeconst(i, gs->players.num) {
		player &p = gs->players[i];

		box *tmp = velbox_alloc();
		memcpy(tmp->pos, p.m.oldPos, sizeof(tmp->pos));
		range(j, 3) tmp->vel[j] = p.m.pos[j] - p.m.oldPos[j];
		// TODO define for SHAPE_CUBE (=0)
		tmp->r = PLAYER_SHAPE_RADIUS*shapeDiagonalMultipliers[0];
		tmp->end = tmp->start + 1; // TODO verify this is correct - I think it is?
		tmp->data = &p.m;
		velbox_insert(p.prox, tmp);
		tmpPlayerBoxes.add(tmp);
	}
}

static void playerRmBoxes(gamestate *gs) {
	rangeconst(i, tmpPlayerBoxes.num) {
		velbox_remove(tmpPlayerBoxes[i]);
	}
}

void runTick(gamestate *gs) {
	velbox_refresh(gs->vb_root);

	rangeconst(i, gs->solids.num) {
		solidUpdate(gs, gs->solids[i]);
	}
	range(i, gs->players.num) {
		playerUpdate(gs, &gs->players[i]);
	}

	// Todo: If I cared about efficiency here, `trails` could be a `queue`.
	while(gs->trails.num && gs->trails[0].expiry <= vb_now) {
		gs->trails.stableRmAt(0);
	}

	playerAddBoxes(gs);
	range(i, gs->players.num) {
		// This uses `bcast`, which requires stuff to have its
		// old position populated. Must run after other stuff.
		pl_postStep(gs, &gs->players[i]);
	}
	playerRmBoxes(gs);

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

	// Nothing special in the trail data, we can just copy it all
	ret->trails.init(orig->trails);

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
	gs->trails.init();
	gs->vb_root = velbox_getRoot();
}

void cleanup(gamestate *gs) {
	velbox_freeRoot(gs->vb_root);
	gs->trails.destroy();
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
	trans32(&s->m.type);
	trans32(&s->tex);
	transBlock(s->m.rot, sizeof(s->m.rot));
	transWeakRef(&s->b, &boxSerizPtrs);
	if (seriz_reading) {
		// Some fields that we want consistently initialized,
		// but ideally nothing will need them before they are reset.
		memset(s->m.oldPos, 0, sizeof(s->m.oldPos));
		memset(s->m.oldRot, 0, sizeof(s->m.oldRot));

		// Sanity / malicious data check.
		// Doing our own serialization means we're responsible for avoiding array access issues...
		solidValidate(s);

		// Box and Solid have ptrs to each other, but only one dir gets
		// explicitly serialized. Other has to be handled by hand during de-seriz.
		s->b->data = &s->m;
	}
}

static void transAllSolids(gamestate *gs) {
	transItemCount(&gs->solids);
	rangeconst(i, gs->solids.num) {
		if (seriz_reading) gs->solids[i] = new solid();
		transSolid(gs->solids[i]);
	}
}

static void transTrail(trail *tr) {
	transBlock(tr->origin, sizeof(tr->origin));
	transBlock(tr->dir, sizeof(tr->dir));
	trans64(&tr->len);
	trans32(&tr->expiry);
}

static void transTrails(gamestate *gs) {
	transItemCount(&gs->trails);
	rangeconst(i, gs->trails.num) {
		transTrail(&gs->trails[i]);
	}
}

static void transPlayer(player *p) {
	range(i, 3) trans64(&p->m.pos[i]);
	range(i, 3) trans64(&p->vel[i]);
	range(i, 3) trans32(&p->inputs[i]);
	trans8(&p->jump);
	trans8(&p->shoot);
	trans32(&p->cooldown);
	trans8(&p->hits);
	trans8(&p->hitsCooldown);
	range(i, 4) trans32(&p->m.rot[i]);
	if (seriz_reading) {
		range(i, 3) p->m.oldPos[i] = -1;
		range(i, 4) p->m.oldRot[i] = 0;
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
	transTrails(gs);

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
	transTrails(gs);

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
	tmpPlayerBoxes.init();
}

void gamestate_destroy() {
	tmpPlayerBoxes.init();
	queryResults.destroy();
}
