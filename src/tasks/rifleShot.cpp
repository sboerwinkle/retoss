#include "../util.h"

#include "../gamestate.h"

#include "../bcast.h"
#include "../game.h"
#include "../game_gamestate.h"
#include "../main.h"
#include "../task.h"
#include "../serialize.h"
#include "../player.h"

#include "../tools/rifle.h"

#include "blast.h"
#include "rocket.h"

// This is a center-to-center radius,
// but that's not a problem unless we
// blast more than people.
static const int BLAST_RADIUS = 2000;
static fraction const limit = {.numer = RIFLE_RANGE, .denom = FIXP};

static mover *checkShot(box *queryArea, unitvec const look, int64_t *shotOrigin, mover *srcMover, fraction *time) {
	bcast_start(queryArea, look, shotOrigin);
	mover *result;
	do {
		result = bcast(time, look, shotOrigin);
		// For now, grenades and rockets can't be told apart and have different structures,
		// so we can't really shoot them even if we wanted to.
	} while (result == srcMover || (result && (result->type & T_PROJ)));
	if (!result) {
		*time = limit;
	} else if (limit.lt(*time)) {
		*time = limit;
		result = NULL;
	}
	return result;
}

static char priorityHit(player *p, mover *hit) {
	return (
		hit
		&& T_PLAYER == (hit->type & T_MASK)
		&& playerFromMover(hit)->team != p->team
	);
}

static void shoot(gamestate *gs, player *p, char octopus) {
	int shotRule;
	if (gs == rootState) shotRule = 2;
	else shotRule = shotPredictionRules[p != &gs->players[myPlayer]];

	if (!shotRule) return;
	// Todo: Surely we'll need this more often, right? Save it somewhere?
	unitvec look;
	iquat_apply(look, p->m.rot, ((unitvec const){0, FIXP, 0}));

	// Todo: I think this may be a bit sloppy at the moment, since a box that's a suitable
	//       parent for something that large will necessarily be bigger than we need.
	//       Might be able to improve this with a small velbox tweak involving `minParentR`.
	// Add player radius for octopus arms
	box *queryArea = velbox_findParent(p->prox, p->m.oldPos, p->vel, RIFLE_RANGE + PLAYER_SHAPE_RADIUS);

	int64_t *shotOrigin = p->m.oldPos;
	fraction time;
	mover *result = checkShot(queryArea, look, shotOrigin, &p->m, &time);

	offset armOrigin;
	if (octopus && !priorityHit(p, result)) {
		offset armPositions[4];
		iquat_apply64(armPositions[0], p->m.rot, ((offset const){PLAYER_SHAPE_RADIUS, 0, 0}));
		iquat_apply64(armPositions[1], p->m.rot, ((offset const){0, 0, PLAYER_SHAPE_RADIUS}));
		range(i, 3) {
			armPositions[2][i] = -armPositions[0][i];
			armPositions[3][i] = -armPositions[1][i];
		}

		mover *armResult;
		fraction armTime;
		// Check each arm to see if they're better.
		range(arm, 4) {
			range(i, 3) armOrigin[i] = shotOrigin[i] + armPositions[arm][i];
			armResult = checkShot(queryArea, look, armOrigin, &p->m, &armTime);
			if (priorityHit(p, armResult)) {
				result = armResult;
				time = armTime;
				shotOrigin = armOrigin;
				break;
			}
		}
	}

	if (result && shotRule != 1) {
		int32_t type = result->type & T_MASK;
		if (type == T_PLAYER) {
			player *shootee = playerFromMover(result);
			player_hit(gs, gs->clock, shootee, 1);
		} else {
			uint32_t soundId =
				0xFF00'FF01
				+ (p - gs->players.items) * 0x1'0000;
			offset impact;
			range(i, 3) impact[i] = shotOrigin[i] + look[i] * time.numer / time.denom;
			offset v;
			// Todo: Doesn't account for if impact surface is rotating
			range(i, 3) v[i] = result->pos[i] - result->oldPos[i];
			addSound(gs->clock, impact, v, soundId, SND_TAP);
			tskBlast_create(gs, impact, v, BLAST_RADIUS, 0, 10);

			solid *s = solidFromMover(result);
			list<mover*> blastMovers;
			blastMovers.init();
			// Currently the blast radius is 3k units, need to write this down somewhere
			velbox_query(s->m.b, impact, v, BLAST_RADIUS, &blastMovers);
			rangeconst(iter, blastMovers.num) {
				mover *m = blastMovers[iter];
				if (!(m->type & T_PLAYER)) continue;
				player *blastee = playerFromMover(m);
				offset d;
				range(i, 3) d[i] = blastee->m.oldPos[i] - impact[i];
				// This should almost always be small enough to safely square,
				// but for relativistic players maybe not.
				int64_t mg = mag(d);
				if (mg > BLAST_RADIUS || !mg) continue;
				range(i, 3) blastee->vel[i] += d[i]*200/mg;
			}
			blastMovers.destroy();
		}
	}

	int who = p - gs->players.items;
	uint32_t soundId =
		0xFF00'FF00
		+ who * 0x1'0000;
	addPlayerSound(gs->clock, who, soundId, SND_POP);

	trail &tr = gs->trails.add();
	memcpy(tr.origin, shotOrigin, sizeof(offset));
	memcpy(tr.dir, look, sizeof(unitvec));
	tr.len = time.numer*FIXP/time.denom;
	tr.expiry = gs->clock + TRAIL_LIFETIME;
}

static char step(gamestate *gs, void *data) {
	shoot(gs, (player*)data, 0);
	return 1;
}

static char stepOct(gamestate *gs, void *data) {
	shoot(gs, (player*)data, 1);
	return 1;
}

static char trans(gamestate *gs, void **ptr) {
	if (seriz_error()) {
		puts("tasks/rifleShot should never be serialized!");
	}
	return 1;
}

static void copy(void **to, void *from) {
	puts("tasks/rifleShot should never be copied!");
	*to = NULL;
}

static void destroy(void *data) {
	// No-op, `data` is not ours to free.
}

void taskRifleShot_define(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->copy = &copy;
	d->destroy = &destroy;
}

void taskRifleShot_defineOct(taskDefn *d) {
	d->step = &stepOct;
	d->trans = &trans;
	d->copy = &copy;
	d->destroy = &destroy;
}
