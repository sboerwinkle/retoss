#include <stdio.h>

#include "../gamestate.h"
#include "../bctx.h"
#include "../dl.h"
#include "../random.h"

#include "../comp/house1.h"

static void island() {
	gp("0");

	bctx.push();
	bctx.pos(pvar("pos", (offset const){0, 0, -1600}));
	bctx.add(1, var("sprite", 7), var("size", 12800));

	bctx.pop();
	house1_center(var("wall", 4200), var("door", 4200));
}

static void rand_island(uint32_t seed) {
	gp("setup");
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

extern void lv_swarm(gamestate *gs) {
	bctx.reset(gs);
	//bctx.resel();

	bctx.push();

	gp("setup");
	bctx.pos(pvar("view", (offset const){67600, 0, 0}));
	island();
	gp("spawn");
	bctx.pos(pvar("pos", (offset const){0, 0, 800}));
	bctx.finalizeTranslate();
	if (var("do", 1)) {
		rangeconst(i, gs->players.num) {
			memcpy(gs->players[i].m.pos, bctx.transf.pos, sizeof(offset));
			memcpy(gs->players[i].vel, (int64_t const[]){0,0,0}, sizeof(offset));
		}
	}
	bctx.peek();
	gp("setup"); // Same gp again
	uint32_t seed = var("seed", 0);
	rangeconst(i, var("count", 64)) {
		rand_island(splitmix32(&seed));
	}

	//#add_here

	/*#1
	gp();
	bctx.peek();
	bctx.pos(pvar("pos", look(3000)));
	bctx.scale(var("scale", 1000));
	int32_t r[3] = {quatSin(var("yaw")), 0, 0};
	bctx.rot(r);
	bctx.add(0, 4, 1000);

	 */
}
