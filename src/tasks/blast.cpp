#include <math.h>
#include "../util.h"
#include "../random.h"

#include "../matrix.h"
#include "../gamestate.h"
#include "../bctx.h"
#include "../task.h"
#include "../serialize.h"

#include "blast.h"

/*
static tskBlastInstructions* mkInstr() {
	tskBlastInstructions *instr = (tskBlastInstructions*)malloc(sizeof(tskBlastInstructions));
	instr->refs = 1;
	instr->pts.init();
	return instr;
}
*/

static void initBlastBits(tskBlastData *data) {
	tskBlastBits *bb = new tskBlastBits;
	bb->bits.init();
	bb->refs = 1;

	data->bb = bb;
}

static void destroy(tskBlastBits *bb) {
	bb->bits.destroy();
	delete bb;
}

// This incr/decr are pretty standard by now,
// might template them?
static void incr(tskBlastBits *bb) {
	bb->refs++;
}
static void decr(tskBlastBits *bb) {
	bb->refs--;
	if (bb->refs) return;
	destroy(bb);
}

static char step(gamestate *_unused, void *_data) {
	// TODO!
	return 0;
}

/*
static void transInstructions(tskBlastInstructions *instr) {
	transItemCount(&instr->pts);
	rangeconst(i, instr->pts.num) {
		tskBlastPt *p = &instr->pts[i];
		transOffset(p->pos);
		transIquat(p->rot);
		trans32(&p->time);
	}
}
*/

static char trans(gamestate *gs, void **ptr) {
	if (seriz_reading) {
		*ptr = new tskBlastData;
	}
	tskBlastData *data = (tskBlastData*)*ptr;
	trans32(&data->time);
	trans32(&data->seed);
	trans64(&data->r);
	if (seriz_reading) {
		initBlastBits(data);
	}
	return 0;
}

// TODO
static void copy(void **_to, void *_from) {
	tskBlastData *from = (tskBlastData*)_from;
	tskBlastData *to = (tskBlastData*)malloc(sizeof(tskBlastData));
	*_to = to;

	*to = *from;
	incr(from->bb);
}

static void destroy(void *_data) {
	tskBlastData *data = (tskBlastData*)_data;
	decr(data->bb);
	delete data;
}

tskBlastData* tskBlast_create(gamestate *gs, buildCtx *b) {
	tskBlastData *data = new tskBlastData;
	addTask(gs, TSK_BLAST, data);

	data->time = gs->clock;
	data->seed = splitmix32(&gs->seed);
	data->r = b->transf.scale;
	initBlastBits(data);

	return data;
}

void defineTask_blast(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->copy = &copy;
	d->destroy = &destroy;
}
