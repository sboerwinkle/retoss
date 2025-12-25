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
void buildCtx::rot(iquat const r) {
	iquat_rotateBy(transf.rot, r);
	iquat_norm(transf.rot);
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

void buildCtx::add(int32_t tex, int64_t size) {
	solid *s = addSolid(gs, prevBox, transf.pos[0], transf.pos[1], transf.pos[2], size*transf.scale/1000, tex);
	memcpy(s->rot, transf.rot, sizeof(transf.rot));
	gs->selection.add(s);
	prevBox = s->b;
}

void bctx_init() {
	bctx.transformStack.init();
	bctx.gs = NULL;
}
void bctx_destroy() {
	bctx.transformStack.destroy();
}
