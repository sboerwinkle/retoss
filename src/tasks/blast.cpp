#include <math.h>
#include "../util.h"
#include "../random.h"

#include "../matrix.h"
#include "../gamestate.h"
#include "../graphics.h"
#include "../task.h"
#include "../serialize.h"

#include "blast.h"

struct tskBlastItem {
	offset v;
	int32_t dur;
	float spr_x, spr_y, spr_w;
};

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

/*
A sphere occupies only about 52% of its circumscribing cube.
However, it occupies over 60% of its circumscribing octahedron!
This means I totally didn't waste my time with this overcomplicated
rejection-sampling method.
*/
static int32_t getPt(uint32_t *seed, offset dest, int64_t r) {
	uint32_t r1 = splitmix32(seed);
	uint32_t r2 = splitmix32(seed);
	int32_t x = r1 & (FIXP-1);
	int32_t y = (r1/FIXP) & (FIXP-1);
	int32_t z = r2 & (FIXP-1);
	if (x+y+z > FIXP*3/2) {
		x -= FIXP;
		y -= FIXP;
		z -= FIXP;
	}
#define R 28377
	if ((uint32_t)(x*x + y*y + z*z) > R*R) {
		return -1;
	}
	if (r2 & FIXP) x *= -1;
	if (r2 & (2*FIXP)) y *= -1;
	dest[0] = x * r / R;
	dest[1] = y * r / R;
	dest[2] = z * r / R;
	return r2 / (4*FIXP);
#undef R
}

static void populateBlastBits(tskBlastBits *data) {
	uint32_t seed = data->seed;
	int64_t r = data->r;
	// We'll add particles for sparks and smoke.
	// Sparks tend to move faster!
	offset dest;
	range(i, 20) {
		int32_t x = getPt(&seed, dest, r);
		if (x == -1) continue;
		tskBlastItem &item = data->bits.add();
		item.spr_w = 4.0/64;
		item.spr_y = 0;
		item.spr_x = (27 + 5*(x%3)) / 64.0;
		item.dur = 2 + x*4/FIXP;
		range(j, 3) item.v[j] = dest[j] / item.dur;
	}
	range(i, 40) {
		int32_t x = getPt(&seed, dest, r);
		if (x == -1) continue;
		tskBlastItem &item = data->bits.add();
		item.spr_w = 3.0/64;
		item.spr_x = (20 + (x%4)) / 64.0;
		item.spr_y = ((x/4)%4) / 64.0;
		item.dur = 4 + x*12/FIXP;
		range(j, 3) item.v[j] = dest[j] / item.dur;
	}
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

static char step(gamestate *gs, void *_data) {
	tskBlastData *data = (tskBlastData*)_data;
	return gs->clock > data->bb->time+15;
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
	if (seriz_reading) {
		initBlastBits(data);
	}
	trans32(&data->bb->time);
	trans32(&data->bb->seed);
	trans64(&data->bb->r);
	transOffset(data->bb->pos);
	transOffset(data->bb->vel);
	if (seriz_reading) {
		populateBlastBits(data->bb);
	}
	return 0;
}

static void copy(void **_to, void *_from) {
	tskBlastData *from = (tskBlastData*)_from;
	tskBlastData *to = new tskBlastData;
	*_to = to;

	*to = *from;
	incr(from->bb);
}

static void destroy(void *_data) {
	tskBlastData *data = (tskBlastData*)_data;
	decr(data->bb);
	delete data;
}

tskBlastData* tskBlast_create(gamestate *gs, offset oldPos, offset vel) {
	tskBlastData *data = new tskBlastData;
	addTask(gs, TSK_BLAST, data);

	initBlastBits(data);
	data->bb->time = gs->clock - 1;
	data->bb->seed = splitmix32(&gs->seed);
	data->bb->r = 3000;
	memcpy(data->bb->pos, oldPos, sizeof(offset));
	memcpy(data->bb->vel, vel, sizeof(offset));
	populateBlastBits(data->bb);

	return data;
}

// TODO None of these calculations depend on interpRatio,
//      which means I'm wasting a lot of math.
//      Really I should put the calculated positions in
//      some "transient" fields that aren't serialized
//      or dup'd, and only run the calculations per-tick
//      if we're not the root state. Or better yet, add
//      some global flag so we can check if we're the
//      doing the step just before rendering, and only do
//      it then.
void tskBlast_draw(void *data, int32_t now) {
	tskBlastBits *bb = ((tskBlastData*)data)->bb;

	int32_t t2 = now - bb->time;
	int32_t t1 = t2-1;

	offset core_p1;
	range(i, 3) {
		core_p1[i] = bb->pos[i] + bb->vel[i]*t1;
	}
	offset p1, p2;

	rangeconst(iter, bb->bits.num) {
		tskBlastItem const &item = bb->bits[iter];

		if (t2 > item.dur) continue;

		range(i, 3) {
			p1[i] = core_p1[i] + item.v[i]*t1;
			p2[i] = p1[i] + bb->vel[i] + item.v[i];
		}

		// TODO We're always rendering from tex 1 here,
		//      maybe make texture selection its own call??
		drawBillboard(p1, p2, 1, item.spr_x, item.spr_y, item.spr_w, 600);
	}
	reset3dTexScale();
}

void defineTask_blast(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->copy = &copy;
	d->destroy = &destroy;
}
