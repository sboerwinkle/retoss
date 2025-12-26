#include <stdio.h>

#include "../gamestate.h"
#include "../bctx.h"
#include "../dl.h"

extern "C" void lvlUpd(gamestate *gs) {
	bctx.resel();

	bctx.pos(0, 0, -10000);
	bctx.pos(var("x"), var("y"), var("z"));
	bctx.push();
	int w = var("w");
	int h = var("h");
	int step = var("step", 200);
	rangeconst(i, w) {
		rangeconst(j, h-i) {
			bctx.peek();
			bctx.pos(i*-2000, j*-2000, (i+j)*step);
			bctx.add(4, 1000);
		}
	}
	bctx.pop();
}
