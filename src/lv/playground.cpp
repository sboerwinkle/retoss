#include <stdio.h>

#include "../gamestate.h"
#include "../bctx.h"
#include "../dl.h"

static void rubblePortion();

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

	bctx.reset(gs);
	rubblePortion();
}

static void rubblePortion() {
	//bctx.resel();

	bctx.push();
	gp("0");
	bctx.pos(pvar("pos", (offset const){32000, 0, -8000}));
	bctx.add(var("shape", 1), 5, var("scale", 16000));
	bctx.peek();

	gp("arch");
	bctx.pos(pvar("pos", (offset const){26000, -7000, -3000}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.push();
	gp("a.1");
	bctx.pos(pvar("pos", (offset const){0, 0, 0}));
	bctx.add(var("shape", 1), 5, var("scale", 3000));
	bctx.peek();
	gp("a.2");
	bctx.pos(pvar("pos", (offset const){9000, 0, 0}));
	bctx.add(var("shape", 1), 5, var("scale", 3000));
	bctx.peek();
	gp("a.3");
	bctx.pos(pvar("pos", (offset const){4500, 1500, 0}));
	bctx.add(var("shape", 1), 5, var("scale", 1500));
	bctx.peek();
	// End of arch
	bctx.pop();
	bctx.peek();

	gp("1");
	bctx.pos(pvar("pos", (offset const){25100, -5000, -4400}));
	bctx.add(var("shape", 0), 4, var("scale", 1600));
	bctx.peek();
	gp("2");
	bctx.pos(pvar("pos", (offset const){27588, -5210, -4780}));
	bctx.rot(rvar("rot", (int32_t const[]){-5126, -12540, -3709}));
	bctx.add(var("shape", 2), 4, var("scale", 2000));
	bctx.peek();
	gp("3");
	bctx.pos(pvar("pos", (offset const){24100, -4000, -2200}));
	bctx.add(var("shape", 0), 4, var("scale", 600));
	bctx.peek();
	gp("w1");
	bctx.pos(pvar("pos", (offset const){22700, -3800, -3000}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 23170, 0}));
	bctx.add(var("shape", 1), 4, var("scale", 3000));
	bctx.peek();
	gp("w2");
	bctx.pos(pvar("pos", (offset const){24700, -200, -3600}));
	bctx.rot(rvar("rot", (int32_t const[]){-32488, 23170, 0}));
	bctx.add(var("shape", 1), 4, var("scale", 2400));
	bctx.peek();
	gp("w3");
	bctx.pos(pvar("pos", (offset const){34800, -400, -5000}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 0, 0}));
	bctx.add(var("shape", 2), 4, var("scale", 8100));
	bctx.peek();
	gp("rubble1");
	bctx.pos(pvar("pos", (offset const){27800, 7000, -4300}));
	bctx.add(var("shape", 0), 4, var("scale", 1700));
	bctx.peek();
	gp("rubble2");
	bctx.pos(pvar("pos", (offset const){33500, 4600, -5300}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(var("shape", 2), 4, var("scale", 12600));
	bctx.peek();
	gp("rubble_wall");
	bctx.pos(pvar("pos", (offset const){26800, 10100, -4800}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 16384, 0}));
	bctx.add(var("shape", 1), 4, var("scale", 1400));
	bctx.peek();
}
