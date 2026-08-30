#include <stdio.h>

#include "../gamestate.h"
#include "../bctx.h"
#include "../dl_helpers.h"
#include "../random.h"

#include "../comp/house1.h"
#include "../tasks/killPlane.h"

static void island() {
	gp("0");

	bctx.push();
	bctx.pos(pvar("pos", (offset const){0, 0, -1600}));
	bctx.add(1, var("sprite", 7), var("size", 12800));

	bctx.pop();
	house1_center(var("wall", 4200), var("door", 4200));
}

static void rand_island(uint32_t seed) {
	gp("");
	offset p;
	range(i, 3) {
		p[i] = ((int32_t)splitmix32(&seed) + INT32_MIN) * var("dist", 40000);
		p[i] = p[i]>>31;
	}
	bctx.push();
	bctx.pos(p);
	island();
	bctx.pop();
}

//#name lv_swarm
extern "C" void lv_swarm(gamestate *gs) {
	prepareGamestateForLoad(gs, -1);
	coreSetup(gs);
	bctx.reset(gs);

	taskKillPlane_create(gs, -64000);

	bctx.push();

	bctx.pos(pvar("island0", (offset const){67600, 0, 0}));
	island();
	gp("");

	bctx.pos(pvar("start", (offset const){0, 0, 800}));
	bctx.finalizeTranslate();
	if (var("respawn", 1)) {
		rangeconst(i, gs->players.num) {
			memcpy(gs->players[i].m.pos, bctx.transf.pos, sizeof(offset));
			memcpy(gs->players[i].vel, (int64_t const[]){0,0,0}, sizeof(offset));
		}
	}
	bctx.peek();

	uint32_t seed = var("seed", 0);
	rangeconst(i, var("count", 64)) {
		rand_island(splitmix32(&seed));
	}
}
