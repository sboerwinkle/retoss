#include <math.h>
#include "../util.h"

#include "../matrix.h"
#include "../gamestate.h"

#include "../bctx.h"
#include "../collision.h"
#include "../game_gamestate.h"
#include "../graphics.h"
#include "../player.h"
#include "../random.h"
#include "../serialize.h"
#include "../task.h"

#include "blast.h"

#include "rocket.h"

#define RADIUS 400
#define BLAST_R 5000

static void putVb(taskRocket *data, box *guess) {
	box *tmp = velbox_alloc();
	data->m.b = tmp;
	memcpy(tmp->pos, data->m.oldPos, sizeof(tmp->pos));
	// Todo usually data->vel is correct, maybe
	//      add special case for after impact to do more math?
	//memcpy(tmp->vel, data->vel, sizeof(tmp->vel));
	range(i, 3) tmp->vel[i] = data->m.pos[i] - data->m.oldPos[i];
	tmp->r = RADIUS;
	tmp->end = tmp->start + 1;
	tmp->data = &data->m;
	velbox_insert(guess, tmp);
}

static void blowUp(gamestate *gs, taskRocket *data, box *p) {

	offset queryV;
	range(i, 3) queryV[i] = data->m.pos[i] - data->m.oldPos[i];

	list<mover*> blastMovers;
	blastMovers.init();
	velbox_query(p, data->m.oldPos, queryV, BLAST_R, &blastMovers);
	rangeconst(iter, blastMovers.num) {
		mover *m = blastMovers[iter];
		if (!(m->type & T_PLAYER)) continue;
		player *blastee = playerFromMover(m);
		offset d;
		range(i, 3) d[i] = blastee->m.pos[i] - data->m.pos[i];
		// This should almost always be small enough to safely square,
		// but for relativistic players maybe not.
		int64_t mg = mag(d);
		if (mg > BLAST_R || !mg) continue;
		range(i, 3) blastee->vel[i] += d[i]*800/mg;
		player_hit(gs, gs->clock+1, blastee, 2);
	}
	blastMovers.destroy();
}

static char step(gamestate *gs, void *_data) {
	taskRocket *data = (taskRocket*)_data;

	box *parent = data->m.b->parent;
	// For now these are 1-frame only boxes,
	// so we know it's dead!
	velbox_reclaimDead(data->m.b);

	if (data->dead) {
		// force/damage/sound happened at the end of last frame,
		// smoke happens at the start of this one.
		tskBlast_create(gs, data->m.pos, data->vel, 5000, 80, 160);
		return 1;
	}

	// TODO We're never going to use `oldRot` or `rot`,
	//      maybe they rightly belong to `solid`?
	//      Players need them too; maybe players have a solid?
	memcpy(data->m.oldPos, data->m.pos, sizeof(offset));

	range(i, 3) {
		data->vel[i] += data->accel[i];
	}

	offset smokeV;
	if (!(data->ttl % 4)) {
		// Our "whoosh" sounds are about 4 frames long (plus 0.09 seconds of fade in/out)
		uint32_t soundId = data->soundId + data->ttl;
		uint32_t seed = gs->clock*17 + data->ttl/4;
		int variant = splitmix32(&seed) % 3;
		addSound(gs->clock, data->m.pos, data->vel, soundId, SND_WHOOSH_A + variant);
	}
	range(i, 3) {
		data->m.pos[i] += data->vel[i];
		smokeV[i] = data->vel[i] - 4*data->accel[i];
	}
	tskBlast_create(gs, data->m.oldPos, smokeV, 2000, 0, 1);

	// Todo: Can I do better than re-allocating every time? Is it worth it?
	list<mover*> toCheck;
	toCheck.init();

	unitvec forceDir;
	offset contactVel;
	mover *best = NULL;
	int32_t bestTime = FIXP+1;
	// `bestVec` will mean different things depending on what we hit,
	// but it's always related to figuring out the rocket's explosion positoin.
	offset bestVec;

	parent = velbox_query(parent, data->m.oldPos, data->vel, RADIUS, &toCheck);
	rangeconst(iter, toCheck.num) {
		mover *other = toCheck[iter];
		int32_t type = other->type & T_MASK;
		int32_t time;
		if (type == T_PROJ) {
			// Need to figure out where the radius comes from -
			// mover should probably have either a radius or the box ptr
			char hit = collide_sphere(data->m.oldPos, data->m.pos, RADIUS*2, other, &time);
			if (hit && time < bestTime) {
				best = other;
				bestTime = time;
				memcpy(bestVec, other->pos, sizeof(bestVec));
			}
			continue;
		}
		solid *s;
		solid tmp;
		if (!type) {
			s = solidFromMover(other);
		} else if (type == T_PLAYER) {
			// TODO players should contain a full-fledged solid,
			//      but making it on the fly works for now.
			tmp.m = *other;
			tmp.r = PLAYER_SHAPE_RADIUS;
			// `tex`, `b`, and `clone` don't matter
			s = &tmp;
		} else {
			printf("tasks/rocket: bad type 0x%X\n", type);
			continue;
		}

		int64_t dist = collide_check(data->m.oldPos, data->m.pos, RADIUS, s, forceDir, contactVel, &time);
		if (dist && time < bestTime) {
			// contactVel has to be updated with our velocity,
			// definitely the weirdest part of the `collide_check` behavior.
			range(i, 3) contactVel[i] += data->vel[i];
			// To avoid hitting things we're leaving... like the player that launched the rocket
			if (dot(contactVel, forceDir) <= 0) {
				best = other;
				bestTime = time;
				memcpy(bestVec, contactVel, sizeof(bestVec));
			}
		}
	}

	toCheck.destroy();

	if (best) {
		int32_t type = T_MASK & best->type;
		if (type == T_PROJ) {
			memcpy(data->m.pos, bestVec, sizeof(bestVec));
			taskRocket *other = rocketFromMover(best);
			other->live = 0;
			// data->vel unchanged.
		} else if (type == T_PLAYER) {
			// Ignore the surface velocity of players,
			// it'd be kinda funny if you could "flick"
			// rockets but frankly it would mostly be weird
			player *pl = playerFromMover(best);
			range(i, 3) {
				int64_t d = (pl->m.pos[i] - pl->m.oldPos[i]) - (data->m.pos[i] - data->m.oldPos[i]);
				data->m.pos[i] += d * (FIXP-bestTime) / FIXP;
			}
			memcpy(data->vel, pl->vel, sizeof(data->vel));
			// Direct hit does 1 extra damage
			// (you should be caught in the blast, too)
			player_hit(NULL, 0, pl, 1);
		} else {
			range(i, 3) {
				data->m.pos[i] -= bestVec[i] * (FIXP-bestTime) / FIXP;
				data->vel[i] -= bestVec[i];
			}
		}
		data->live = 0;
	}

	data->ttl--;
	if (!data->ttl) data->live = 0;

	if (!data->live) {
		blowUp(gs, data, parent);
		uint32_t soundId = data->soundId + 0x800 + data->ttl;
		addSound(gs->clock+1, data->m.pos, data->vel, soundId, SND_POP);
		data->dead = 1;
	}

	putVb(data, parent);

	return 0;
}

static char trans(gamestate *gs, void **ptr) {
	if (seriz_reading) {
		*ptr = malloc(sizeof(taskRocket));
	}
	taskRocket *data = (taskRocket*)*ptr;

	if (!seriz_reading && !vb_live(data->m.b)) {
		if (seriz_error()) {
			puts("Rocket velbox not live, was it initialized improperly?");
		}
	}
	transMover(&data->m, T_PROJ, T_PROJ+1);
	transOffset(data->vel);
	transOffset(data->accel);
	trans8(&data->dead);
	trans8(&data->live);
	trans16(&data->ttl);
	trans32(&data->soundId);
	return 0;
}

static void copy(void **_to, void *_from) {
	taskRocket *from = (taskRocket*)_from;
	taskRocket *to = (taskRocket*)malloc(sizeof(taskRocket));
	*_to = to;

#ifndef NODEBUG
	if (!vb_live(from->m.b)) {
		puts("Rocket velbox not live during copy!");
	}
#endif
	*to = *from;
	to->m.b = (box*)to->m.b->clone.ptr;
	to->m.b->data = &to->m;
}

static void destroy(void *_data) {
	taskRocket *data = (taskRocket*)_data;

	// No need to clean up our box.
	// During a game cleanup the velbox heirarchy will already be torn down,
	// and we only explicitly request a destroy after cleaning up our box.

	free(data);
}

void taskRocket_draw(void *_data) {
	taskRocket *data = (taskRocket*)_data;
	// TODO Is radius correct? Off by a factor of 2 in some direction?
	// TODO Should pulse / waver
	int64_t r = RADIUS;
	drawBillboard(data->m.oldPos, data->m.pos, 1, 42.0/64, 0, 6.0/64, r);

	reset3dTexScale();
}

// TODO Review anything that might be looking at T_PROJ.
//      Eventually the rifle tool, but maybe not yet???

void taskRocket_create(gamestate *gs, offset p1, offset vel, unitvec dir, box *parent, uint32_t soundId) {
	taskRocket *data = (taskRocket*)malloc(sizeof(taskRocket));
	addTaskEnd(gs, TSK_ROCKET, data);

	range(i, 3) {
		data->m.oldPos[i] = -1;
		// Arbitrarily use radius as starting offset here
		data->m.pos[i] = p1[i] + RADIUS * dir[i] / FIXP;
		data->vel[i] = vel[i] + 600 * dir[i] / FIXP;
		data->accel[i] = 300 * dir[i] / FIXP;
	}

	// Not really ever used, but prevents warnings from valgrind about `write()`ing uninitialized data
	memset(data->m.rot, 0, sizeof(data->m.rot));

	data->m.type = T_PROJ;
	data->dead = 0;
	data->live = 1;
	// 10s of flight time
	// (`+2` => make it divisible by 4 so sound starts immediately)
	data->ttl = 10*15 + 2;
	data->soundId = soundId;

	// a dead box with the right parent
	data->m.b = velbox_alloc();
	data->m.b->parent = parent;
}

void taskRocket_define(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->copy = &copy;
	d->destroy = &destroy;
}
