#include <stdio.h>

#include "../gamestate.h"
#include "../bctx.h"
#include "../dl.h"

extern "C" void lvlUpd(gamestate *gs) {
	bctx.resel();

	bctx.push();

	//#add_here

	/*#1
	gp();
	bctx.pos(pvar("pos", look(3000)));
	bctx.scale(var("scale", 1000));
	bctx.add(0, 4, 1000);
	bctx.peek();
	 */
}
