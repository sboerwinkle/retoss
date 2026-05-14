#include <math.h>
#include "../util.h"

#include "../matrix.h"
#include "../gamestate.h"
#include "../task.h"
#include "../serialize.h"

#include "rails.h"

static tskRailsInstructions* mkInstr() {
	tskRailsInstructions *instr = (tskRailsInstructions*)malloc(sizeof(tskRailsInstructions));
	instr->refs = 1;
	instr->pts.init();
	return instr;
}

static void incr(tskRailsInstructions *instr) {
	instr->refs++;
}

static void decr(tskRailsInstructions *instr) {
	instr->refs--;
	if (instr->refs) return;
	instr->pts.destroy();
	free(instr);
}

// `tskRails_timeHelper` currently depends on the gamestate being unused
static void step(gamestate *_unused, void *_data) {
	tskRailsData *data = (tskRailsData*)_data;
	int numPts = data->instr->pts.num;
	if (!numPts) return;
	constelInst *ci = data->ci;

	int32_t time = data->time + 1;
	int32_t ic = data->ic;
	int32_t next = (ic+1)%numPts;
	tskRailsPt &p1 = data->instr->pts[ic];
	tskRailsPt &p2 = data->instr->pts[next];
	int32_t duration = p2.time;
	// Safety checks; shouldn't happen.
	if (duration < 1) duration = 1;
	if (time > duration) time = duration;

	range(i, 3) ci->m.oldPos[i] = ci->m.pos[i];
	range(i, 4) ci->m.oldRot[i] = ci->m.rot[i];

	char rotChanged = 0;
	range(i, 4) rotChanged |= (p1.rot[i] != p2.rot[i]);
	if (!rotChanged) {
		ci->duration = duration - time + 1;
	} else if (time != duration) {
		ci->duration = 1;
		// Compute rotation to get from orientation A to orientation B
		// (this is re-computed a lot - kind of a waste)
		iquat r1Inv;
		r1Inv[0] =  p1.rot[0];
		r1Inv[1] = -p1.rot[1];
		r1Inv[2] = -p1.rot[2];
		r1Inv[3] = -p1.rot[3];
		iquat difference;
		iquat_mult(difference, p2.rot, r1Inv);

		// I'll be honest - I wrote most of this without knowing
		// that the `w` in a quaternion could be negative. Turns
		// out the interpolation gets weird in that case, but it
		// looks like you can keep the "same" quaternion even if
		// you flip the signs on everything!
		if (difference[0] < 0) range(i, 4) difference[i] *= -1;

		// Scale that rotation by the fraction `time/duration`.
		// I feel like I could probably improve this (do I need 2 `sqrt`s?),
		// but we'll roll with this for now.
		int32_t sinFull = sqrt(difference[1]*difference[1]+difference[2]*difference[2]+difference[3]*difference[3]);
		int32_t sinInterp = shittySin(shittyASin(sinFull)*time/duration);
		range(i, 3) difference[i+1] = difference[i+1]*sinInterp/sinFull;
		// We just take difference[0] to be "whatever's left",
		// so it should be pretty normalized.
		difference[0] = sqrt(
			FIXP*FIXP
			-difference[1]*difference[1]
			-difference[2]*difference[2]
			-difference[3]*difference[3]
		);

		// Re-apply scaled rotation to get our interpolated orientation.
		iquat_mult(ci->m.rot, difference, p1.rot);
	} else {
		// Could use the above code for this case,
		// but then there's integer rounding concerns and it's just a mess.
		// For now this feels like the smaller headache.
		range(i, 4) ci->m.rot[i] = p2.rot[i];
	}

	// Todo: The velbox drifts at a constant speed,
	//       but we interp our pos each time, so they could be out of sync!
	offset delta;
	range(i, 3) delta[i] = p2.pos[i] - p1.pos[i];
	range(i, 3) ci->m.pos[i] = p1.pos[i] + delta[i]*time/duration;

	if (time >= p2.time) {
		data->ic = next;
		time = 0;
	}
	data->time = time;
}

static void transInstructions(tskRailsInstructions *instr) {
	transItemCount(&instr->pts);
	rangeconst(i, instr->pts.num) {
		tskRailsPt *p = &instr->pts[i];
		transOffset(p->pos);
		transIquat(p->rot);
		trans32(&p->time);
	}
}

static char trans(gamestate *gs, void **ptr) {
	if (seriz_reading) {
		*ptr = malloc(sizeof(tskRailsData));
	}
	tskRailsData *data = (tskRailsData*)*ptr;
	trans32(&data->ic);
	trans32(&data->time);
	if (seriz_reading) {
		int ix = read32();
		if (ix < 0 || ix >= gs->constels.num) {
			if (seriz_error()) {
				printf("Have %d constels, but taskRails got index %d?\n", gs->constels.num, ix);
			}
			free(*ptr);
			return 1;
		}
		data->ci = gs->constels[ix];
		// We assume each "rails" task has a different set of instructions.
		// Todo: If I ever get nicer helpers for such things, could support
		//       shared instructions here?
		data->instr = mkInstr();
	} else {
		write32(data->ci->clone.idx);
	}
	transInstructions(data->instr);
	if (seriz_reading && data->instr->pts.num) {
		data->ic = data->ic % data->instr->pts.num;
	}
	return 0;
}

static void copy(void **_to, void *_from) {
	tskRailsData *from = (tskRailsData*)_from;
	tskRailsData *to = (tskRailsData*)malloc(sizeof(tskRailsData));
	*_to = to;

	to->instr = from->instr;
	to->ic = from->ic;
	to->time = from->time;
	to->ci = (constelInst*)from->ci->clone.ptr;
	incr(to->instr);
}

static void destroy(void *_data) {
	tskRailsData *data = (tskRailsData*)_data;
	decr(data->instr);
	free(data);
}

// This is intended for "mostly correct" time (like while messing with the editor),
// it won't handle really really large numbers efficiently.
void tskRails_timeHelper(tskRailsData *data) {
	list<tskRailsPt> &pts = data->instr->pts;

	// Avoids some infinite loop issues when editing and route is fresh
	int32_t sum = 0;
	rangeconst(i, pts.num) {
		sum += pts[i].time;
	}
	if (sum <= 0) return;

	while (1) {
		int next = (data->ic+1)%pts.num;
		if (data->time < pts[next].time) break;
		data->time -= pts[next].time;
		data->ic = next;
	}

	// Roll `time` back one step.
	// I'm not sure if negative time is okay,
	// so we dance around a bit (`while` loop instead of `if`) to avoid
	// any zero-time instructions (which might be present while editing).
	while (data->time == 0) {
		data->time = pts[data->ic].time;
		data->ic = (data->ic+pts.num-1) % pts.num;
	}
	data->time--;
	// Steps that don't rotate don't bother updating the rotation,
	// so we do that here in case we're in the middle of some such step.
	memcpy(data->ci->m.rot, pts[data->ic].rot, sizeof(iquat));
	// And finally run one step (undoing the "one step backwards"),
	// this has all the usual logic about interping positions / rotations
	// so the things spawn in without snapping even if in the middle
	// of their instructions!
	step(NULL, data);
	// This happens at a different time than it would during a normal step,
	// but I can't think of any way that would break?
	// Only problem is that the `constelInst` might be out of sync with its
	// solids and/or velboxes, but hopefully nothing cares?
	// Worst case, I can make this function mandatory, and hold off adding
	// `data->ci` to `gs` until here. Would require changing how it's built I guess.
}

tskRailsData* tskRails_create(gamestate *gs, constelInst *ci) {
	taskInstance &task = gs->tasks.add();
	task.defn = taskLookup(TSK_RAILS);
	tskRailsData *data = (tskRailsData*)malloc(sizeof(tskRailsData));
	task.data = data;

	data->ic = data->time = 0;
	data->ci = ci;
	data->instr = mkInstr();

	return data;
}

void defineTask_rails(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->copy = &copy;
	d->destroy = &destroy;
}
