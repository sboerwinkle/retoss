#include <stdio.h>

#include "../gamestate.h"
#include "../bctx.h"
#include "../dl.h"

extern "C" void lvlUpd(gamestate *gs) {
	bctx.resel();

	bctx.pos(-10000, 0, -10000);
	bctx.push();

	bctx.pos(pvar("pos", (offset const){12200, 1800, 10000}));
	bctx.rot(rvar("rot", (int32_t const[]){-12275, 286, 32767}));
	bctx.add(0, 4, var("scale", 1000));
	bctx.peek();
	//#add_here
	/*#1
	bctx.pos(pvar("pos", look(3000)));
	bctx.scale(var("scale", 1000));
	bctx.add(0, 4, 1000);
	bctx.peek();
	 */
}
