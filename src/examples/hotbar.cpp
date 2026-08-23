#include <stdio.h>

#include "../gamestate.h"
#include "../bctx.h"
#include "../dl_helpers.h"

extern "C" void lvlUpd(gamestate *gs) {
	bctx.resel();

	bctx.push();

	//#add_here

	/*#1
	gp();
	bctx.pos(pvar("pos", look(3000)));
	bctx.rot(rvar("rot"));
	bctx.add(var("shape"), var("tex", 4), var("scale", 1000));
	bctx.peek();
	 */
}
