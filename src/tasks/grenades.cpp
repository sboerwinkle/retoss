#include <math.h>
#include "../util.h"

#include "../matrix.h"
#include "../gamestate.h"

#include "../collision.h"
#include "../game_gamestate.h"
#include "../graphics.h"
#include "../player.h"
#include "../serialize.h"
#include "../task.h"

#include "blast.h"

#include "grenades.h"

#define RADIUS 100
#define BLAST_R 5000
#define TRIGGER_R 3000

static void putVb(taskGrenade *nade, box *guess) {
	box *tmp = velbox_alloc();
	nade->b = tmp;
	memcpy(tmp->pos, nade->oldPos, sizeof(tmp->pos));
	range(i, 3) tmp->vel[i] = nade->pos[i] - nade->oldPos[i];
	tmp->r = RADIUS;
	tmp->end = tmp->start + 1;
	tmp->data = nade;
	velbox_insert(guess, tmp);
}

// TODO This is coming out to be VERY similar to the rocket stuff,
//      should maybe make this shared.
// Todo Do grenades need to be launched?
static void blowUp(gamestate *gs, taskGrenadeExplosion *boom) {

	offset queryV;
	range(i, 3) queryV[i] = boom->p2[i] - boom->p1[i];

	uint32_t soundId = SND_ID_GRENADE + boom->soundId;
	addSound(gs->clock+1, boom->p2, queryV, soundId, SND_POP);

	list<mover*> blastMovers;
	blastMovers.init();
	velbox_query(boom->parent, boom->p1, queryV, BLAST_R, &blastMovers);
	rangeconst(iter, blastMovers.num) {
		mover *m = blastMovers[iter];
		if (!(m->type & T_PLAYER)) continue;
		player *blastee = playerFromMover(m);
		offset d;
		range(i, 3) d[i] = blastee->m.pos[i] - boom->p2[i];
		// This should almost always be small enough to safely square,
		// but for relativistic players maybe not.
		int64_t mg = mag(d);
		if (mg > BLAST_R || !mg) continue;
		range(i, 3) blastee->vel[i] += d[i]*600/mg;
		player_hit(gs, gs->clock+1, blastee, 1);
	}
	blastMovers.destroy();
}

static void addSmoke(gamestate *gs, taskGrenadeExplosion *boom) {
	// todo: recalculated vs blowUp
	offset v;
	range(i, 3) v[i] = boom->p2[i] - boom->p1[i];

	tskBlastData *blast = tskBlast_create(gs, boom->p2, v, BLAST_R, 40, 80);
	tskBlast_later(blast);
}

static char grenadePhysics(gamestate *gs, taskGrenade *nade, list<mover*> *_toCheck);

static char step(gamestate *gs, void *_data) {
	taskGrenadesData *data = (taskGrenadesData*)_data;
	list<taskGrenade> &l = data->l;
	list<taskGrenadeExplosion> booms;
	booms.init();

	list<mover*> toCheck;
	toCheck.init();
	range(i, l.num) {
		taskGrenade *nade = &l[i];
		if (grenadePhysics(gs, nade, &toCheck)) {
			taskGrenadeExplosion &boom = booms.add();
			boom.parent = nade->b;
			boom.soundId = nade->soundId;
			memcpy(boom.p1, nade->oldPos, sizeof(offset));
			memcpy(boom.p2, nade->pos, sizeof(offset));

			l.quickRmAt(i);
			if (i < l.num) {
				// Grenade moved memory locations
				nade->b->data = nade;
			}
			i--;
		}
	}
	toCheck.destroy();

	range(i, l.num) {
		taskGrenade *nade = &l[i];
		// A prospective parent got stored in `nade->b` during `grenadePhysics`.
		putVb(nade, nade->b);
	}

	// Eventually we might want grenade blasts to knock around other grenades,
	// so we do the explosions last.
	range(i, booms.num) {
		taskGrenadeExplosion &boom = booms[i];
		blowUp(gs, &boom);
		addSmoke(gs, &boom);
	}
	booms.destroy();

	return !l.num;
}

// Originally based on player physics
static char physLe(mover* const &a, mover* const &b) {
	int32_t aPl = a->type & T_PLAYER;
	int32_t bPl = b->type & T_PLAYER;
	if (aPl != bPl) return !!aPl;
	return a->pos[2] >= b->pos[2];
}

static char grenadePhysics(gamestate *gs, taskGrenade *nade, list<mover*> *_toCheck) {
	list<mover*> &toCheck = *_toCheck;
	// TODO: Need smoke like rockets have at explosion.
	//       Ideally I'd like to avoid them hanging on for an extra frame, though, if possible.

	memcpy(nade->oldPos, nade->pos, sizeof(offset));
	nade->vel[2] -= gs_gravity;
	range(i, 3) {
		nade->pos[i] += nade->vel[i];
	}
	box *parent = nade->b->parent;
	// 1-frame boxes, so it must be dead
	velbox_reclaimDead(nade->b);

	offset dest;
	memcpy(dest, nade->pos, sizeof(dest));
	unitvec forceDir;
	offset contactVel;
	int32_t time; // unused

	toCheck.num = 0;
	// The radius here needs to account for the greater of:
	// - how much the trajectory might bounce around
	// - the trigger radius
	// The trigger radius is probably bigger.
	nade->b = velbox_query(parent, nade->oldPos, nade->vel, TRIGGER_R, &toCheck);
	toCheck.qsort(physLe);
	rangeconst(iter, toCheck.num) {
		mover *other = toCheck[iter];
		int32_t type = other->type & T_MASK;
		if (type == T_PROJ) {
			// Don't collide with rockets or grenades
			continue;
		}
		solid *s;
		solid tmp;
		if (!type) {
			s = solidFromMover(other);
		} else if (type == T_PLAYER) {
			// TODO This is duplicated w/ rockets,
			//      but should go away once we give
			//      players a full-fledged solid.
			tmp.m = *other;
			tmp.r = PLAYER_SHAPE_RADIUS;
			// `tex`, `b`, and `clone` don't matter
			s = &tmp;
		} else {
			printf("tasks/grenades: bad type 0x%X\n", type);
			continue;
		}

		int64_t dist = collide_check(nade->oldPos, dest, RADIUS, s, forceDir, contactVel, &time);
		if (!dist) continue;
		range(i, 3) contactVel[i] += nade->vel[i];
		range(i, 3) dest[i] += forceDir[i] * dist / FIXP;
		int64_t normalForce = -dot(contactVel, forceDir);
		if (normalForce <= 0) continue;
		if (nade->bounces) {
			nade->bounces--;
			range(i, 3) nade->vel[i] += normalForce * forceDir[i] * 3 / (FIXP*2);
		} else {
			range(i, 3) {
				// Cancels out normal component from grenade vel and contactVel
				nade->vel[i] += normalForce * forceDir[i] / FIXP;
				contactVel[i] += normalForce * forceDir[i] / FIXP;
			}
			// contactVel bounded, it's now friction (but negative)
			bound64(contactVel, normalForce/2);
			// Apply friction
			range(i, 3) {
				dest[i] -= contactVel[i];
				nade->vel[i] -= contactVel[i];
			}
		}
	}

	memcpy(nade->pos, dest, sizeof(dest));

	// We do our detonation check based on old positions
	// simply because `dest` could be bounced all over by geometry,
	// and we don't want to have detection be based on dumb luck
	// (the query can report false positives).
	// The old position doesn't bounce around though.
	rangeconst(iter, toCheck.num) {
		mover *other = toCheck[iter];
		int32_t type = other->type & T_MASK;
		if (type != T_PLAYER) continue;
		player *p = playerFromMover(other);
		if (p->team == nade->team) continue;
		offset delta;
		range(i, 3) delta[i] = nade->oldPos[i] - other->oldPos[i];
		int64_t mg = mag(delta);
		if (mg <= TRIGGER_R) {
			return 1;
		}
	}

	return 0;
}

static char trans(gamestate *gs, void **ptr) {
	if (seriz_reading) *ptr = new taskGrenadesData;
	taskGrenadesData *data = (taskGrenadesData*)*ptr;
	if (seriz_reading) data->l.init();

	transItemCount(&data->l);

	rangeconst(i, data->l.num) {
		taskGrenade *nade = &data->l[i];
		transMover(nade, T_PROJ);
		transOffset(nade->vel);
		trans8(&nade->bounces);
		trans8(&nade->team);
		trans16(&nade->soundId);
	}

	return 0;
}

static void copy(void **_to, void *_from) {
	taskGrenadesData *from = (taskGrenadesData*)_from;
	taskGrenadesData *to = new taskGrenadesData;
	*_to = to;

	// If this accounts for the number of grenades we're
	// expecting to add during forward-simulation, we can
	// avoid a slow reallocation.
	to->l.init(from->l.num + 10);
	to->l.addAll(&from->l);

	rangeconst(i, to->l.num) {
		taskGrenade* nade = &to->l[i];
		nade->b = (box*)nade->b->clone.ptr;
		nade->b->data = nade;
	}
}

static void destroy(void *_data) {
	taskGrenadesData *data = (taskGrenadesData*)_data;

	// No need to clean up our boxes.
	// During a game cleanup the velbox heirarchy will already be torn down,
	// and we only explicitly request a destroy after all grenades are gone.

	data->l.destroy();
	delete data;
}

// This assumes it is called before the task executes.
// Mostly this assumption is because I currently need it for
// the TDM scoring task, which runs before!
// If you need to clear them out after, they're already
// marked in the velbox tree, and it's a little different.
void taskGrenades_clear(void *_data) {
	taskGrenadesData *data = (taskGrenadesData*)_data;
	list<taskGrenade> &l = data->l;
	range(i, l.num) {
		velbox_reclaimDead(l[i].b);
	}
	l.num = 0;
}

void taskGrenades_draw(void *_data) {
	taskGrenadesData *data = (taskGrenadesData*)_data;
	int64_t r = RADIUS;
	range(i, data->l.num) {
		taskGrenade *nade = &data->l[i];
		drawBillboard(nade->oldPos, nade->pos, 1, 42.0/64, 0, 6.0/64, r);
	}

	reset3dTexScale();
}

void taskGrenades_add(gamestate *gs, offset p1, offset vel, box *parent, char team, u16 soundId) {
	void **tmp = singletonTaskEnd(gs, TSK_GRENADES);
	taskGrenadesData *tsk;
	if (*tmp == NULL) {
		*tmp = tsk = new taskGrenadesData;
		tsk->l.init(10);
	} else {
		tsk = (taskGrenadesData*)*tmp;
	}
	list<taskGrenade> &l = tsk->l;
	char moved = (l.num == l.max);
	taskGrenade *nade = &tsk->l.add();
	// I may think of a better way to do this eventually
	if (moved) {
		rangeconst(i, l.num - 1) {
			l[i].b->data = &l[i];
		}
	}

	range(i, 3) {
		nade->oldPos[i] = -1;
	}
	memcpy(nade->pos, p1, sizeof(offset));
	memcpy(nade->vel, vel, sizeof(offset));

	// Not really ever used, but prevents warnings from valgrind about `write()`ing uninitialized data
	memset(nade->rot, 0, sizeof(iquat));

	nade->type = T_PROJ;
	nade->bounces = 2;
	nade->team = team;
	nade->soundId = soundId;

	// a dead box with the right parent
	nade->b = velbox_alloc();
	nade->b->parent = parent;
}

void taskGrenades_define(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->copy = &copy;
	d->destroy = &destroy;
}
