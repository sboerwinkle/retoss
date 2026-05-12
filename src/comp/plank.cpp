#include "../gamestate.h"
#include "../bctx.h"

void plank(int64_t w, int64_t h, int tex, int pad) {
	if (w >= h) h = w+1;
	int64_t span = h-w;
	int blocks = (span+w-1)/w + 1;
	// If we end with a large block,
	// adjust the end down a bit.
	// This may have a weird effect
	// if `h` is only a bit over `w`,
	// but that's fine I guess.
	int64_t shift = 0;
	if (blocks%2 == 0) {
		span -= pad/2;
		shift = -pad/2;
	}

	bctx.push();
	int denom = blocks-1;
	range(i, blocks) {
		bctx.pos(0, (i*2-denom)*span/denom+shift, 0);
		bctx.add(1, tex, w+pad*(i%2));
		bctx.peek();
	}

	// This whole affair could probable be optimized some
	bctx.pop();
}
