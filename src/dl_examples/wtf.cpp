#include <stdio.h>

#include "../gamestate.h"
#include "../bctx.h"
#include "../dl.h"

extern "C" void lvlUpd(gamestate *gs) {
	bctx.resel();

	bctx.pos(-10000, 0, -10000);
	bctx.push();
	int64_t size_incr = 600;
	range(i, 4) {
		int64_t size = 1000 + size_incr*i;
		bctx.pos(
			size*2-size_incr,
			size_incr,
			size_incr
		);
		bctx.add(0, 4, size);
		bctx.push();
		bctx.pos(0, 0, -3800);
		bctx.add(0, 4, 1000);
		bctx.pop();
	}
	bctx.peek();
	//#add_here
	/*#1
	bctx.pos(var("x", look(3000)[0]), var("y", look(3000)[1]), var("z", look(3000)[2]));
	bctx.scale(var("scale", 1000));
	bctx.add(0, 4, 1000);
	bctx.peek();
	 */
}
