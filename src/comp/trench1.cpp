#include "../bctx.h"
#include "../dl.h"

void trench1() {
	pushVarIgnore();
	bctx.push();

	gp("0");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){0, 0, 1400}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, -12540}));
	bctx.add(var("shape", 2), var("sprite", 5), var("scale", 8000));

	gp("u");
	int32_t const *r = rvar("rot", (int32_t const[]){0, 0, 16384});

	gp("u.0");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){400, 5600, 2700}));
	bctx.rot(r);
	bctx.add(var("shape", 2), var("sprite", 5), var("scale", 2400));

	gp("u.1");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){400, -5600, 2700}));
	bctx.rot(r);
	bctx.add(var("shape", 2), var("sprite", 5), var("scale", 2400));

	gp("u.2");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){500, 0, 2600}));
	bctx.rot(r);
	bctx.add(var("shape", 2), var("sprite", 5), var("scale", 3200));

	gp("2.0");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){-2100, 2400, 700}));
	bctx.add(var("shape", 2), var("sprite", 5), var("scale", 5600));

	gp("2.1");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){-2100, -2200, 700}));
	bctx.add(var("shape", 2), var("sprite", 5), var("scale", 5800));

	bctx.pop();
	popVarIgnore();
}
