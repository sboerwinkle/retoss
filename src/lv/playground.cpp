#include <stdio.h>

#include "../gamestate.h"
#include "../bctx.h"
#include "../dl.h"

extern void lv_playground(gamestate *gs) {
	bctx.reset(gs);
	bctx.scale(2000);
	//bctx.resel();

	bctx.push();
	gp("ground");
	bctx.pos(pvar("pos", (offset const){0, 0, -4000}));
	bctx.add(var("shape", 1), 4, var("scale", 8000));

	gp("funnel");
	bctx.push();
	bctx.pos(pvar("pos", (offset const){-11970, 0, -2200}));
	range(i, 4) {
		bctx.push();
		bctx.pos(pvar("p1", (offset const){0, 1500, 0}));
		bctx.rot(rvar("rot", (int32_t const[]){0, 12540, 0}));
		bctx.add(var("shape", 1), 4, var("scale", 4000));
		bctx.pop();
		bctx.rot(rvar("r2", (int32_t const[]){23170, 0, 0}));
	}
	bctx.pop();

	gp("ground.2");
	bctx.pos(pvar("pos", (offset const){0, -16000, 0}));
	bctx.add(var("shape", 1), 4, var("scale", 8000));
	bctx.push();
	gp("7");
	bctx.pos(pvar("pos", (offset const){-2900, 3600, 1690}));
	bctx.rot(rvar("rot", (int32_t const[]){30274, 8481, 0}));
	range(i, var("num", 6)) {
		bctx.push();
		// TODO really gotta get better about this...
		range(j, i+1) bctx.rot(rvar("shift_r", (int32_t const[]){0, 0, 2571}));
		bctx.add(var("shape", 2), var("spr", 5), var("scale", 2000));
		bctx.pop();
		bctx.pos(pvar("shift", (offset const){-1500, 0, 0}));
	}
	//#add_here
	bctx.pop();

	// Other b.s.
	bctx.pop();
	bctx.pos(-10000, 0, -10000);
	bctx.push();

	gp("4");
	bctx.pos(pvar("pos", (offset const){12200, 1800, 10000}));
	bctx.rot(rvar("rot", (int32_t const[]){-12540, 0, 0}));
	bctx.add(var("shape", 0), 4, var("scale", 1000));
	bctx.peek();
	gp("6");
	bctx.pos(pvar("pos", (offset const){4038, -733, 9934}));
	bctx.scale(var("scale", 1000));
	bctx.add(var("shape", 0), 4, 1000);
	bctx.peek();
	/*#1
	gp();
	bctx.peek();
	bctx.pos(pvar("pos", look(3000)));
	bctx.add(var("shape"), 4, var("scale", 1000));
	 */
}
