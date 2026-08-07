#include "../gamestate.h"

#include "../bcast.h"
#include "../game.h"
#include "../game_gamestate.h"
#include "../graphics.h"
#include "../main.h"
#include "../player.h"
#include "../serialize.h"
#include "../tool.h"

#include "../tasks/blast.h"

#include "rifle.h"

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
	// Guns happen at the start of the step,
	// so we have to subtract 1 from the clock
	int32_t soundTime = gs->clock - 1;
	if (result && shotRule != 1) {
		if ((result->type & T_PLAYER)) {
			player *shootee = playerFromMover(result);
			int who = shootee - gs->players.items;
			// Player can only shoot so fast,
			// if I based soundId off the shooter (not shootee)
			// I probably wouldn't need to include the hits counter.
			uint32_t soundId =
				0xFF00'0100
				+ who * 0x1'0000
				+ shootee->hits;
			addPlayerSound(soundTime, who, soundId, 1);
			shootee->hits++;
			player_hitsCooldown(shootee);
		} else {
			uint32_t soundId =
				0xFF00'FF01
				+ (p - gs->players.items) * 0x1'0000;
			offset impact;
			range(i, 3) impact[i] = p->m.oldPos[i] + look[i] * time.numer / time.denom;
			offset v;
			// Todo: Doesn't account for if impact surface is rotating
			range(i, 3) v[i] = result->pos[i] - result->oldPos[i];
			addSound(soundTime, impact, v, soundId, 3);
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
	addPlayerSound(soundTime, who, soundId, 2);

	trail &tr = gs->trails.add();
	memcpy(tr.origin, p->m.oldPos, sizeof(offset));
	memcpy(tr.dir, look, sizeof(unitvec));
	tr.len = time.numer*FIXP/time.denom;
	tr.expiry = vb_now + TRAIL_LIFETIME;
}

static void draw(gamestate *gs, player *p, float y, toolInst *_data) {
	toolRifle &data = *(toolRifle*)_data;

	// We've got it on the "font" texture for now.
	// We double the resolution b/c we have to draw halves in some cases,
	// and need to split a pixel for that.
	selectTex2d(1, 128, 128);
	centeredGrid2d(256);
	y *= displayAreaBounds[1];
	if (data.cooldown) {
		// Split crosshair
		float distance = (data.cooldown - gfx_interpRatio)/2;
		// src coords, size, dest coords
		sprite2d(0, 10, 5, 10, -5-distance, y-5);
		sprite2d(5, 10, 5, 10,    distance, y-5);
	} else {
		// Could draw it as 2 halves in this case as well,
		// I'm just not sure if it might look funny b/c of
		// pixel nonsense.
		sprite2d(0, 10, 10, 10, -5, y-5);
	}
}

static void use(gamestate *gs, player *p, char input, toolInst *_data) {
	toolRifle &data = *(toolRifle*)_data;
	if (data.cooldown) data.cooldown--;
	if (input && !data.cooldown) {
		data.cooldown = 10;
		shoot(gs, p);
	}
}

static char trans(toolInst **_data) {
	if (seriz_reading) {
		*_data = (toolRifle*)malloc(sizeof(toolRifle));
	}
	toolRifle &data = *(toolRifle*)*_data;
	trans32(&data.cooldown);
	return 0;
}

static void copy(toolInst **_to, toolInst *_from) {
	*_to = (toolRifle*)malloc(sizeof(toolRifle));
	toolRifle &to = *(toolRifle*)*_to;
	toolRifle &from = *(toolRifle*)_from;
	to.cooldown = from.cooldown;
}

static void destroy(toolInst *data) {
	free(data);
}

void toolRifle_create(toolInst **_data) {
	*_data = (toolRifle*)malloc(sizeof(toolRifle));
	toolRifle &data = *(toolRifle*)*_data;
	data.defn = toolLookup(TOOL_RIFLE);
	data.cooldown = 0;
}

void toolRifle_define(toolDefn *defn) {
	defn->draw = draw;
	defn->use = use;
	defn->trans = trans;
	defn->copy = copy;
	defn->destroy = destroy;
}
