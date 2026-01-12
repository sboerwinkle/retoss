#include <math.h>

#include "list.h"
#include "matrix.h"

#include "gamestate.h"

#include "bctx.h"

buildCtx bctx;

void buildCtx::reset(gamestate *gs2) {
	gs = gs2;
	prevBox = gs->vb_root;
	transformStack.num = 0;

	transf.rot[0] = FIXP;
	range(i, 3) {
		transf.pos[i] = 0;
		transf.rot[i+1] = 0;
	}
	transf.scale = 1000; // TODO make this a constant somewhere

	selecting = 0;
}

void buildCtx::complete() {
	gs = NULL;
}

void buildCtx::push() {
	transformStack.add(transf);
}

void buildCtx::pop() {
	if (!transformStack.num) {
		puts("Illegal buildCtx.pop()!");
		return;
	}
	transformStack.num--;
	transf = transformStack[transformStack.num];
}

void buildCtx::peek() {
	if (!transformStack.num) {
		puts("Illegal buildCtx.peek()!");
		return;
	}
	transf = transformStack[transformStack.num-1];
}

void buildCtx::pos(int64_t x, int64_t y, int64_t z) {
	offset tmp = {x, y, z};
	pos(tmp);
}

void buildCtx::pos(offset const v) {
	offset v2, v3;
	range(i, 3) v2[i] = transf.scale * v[i] / 1000;
	iquat_applySm(v3, transf.rot, v2);
	range(i, 3) transf.pos[i] += v3[i];
}

// Todo: Is this okay or backwards?
void buildCtx::rotQuat(iquat const r) {
	iquat_rotateBy(transf.rot, r);
	iquat_norm(transf.rot);
}

void buildCtx::rot(int32_t const rotParams[3]) {
	int32_t yawSin   = rotParams[0];
	int32_t pitchSin = rotParams[1];
	int32_t rollSin  = rotParams[2];
	if (
		yawSin > FIXP ||
		yawSin < -FIXP ||
		pitchSin > FIXP ||
		pitchSin < -FIXP ||
		rollSin > FIXP ||
		rollSin < -FIXP
	) {
		// TODO: Put this in a `#ifndef NODEBUG` and `exit(1)`
		//       once we have better validations other places
		puts("Bad yaw/pitch/roll to `rot()`");
		return;
	}
	// These "Sin" values are the sine (* FIXP) of half the rotation angle.
	// We will need the cosine as well.
	// This works fine so long as our rotations are always within +/- 180 deg,
	// which means we're using the sine/cosine of something within +/- 90 deg
	// (where the cosine is always positive)

	// I don't want to do trig here, since level generation should (ideally) work the same
	// across clients. I don't trust all computers to do trig exactly the same.
	// However, I feel more comfortable about them finding the correct square root of an integer
	// (at least, after truncation to an integer), so we do compute the cosines here.
	// Note that I don't actually *know* if trig is bad and sqrt is good lol
	int32_t yawCos   = sqrt(FIXP*FIXP-yawSin*yawSin);
	int32_t pitchCos = sqrt(FIXP*FIXP-pitchSin*pitchSin);
	int32_t rollCos  = sqrt(FIXP*FIXP-rollSin*rollSin);

	iquat a,b,c;

	a[0] = yawCos;
	a[1] = 0;
	a[2] = 0;
	a[3] = yawSin;

	b[0] = pitchCos;
	b[1] = pitchSin;
	b[2] = 0;
	b[3] = 0;

	iquat_mult(c, a, b);
	// `c` is now the result of composing the first 2 rotations,
	// but we still have one more to compose. We can re-use `a` and `b` now,
	// however.

	b[0] = rollCos;
	b[1] = 0;
	b[2] = rollSin;
	//b[3] still 0

	iquat_mult(a, c, b);

	// We actually haven't normalized at all here.
	// We rely on `rotQuat`'s normalization to
	// be enough - which it probably is?
	rotQuat(a);
}

void buildCtx::scale(int32_t scale) {
	// This 1000 is pretty arbitrary, maybe I'll change it later?
	// We use FIXP for 'precise' math, but this is more human-readable
	// (for level design)
	transf.scale = transf.scale*scale/1000;
}

void buildCtx::resel() {
	while (gs->selection.num) {
		rmSolid(gs, gs->selection[0]);
	}
	selecting = 1;
}

void buildCtx::add(int32_t shape, int32_t tex, int64_t size) {
	int64_t finalSize = size*transf.scale/1000;
	if (finalSize <= 0) {
		printf("bctx: Bad object size %ld\n", finalSize);
		return;
	}
	solid *s = addSolid(gs, prevBox, transf.pos[0], transf.pos[1], transf.pos[2], finalSize, shape, tex);
	memcpy(s->rot, transf.rot, sizeof(transf.rot));
	gs->selection.add(s);
	prevBox = s->b;
	if (solidCallback) (*solidCallback)(s);
}

void bctx_init() {
	bctx.transformStack.init();
	bctx.gs = NULL;
	bctx.solidCallback = NULL;
}
void bctx_destroy() {
	bctx.transformStack.destroy();
}
