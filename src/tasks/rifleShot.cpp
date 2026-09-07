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

static void shoot(gamestate *gs, player *p) {
	int shotRule;
	if (gs == rootState) shotRule = 2;
	else shotRule = shotPredictionRules[p != &gs->players[myPlayer]];

	if (!shotRule) return;
	// Todo: Surely we'll need this more often, right? Save it somewhere?
	unitvec look;
	iquat_apply(look, p->m.rot, ((unitvec const){0, FIXP, 0}));

	fraction const limit = {.numer = RIFLE_RANGE, .denom = FIXP};
	// Todo: I think this may be a bit sloppy at the moment, since a box that's a suitable
	//       parent for something that large will necessarily be bigger than we need.
	//       Might be able to improve this with a small velbox tweak involving `minParentR`.
	box *queryArea = velbox_findParent(p->prox, p->m.oldPos, p->vel, limit.numer);
	bcast_start(queryArea, look, p->m.oldPos);
	fraction time;
	mover *result;
	do {
		result = bcast(&time, look, p->m.oldPos);
	} while (result == &p->m);
	if (!result) {
		time = limit;
	} else if (limit.lt(time)) {
		time = limit;
		result = NULL;
	}

	if (result && shotRule != 1) {
		int32_t type = result->type & T_MASK;
		if (type == T_PLAYER) {
			player *shootee = playerFromMover(result);
			player_hit(gs, gs->clock, shootee, 1);
		} else if (type == T_PROJ) {
			taskRocket *rocket = rocketFromMover(result);
			rocket->live = 0;
		} else {
			uint32_t soundId =
				0xFF00'FF01
				+ (p - gs->players.items) * 0x1'0000;
			offset impact;
			range(i, 3) impact[i] = p->m.oldPos[i] + look[i] * time.numer / time.denom;
			offset v;
			// Todo: Doesn't account for if impact surface is rotating
			range(i, 3) v[i] = result->pos[i] - result->oldPos[i];
			addSound(gs->clock, impact, v, soundId, SND_TAP);
			tskBlast_create(gs, impact, v, 3000, 20, 40);

			solid *s = solidFromMover(result);
			list<mover*> blastMovers;
			blastMovers.init();
			// Currently the blast radius is 3k units, need to write this down somewhere
			velbox_query(s->m.b, impact, v, 3000, &blastMovers);
			rangeconst(iter, blastMovers.num) {
				mover *m = blastMovers[iter];
				if (!(m->type & T_PLAYER)) continue;
				player *blastee = playerFromMover(m);
				offset d;
				range(i, 3) d[i] = blastee->m.oldPos[i] - impact[i];
				// This should almost always be small enough to safely square,
				// but for relativistic players maybe not.
				int64_t mg = mag(d);
				if (mg > 3000 || !mg) continue;
				range(i, 3) blastee->vel[i] += d[i]*600/mg;
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
	memcpy(tr.origin, p->m.oldPos, sizeof(offset));
	memcpy(tr.dir, look, sizeof(unitvec));
	tr.len = time.numer*FIXP/time.denom;
	tr.expiry = gs->clock + TRAIL_LIFETIME;
}

static char step(gamestate *gs, void *data) {
	shoot(gs, (player*)data);
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
