#include "util.h"
#include "cloneable.h"

#include "random.h"
#include "serialize.h"

#include "gamestate.h"
#include "gamestate_box.h"

#include "collision.h"

list<cloneable*> dummyVelboxSerizList;

static list<box**> lateResolveVbClones;
static list<void*> queryResults;

void resetPlayer(gamestate *gs, int i) {
	gs->players[i] = {
		.pos={0,0,0},
		.inputs={0,0,0},
		.prox=gs->vb_root,
		.tmp=0,
	};
}

void setupPlayers(gamestate *gs, int numPlayers) {
	gs->players.setMaxUp(numPlayers);
	gs->players.num = numPlayers;
	range(i, gs->players.num) resetPlayer(gs, i);
}

static void solidPutVb(solid *s, box *guess) {
	box *tmp = velbox_alloc();
	s->b = tmp;
	memcpy(tmp->pos, s->pos, sizeof(s->pos));
	tmp->vel[0] = 0;
	tmp->vel[1] = 0;
	tmp->vel[2] = 0;
	tmp->r = s->r*7/4; // cube diagonal is sqrt(3), or approx 1.75
	tmp->end = tmp->start + 15; // 1 second
	tmp->data = s;
	velbox_insert(guess, tmp);

}

static void solidUpdate(gamestate *gs, solid *s) {
	memcpy(s->oldPos, s->pos, sizeof(s->pos));
	// For stuff that doesn't spin, we could maybe just set this up after de-seriz and cloning.
	memcpy(s->oldRot, s->rot, sizeof(s->rot));

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
	t->oldPos[0] = t->oldPos[1] = t->oldPos[2] = -1;

	s->clone.ptr = t;
	lateResolveVbClones.add(&t->b);

	return t;
}

static solid* addSolid(gamestate *gs, box *b, int64_t x, int64_t y, int64_t z, int64_t r, int32_t tex) {
	solid *s = new solid();
	gs->solids.add(s);
	s->pos[0] = x;
	s->pos[1] = y;
	s->pos[2] = z;
	s->oldPos[0] = s->oldPos[1] = s->oldPos[2] = -1;
	s->vel[0] = s->vel[1] = s->vel[2] = 0;
	s->r = r;
	s->tex = tex;
	s->rot[0] = FIXP;
	s->rot[1] = 0;
	s->rot[2] = 0;
	s->rot[3] = 0;

	solidPutVb(s, b);
	return s;
}

static void playerUpdate(gamestate *gs, player *p) {
	offset dest;
	range(i, 3) dest[i] = p->pos[i] + p->inputs[i];
	// Could replace this with a more-real velocity
	int64_t fakeVel[3] = {0,0,0};
	queryResults.num = 0;
	p->prox = velbox_query(p->prox, p->pos, fakeVel, 1000, &queryResults);
	rangeconst(j, queryResults.num) {
		solid *s = (solid*) queryResults[j];
		collide_check(p, dest, 200, s);
	}
	memcpy(p->pos, dest, sizeof(dest));
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

void mkSolidAtPlayer(gamestate *gs, player *p, iquat r) {
	solid *s = addSolid(gs, p->prox, p->pos[0], p->pos[1], p->pos[2], 1000, 4);
	memcpy(s->rot, r, sizeof(iquat)); // Array types are weird in C
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

static void resolveVbClones() {
	rangeconst(i, lateResolveVbClones.num) {
		box **b = lateResolveVbClones[i];
		*b = (box*)(*b)->clone.ptr;
	}
}

gamestate* dup(gamestate *orig) {
	gamestate *ret = (gamestate*)malloc(sizeof(gamestate));
	ret->players.init(orig->players);

	lateResolveVbClones.num = 0;
	ret->solids.init(orig->solids.num);
	ret->solids.num = orig->solids.num;
	rangeconst(i, ret->solids.num) {
		ret->solids[i] = solidDup(orig->solids[i]);
	}

	ret->vb_root = velbox_dup(orig->vb_root);

	resolveVbClones();
	rangeconst(i, ret->players.num) {
		player *p = &ret->players[i];
		p->prox = (box*)p->prox->clone.ptr;
	}

	return ret;
}

void init(gamestate *gs) {
	gs->players.init();
	gs->solids.init();
	gs->vb_root = velbox_getRoot();

	addSolid(gs, gs->vb_root,     0, 3000,    0,  1000, 2+32);
	addSolid(gs, gs->vb_root,  1000, 4000, 1000,  1000, 4);
	addSolid(gs, gs->vb_root, 31000, 9000, 1000, 15000, 4);
	iquat r1 = {(int32_t)(FIXP*0.9801), (int32_t)(FIXP*0.1987), 0, 0}; // Just me with a lil' rotation lol
	memcpy(gs->solids[2]->rot, r1, sizeof(r1)); // Array types are weird in C
}

void cleanup(gamestate *gs) {
	velbox_freeRoot(gs->vb_root);
	gs->players.destroy();
	rangeconst(i, gs->solids.num) {
		delete gs->solids[i];
	}
	gs->solids.destroy();
}

// Seriz / Deser stuff

static void writeBlock(char *mem, int len) {
	// Could probably rewrite this to use `data->addAll`,
	// but right now that uses `setMax` instead of `setMaxUp` internally.
	int n = seriz_data->num;
	seriz_data->setMaxUp(n + len);
	memcpy(seriz_data->items + n, mem, len);
	seriz_data->num = n + len;
}

static void readBlock(char *mem, int len) {
	int i = seriz_index;
	if (i + len > seriz_data->num) {
		memset(mem, 0, len);
		return;
	}
	memcpy(mem, seriz_data->items+i, len);
	seriz_index += len;
}

// Not sure if we'll ever need this again lol
static void transBlock(char *mem, int len) {
	if (seriz_data) readBlock(mem, len);
	else writeBlock(mem, len);
}

static void transPlayer(player *p) {
	range(i, 3) trans64(&p->pos[i]);
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

	// TODO seriz solids etc

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

	// TODO seriz solids etc

	int players = read8();
	// If there are fewer players in the game than the file, ignore extras.
	if (gs->players.num < players) players = gs->players.num;
	// If there are more  players in the game than the file, we rely on
	// `prepareGamestateForLoad` to have consistently initialized them.
	
	range(i, players) transPlayer(&gs->players[i]);
	// Maybe extra data here corresponding to extra players we don't have seated currently
}

void gamestate_init() {
	dummyVelboxSerizList.init();
	lateResolveVbClones.init();
	queryResults.init();
}

void gamestate_destroy() {
	queryResults.destroy();
	lateResolveVbClones.destroy();
	dummyVelboxSerizList.destroy();
}
