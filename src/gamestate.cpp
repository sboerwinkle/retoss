#include "util.h"

#include "random.h"
#include "serialize.h"

#include "gamestate.h"
#include "game_graphics.h"

#include "collision.h"
#include "constel.h"
#include "player.h"

static list<mover*> queryResults;
static list<box*> tmpPlayerBoxes;

int32_t gs_gravity = 30;

void resetPlayer(gamestate *gs, int ix) {
	player &p = gs->players[ix];
	p.m={
		.pos={0,0,0},
		.oldPos={-1,-1,-1},
		.rot={FIXP,0,0,0},
		.oldRot={0,0,0,0},
		.type=T_PLAYER,
	};
	range(i, 3) {
		p.vel[i] = 0;
		p.inputs[i] = 0;
	}
	p.team=-1;
	p.prox=gs->vb_root;
	p.skin=NULL;

	softResetPlayer(&p);
}

void softResetPlayer(player *_p) {
	player &p = *_p;
	p.jump=0;
	p.shoot=0;
	p.alive=1;
	p.cooldown=0;
	p.hits=0;
	p.hitsCooldown=0;
}

void setupPlayers(gamestate *gs, int numPlayers) {
	gs->players.setMaxUp(numPlayers);
	gs->players.num = numPlayers;
	range(i, gs->players.num) resetPlayer(gs, i);
}

void killPlayer(player *p) {
	p->alive = 0;
	// If player was reloading, crosshairs will still
	// try to play the animation frame-by-frame,
	// but no progress is being made between game states.
	// This causes a weird jitter.
	p->cooldown = 0;
}

void validateSize(int64_t *_size) {
	int64_t &size = *_size;
	if (size <= 0) {
		printf("Invalid size %ld!\n", size);
		size = 1'000;
	}
	if (size >= (int64_t)1<<47) {
		// Limit here is based on needing to multiply by shapeDiagonalMultipliers
		// and still have the result be exactly representable by a double.
		// Doubles can represent any integer under 1<<53, and the biggest numerator
		// in shapeDiagonalMultipliers is 33 (just over 1<<5, treat as 1<<6),
		// so we take 53-6=47 as our limit.
		// I think this is a few bits less than the other limit, which is that
		// for any offset `o`, the value `FIXP*o` must not overflow.
		printf("Invalid size %ld (too big)!\n", size);
		size = (int64_t)1<<46; // This is bafflingly large
	}
}

void validateShape(int32_t *_shape) {
	int32_t &shape = *_shape;
	if (shape < 0 || shape >= NUM_SHAPES) {
		printf("Invalid shape type %d!!\n", shape);
		shape = 0;
	}
}

void validateTex(int32_t *_tex) {
	// `s->tex & 31` is used in drawing code in main.cpp
	int tex = *_tex & 31;
	if (tex < 0 || tex >= NUM_TEXS) {
		printf("Invalid shape texture %d!!\n", *_tex);
		*_tex = 0;
	}
}

// cube diagonal is sqrt(3), or approx 1.75 (=7/4)
// slab diagonal is sqrt(2), even with a bit of thickenss it's comfortably within 1.5 (=3/2)
// pole diagonal is like really close to 1 once you do the math
double const shapeDiagonalMultipliers[NUM_SHAPES] = {7.0f/4, 3.0f/2, 33.0f/32};

static void solidValidate(solid *s) {
	validateSize(&s->r);
	validateShape(&s->m.type);
	validateTex(&s->tex);
}

void solidPutVb(solid *s, box *guess, int duration) {
	box *tmp = velbox_alloc();
	s->b = tmp;
	memcpy(tmp->pos, s->m.oldPos, sizeof(tmp->pos));
	range(i, 3) tmp->vel[i] = s->m.pos[i] - s->m.oldPos[i];
	tmp->r = s->r*shapeDiagonalMultipliers[s->m.type];
	tmp->end = tmp->start + duration;
	tmp->data = &s->m;
	velbox_insert(guess, tmp);
}

static void solidUpdate(gamestate *gs, solid *s) {
	memcpy(s->m.oldPos, s->m.pos, sizeof(s->m.pos));
	// For stuff that doesn't spin, we could maybe just set this up after de-seriz and cloning.
	memcpy(s->m.oldRot, s->m.rot, sizeof(s->m.rot));

	// The only other thing we're doing is updating our velbox,
	// so if it's still live we leave it alone.
	if (vb_live(s->b)) return;
	box *old = s->b;
	box *p = old->parent;
	velbox_reclaimDead(old);
	solidPutVb(s, p, 15);
}

void cpSolid(solid *t, solid *s) {
	*t = *s;
	// Not stictly necessary - dup'd states are never the canon state,
	// but the idea is that we don't want to rely on oldPos until the
	// thing in question has ticked this frame.
	t->m.oldPos[0] = t->m.oldPos[1] = t->m.oldPos[2] = -1;
	// I don't care enough to reset oldRot, but the same logic applies.

	s->clone.ptr = t;
	t->b = (box*)(s->b->clone.ptr);
	t->b->data = &t->m;
}

solid* addSolid(gamestate *gs, box *b, int64_t x, int64_t y, int64_t z, int64_t r, int32_t shape, int32_t tex) {
	solid *s = new solid();
	gs->solids.add(s);
	s->m.oldPos[0] = s->m.pos[0] = x;
	s->m.oldPos[1] = s->m.pos[1] = y;
	s->m.oldPos[2] = s->m.pos[2] = z;
	s->m.type = shape;
	//s->vel[0] = s->vel[1] = s->vel[2] = 0;
	s->r = r;
	s->tex = tex;
	s->m.rot[0] = FIXP;
	s->m.rot[1] = 0;
	s->m.rot[2] = 0;
	s->m.rot[3] = 0;

	solidValidate(s);

	solidPutVb(s, b, 15);
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

static void constelMoveSolids(constelInst *_ci) {
	constelInst &ci = *_ci;
	constel &c = *ci.c;
	imat rot1, rot2;
	imatFromIquat(rot1, ci.m.oldRot);
	imatFromIquat(rot2, ci.m.rot);
	rangeconst(i, ci.solids.num) {
		solid &s = ci.solids[i];
		constelPt &cp = c.points[i];

		offset o1, o2;
		imat_applySm(o1, rot1, cp.o); // TODO fix dumbass "Sm" naming
		imat_applySm(o2, rot2, cp.o);
		range(j, 3) {
			s.m.oldPos[j] = ci.m.oldPos[j] + o1[j];
			s.m.pos[j] = ci.m.pos[j] + o2[j];
		}

		iquat_mult(s.m.oldRot, cp.rot, ci.m.oldRot);
		iquat_mult(s.m.rot   , cp.rot, ci.m.rot   );
	}
}

static void constelUpdate(gamestate *gs, constelInst *_ci) {
	constelInst &ci = *_ci;
	// For my sanity, probably better if constelInst
	// always has either child solids or the aggregate mover
	// recorded in vb_root at the end of the tick.
	// Will make it simpler for e.g. camera position casting.

	// We don't want to be re-allocating `ci.solids` while it contains stuff
	// (as that would mess up the `mover*`s in `vb_tree`),
	// so it's fair to assume it has either 0 items or the same number as `c.points`.

	if (ci.solids.num) {
		constelMoveSolids(&ci);

		if (!vb_live(ci.solids[0].b)) {
			rangeconst(i, ci.solids.num) {
				box *old = ci.solids[i].b;
				box *p = old->parent;
				velbox_reclaimDead(old);
				solidPutVb(&ci.solids[i], p, ci.duration);
			}
		}
	}
}

static void cpConstelInst(constelInst *b, constelInst *a) {
	b->c = a->c;
	b->c->incr();

	b->m = a->m;
	b->duration = a->duration;

	b->solids.init(a->solids.num);
	b->solids.num = a->solids.num;
	rangeconst(i, a->solids.num) {
		cpSolid(&b->solids[i], &a->solids[i]);
	}

	a->clone.ptr = b;
}

constelInst* mkConstelInst(constel *c, int32_t duration) {
	constelInst *ci = (constelInst*)malloc(sizeof(constelInst));

	ci->c = c;
	c->incr();

	// Caller is expected to initialize most of ci->m, but we'll set the type here.
	// It's always 0, we don't even serialize it.
	ci->m.type = 0;

	ci->duration = duration;
	ci->solids.init();
	// For now, we don't really support constelInst's in implicit state,
	// so it's expected you'll pass this to `addConstelInst` to create the solids.

	return ci;
}

void addConstelInst(gamestate *gs, constelInst *ci) {
	constel *c = ci->c;

	ci->solids.setMaxUp(c->points.num);
	ci->solids.num = c->points.num;
	rangeconst(i, c->points.num) {
		constelPt &pt = c->points[i];
		solid &s = ci->solids[i];
		s.r = pt.r;
		s.tex = pt.tex;
		s.m.type = pt.type;
		// Other stuff is initialized in `constelMoveSolids`, below.
	}
	constelMoveSolids(ci);

	box *p = gs->vb_root;
	rangeconst(i, ci->solids.num) {
		solidPutVb(&ci->solids[i], p, ci->duration);
		p = ci->solids[i].b->parent;
	}

	gs->constels.add(ci);
}

void deleteConstelInst(constelInst *ci) {
	ci->c->decr();
	rangeconst(i, ci->solids.num) {
		velbox_remove(ci->solids[i].b);
	}
	ci->solids.destroy();
	free(ci);
}

void addTask(gamestate *gs, int taskId, void *data) {
	taskInstance *t = new taskInstance;
	t->defn = taskLookup(taskId);
	t->data = data;
	t->prev = gs->tasks.prev;
	t->prev->next = t;
	t->next = &gs->tasks;
	gs->tasks.prev = t;
}

static char playerPhysLe(mover* const &a, mover* const &b) {
	// Simple for now.
	// We check higher objects first,
	// mostly because this reduces the ability to jump off
	// horizontal seams in walls.
	return a->pos[2] >= b->pos[2];
}

static void playerUpdate(gamestate *gs, player *p) {
	// We copy `rot`=>`oldRot` when player input happens.
	memcpy(p->m.oldPos, p->m.pos, sizeof(p->m.pos));
	if (!p->alive) {
		int divisor = 64;
		// TODO some way to go real slow (but I'm out of net inputs for now lol)
		if (p->shoot & 1) divisor /= 4;
		range(i, 3) p->m.pos[i] += p->inputs[i] / divisor;

		p->prox = gs->vb_root;
		return;
	}

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
		// todo: I think at this point `p->m.pos` and `p->m.oldPos` are the same vector?
		//       If so, should change to `oldPos`, since that makes more sense in context.
		int64_t dist = collide_check(p->m.pos, dest, PLAYER_SHAPE_RADIUS, s, forceDir, contactVel);
		if (!dist) continue;
		range(i, 3) contactVel[i] += p->vel[i];
		pl_phys_standard(gs, forceDir, contactVel, dist, dest, p);
	}

	memcpy(p->m.pos, dest, sizeof(dest));

	if (p->hitsCooldown) {
		p->hitsCooldown--;
		if (!p->hitsCooldown) {
			if (p->hits >= 3) {
				killPlayer(p);
				// Todo: Add gibs
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
		if (!p.alive) continue;

		box *tmp = velbox_alloc();
		memcpy(tmp->pos, p.m.oldPos, sizeof(tmp->pos));
		range(j, 3) tmp->vel[j] = p.m.pos[j] - p.m.oldPos[j];
		// TODO define for SHAPE_CUBE (=0)
		tmp->r = PLAYER_SHAPE_RADIUS*shapeDiagonalMultipliers[0];
		tmp->end = tmp->start + 1;
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

	gs->clock++;
	vb_now = gs->clock;
	velbox_refresh(gs->vb_root);
	taskInstance *toDelete = NULL;

	rangeconst(i, gs->solids.num) {
		solidUpdate(gs, gs->solids[i]);
	}

	// TODO: Currently I'm leaning towards making all `constelUpdate`s a task (that we just always have).
	//       Just because the structure is a general concept (we'll need that once constels can become implicit),
	//       doesn't mean the logic to update them can't be a task.
	for (taskInstance *t = gs->tasks.next; t != &gs->tasks; t = t->next) {
		if (t->defn->id == TSK_RAILS) {
			(*t->defn->step)(gs, t->data);
		}
	}
	rangeconst(i, gs->constels.num) {
		constelUpdate(gs, gs->constels[i]);
	}
	for (taskInstance *t = gs->tasks.next; t != &gs->tasks; t = t->next) {
		if (t->defn->id == TSK_RAILS) continue;
		if ((*t->defn->step)(gs, t->data)) {
			t->prev->next = t->next;
			t->next->prev = t->prev;
			t->prev = toDelete;
			// Leave t->next intact, for loop will need it
			toDelete = t;
		}
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
		if (gs->players[i].alive) {
			// This uses `bcast`, which requires stuff to have its
			// old position populated. Must run after other stuff.
			pl_postStep(gs, &gs->players[i]);
		}
	}
	playerRmBoxes(gs);

	while (toDelete) {
		taskInstance *t = toDelete;
		toDelete = t->prev;
		(*t->defn->destroy)(t->data);
		delete t;
	}
}

void mkSolidAtPlayer(gamestate *gs, player *p) {
	solid *s = addSolid(gs, p->prox, p->m.pos[0], p->m.pos[1], p->m.pos[2], 1000, 0, 4);
	memcpy(s->m.rot, p->m.rot, sizeof(iquat)); // Array types are weird in C
}

void prepareGamestateForLoad(gamestate *gs, char isSync) {
	// Shallow copy of player data.
	// We don't need all of it, but it's okay to be slow here.
	list<player> tmp;
	tmp.init(gs->players);
	rangeconst(i, tmp.num) {
		if (tmp[i].skin) tmp[i].skin->refs++;
	}

	cleanup(gs);
	// Re-initialize with valid (but empty) data
	init(gs);

	// Setup any data that might carry over (player count, teams, and skins)
	setupPlayers(gs, tmp.num);
	if (!isSync) {
		rangeconst(i, tmp.num) {
			gs->players[i].team = tmp[i].team;

			gs->players[i].skin = tmp[i].skin;
			if (tmp[i].skin) tmp[i].skin->refs++;
		}
	}
	rangeconst(i, tmp.num) {
		if (tmp[i].skin) tmp[i].skin->decr();
	}
	tmp.destroy();
}

static void playerDupCleanup(player *p) {
	p->prox = (box*)p->prox->clone.ptr;
	range(i, 3) p->m.oldPos[i] = -1;
	if (p->skin) p->skin->refs++;
}

static void playerDestroy(player *p) {
	if (p->skin) p->skin->decr();
}

gamestate* dup(gamestate *orig) {
	gamestate *ret = (gamestate*)malloc(sizeof(gamestate));
	ret->players.init(orig->players);

	ret->vb_root = velbox_dup(orig->vb_root);
	ret->clock = orig->clock;
	ret->seed = orig->seed;

	ret->solids.init(orig->solids.num);
	ret->solids.num = orig->solids.num;
	rangeconst(i, ret->solids.num) {
		ret->solids[i] = new solid();
		cpSolid(ret->solids[i], orig->solids[i]);
	}

	ret->constels.init(orig->constels.num);
	ret->constels.num = orig->constels.num;
	rangeconst(i, ret->constels.num) {
		ret->constels[i] = (constelInst*)malloc(sizeof(constelInst));
		cpConstelInst(ret->constels[i], orig->constels[i]);
	}

	ret->selection.init(orig->selection.num);
	ret->selection.num = orig->selection.num;
	rangeconst(i, ret->selection.num) {
		ret->selection[i] = (solid*) orig->selection[i]->clone.ptr;
	}

	// Nothing special in the trail data, we can just copy it all
	ret->trails.init(orig->trails);

	// Copy tasks.
	// TODO I have having to do all these allocs,
	//      can I put them in a block somehow?
	//      Or is it even actually a problem?
	ret->tasks.data = ret->tasks.defn = NULL;
	taskInstance *s = &ret->tasks;
	for (taskInstance *t = orig->tasks.next; t != &orig->tasks; t = t->next) {
		taskInstance *old = s;
		s = new taskInstance;
		old->next = s;
		s->prev = old;
		s->defn = t->defn;
		(*t->defn->copy)(&s->data, t->data);
	}
	s->next = &ret->tasks;
	ret->tasks.prev = s;

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
	gs->tasks.data = gs->tasks.defn = NULL;
	gs->tasks.next = gs->tasks.prev = &gs->tasks;
	gs->vb_root = velbox_getRoot();
	gs->constels.init();

	gs->clock = 0;
	vb_now = 0;
	gs->seed = 1;
}

void cleanup(gamestate *gs) {
	rangeconst(i, gs->constels.num) deleteConstelInst(gs->constels[i]);
	gs->constels.destroy();

	velbox_freeRoot(gs->vb_root);
	while (gs->tasks.next != &gs->tasks) {
		taskInstance *t = gs->tasks.next;
		gs->tasks.next = t->next;
		(*t->defn->destroy)(t->data);
		delete t;
	}
	gs->trails.destroy();
	rangeconst(i, gs->solids.num) {
		delete gs->solids[i];
	}
	gs->solids.destroy();
	gs->selection.destroy();
	rangeconst(i, gs->players.num) playerDestroy(&gs->players[i]);
	gs->players.destroy();
}

// Seriz / Deser stuff

void transSolid(solid *s) {
	transBlock(s->m.pos, sizeof(s->m.pos));
	//transBlock(s->vel, sizeof(s->vel));
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

		// Unused, so untested
		if (!seriz_reading) gs->solids[i]->clone.idx = i;
	}
}

static void transConstelInst(constelInst *ci) {
	transConstel(&ci->c);

	// At the moment, `constelInst`s don't move themselves, somebody else moves them
	// (or they just stay put). This means they don't update their own `oldPos`,
	// which means we have to serialize it here. Could maybe improve this somehow?
	range(i, 3) trans64(&ci->m.pos[i]);
	range(i, 3) trans64(&ci->m.oldPos[i]);
	range(i, 4) trans32(&ci->m.rot[i]);
	range(i, 4) trans32(&ci->m.oldRot[i]);
	trans32(&ci->duration);
	if (seriz_reading) {
		ci->solids.init();
	}
	// I'm a little groggy, and not sure about this one.
	// The thing is, we don't have to transfer the solids.
	// They're defined by the constel + the mover.
	// So I could always just re-create them during deserialization.
	// Only downside is we'd have to do the velbox placement over again,
	// but that's probably well worth it. Maybe this is a smaller Todo.
	transItemCount(&ci->solids);
	rangeconst(i, ci->solids.num) transSolid(&ci->solids[i]);
	// We have no way to serialize a reference to a constelInst's solid right now,
	// which is related to not setting clone.idx here (wouldn't be helpful).
}

static void transAllConstelInsts(gamestate *gs) {
	transItemCount(&gs->constels);
	rangeconst(i, gs->constels.num) {
		if (seriz_reading) gs->constels[i] = (constelInst*)malloc(sizeof(constelInst));
		transConstelInst(gs->constels[i]);
		if (!seriz_reading) {
			// Will need this for the task that holds pointers to constels
			gs->constels[i]->clone.idx = i;
		}
	}

	// We've written all references to the actual `constel`s.
	// Now that we have a complete list, we can serialize them too.
	constelSerizFinalize();

	if (seriz_reading) {
		// Validation that `constelInst`s have the same # of pts as their constels
		range(i, gs->constels.num) {
			constelInst *ci = gs->constels[i];
			if (ci->solids.num && ci->solids.num != ci->c->points.num) {
				if (seriz_error()) puts("Encountered a constelInst with the wrong # of pts");
				deleteConstelInst(ci);
				gs->constels.stableRmAt(i);
				i--;
			}
		}
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

static void transTasks(gamestate *gs) {
	if (seriz_reading) {
		while (int32_t taskId = read32()) {
			taskInstance *t = new taskInstance;
			t->defn = taskLookup(taskId-1);
			if (!t->defn) {
				if (seriz_error()) {
					printf("Bad task defn ID %d\n", taskId);
				}
				delete t;
				return;
			}
			if ((*t->defn->trans)(gs, &t->data)) {
				delete t;
				return;
			}
			t->prev = gs->tasks.prev;
			t->prev->next = t;
			t->next = &gs->tasks;
			gs->tasks.prev = t;
		}
	} else {
		for (taskInstance *t = gs->tasks.next; t != &gs->tasks; t = t->next) {
			write32(t->defn->id + 1);
			(*t->defn->trans)(gs, &t->data);
		}
		write32(0);
	}
}

static void transDyntexHolder(dyntex_holder *dh) {
	trans32(&dh->descr.baseTex);
	transStr(dh->descr.str, DYNTEX_BUF_LEN);
	if (seriz_reading) {
		dh->refs = 1;
		addGgcMsg(GGC_DYNTEX_NEW, dh);
	}
}

static void transPlayer(player *p) {
	range(i, 3) trans64(&p->m.pos[i]);
	range(i, 3) trans64(&p->vel[i]);
	range(i, 3) trans32(&p->inputs[i]);
	trans8(&p->jump);
	trans8(&p->shoot);
	trans8(&p->alive);
	trans8(&p->team);
	trans32(&p->cooldown);
	trans8(&p->hits);
	trans8(&p->hitsCooldown);
	range(i, 4) trans32(&p->m.rot[i]);

	if (seriz_reading) {
		if (p->skin) p->skin->decr();
		if (read8()) {
			p->skin = new dyntex_holder();
		} else {
			p->skin = NULL;
		}
	} else {
		write8(!!p->skin);
	}
	if (p->skin) transDyntexHolder(p->skin);

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

static void trans(gamestate *gs) {
	velbox_trans(gs->vb_root);
	transAllSolids(gs);
	transAllConstelInsts(gs);
	// We don't bother with the selection for now
	transTrails(gs);
	transTasks(gs);
	trans32(&gs->clock);
	trans32(&gs->seed);
}

void serialize(gamestate *gs, list<char> *data) {
	seriz_data = data;
	seriz_reading = 0;
	seriz_version = seriz_latestVersion;

	seriz_writeHeader();

	trans(gs);

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

	trans(gs);

	int players = read8();
	// If there are fewer players in the game than the file, ignore extras.
	if (gs->players.num < players) players = gs->players.num;
	// If there are more  players in the game than the file, we rely on
	// `prepareGamestateForLoad` to have consistently initialized them.
	
	range(i, players) transPlayer(&gs->players[i]);
	// Maybe extra data here corresponding to extra players we don't have seated currently
}

void dyntex_holder::decr() {
	refs--;
	if (refs) return;
	addGgcMsg(GGC_DYNTEX_OLD, this);
}

void gamestate_init() {
	queryResults.init();
	tmpPlayerBoxes.init();
}

void gamestate_destroy() {
	tmpPlayerBoxes.destroy();
	queryResults.destroy();
}
