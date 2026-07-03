#include <math.h>
#include "../util.h"

#include "../matrix.h"
#include "../gamestate.h"
#include "../bctx.h"
#include "../task.h"
#include "../serialize.h"

#include "dynamics.h"

static void step(gamestate *gs, void *_data) {
	tskDynamicsData *data = (tskDynamicsData*)_data;

	memcpy(data->s.m.oldPos, data->s.m.pos, sizeof(offset));
	memcpy(data->s.m.oldRot, data->s.m.rot, sizeof(iquat));

	// Can ignore vel/rvel for now.

	//printf("[%d, %d)", data->s.b->start, data->s.b->end);
	//printf("* %d", gs->clock);
	box *p = data->s.b->parent;
	// For now these are 1-frame only boxes,
	// so we know it's dead!
	velbox_reclaimDead(data->s.b);
	//puts(".");
	solidPutVb(&data->s, p, 1);
}

static char trans(gamestate *gs, void **ptr) {
	if (seriz_reading) {
		*ptr = malloc(sizeof(tskDynamicsData));
	}
	tskDynamicsData *data = (tskDynamicsData*)*ptr;

	transSolid(&data->s);

	transOffset(data->vel);
	transIquat(data->rvel);
	return 0;
}

static void copy(void **_to, void *_from) {
	tskDynamicsData *from = (tskDynamicsData*)_from;
	tskDynamicsData *to = (tskDynamicsData*)malloc(sizeof(tskDynamicsData));
	*_to = to;

	cpSolid(&to->s, &from->s);
	memcpy(to->vel, from->vel, sizeof(offset));
	memcpy(to->rvel, from->rvel, sizeof(iquat));
	//printf("[%d, %d)...\n", to->s.b->start, to->s.b->end);
}

static void destroy(void *_data) {
	tskDynamicsData *data = (tskDynamicsData*)_data;

	// Nope, velbox heirarchy is torn down by now.
	//velbox_remove(data->s.b);

	free(data);
}

void tskDynamics_create(gamestate *gs, buildCtx *c, offset const vel, iquat const rvel) {
	taskInstance &task = gs->tasks.add();
	task.defn = taskLookup(TSK_DYNAMICS);
	tskDynamicsData *data = (tskDynamicsData*)malloc(sizeof(tskDynamicsData));
	task.data = data;

	memcpy(data->s.m.pos, c->transf.pos, sizeof(offset));
	memcpy(data->s.m.rot, c->transf.rot, sizeof(iquat));
	data->s.m.type = 0; // Means "cube" I guess
	data->s.r = 800;
	data->s.tex = 8;

	memcpy(data->vel, vel, sizeof(offset));
	memcpy(data->rvel, rvel, sizeof(iquat));

	range(i, 3) data->s.m.oldPos[i] = data->s.m.pos[i] - data->vel[i];
	vb_now = gs->clock; // TODO oof
	solidPutVb(&data->s, gs->vb_root, 1);
	//printf("[%d, %d)", data->s.b->start, data->s.b->end);
	//printf("^ %d\n", gs->clock);
	// Unlike `oldPos`, this isn't used in `solidPutVb`,
	// so we don't really care what it is yet.
	memset(data->s.m.oldRot, 0, sizeof(iquat));
}

void defineTask_dynamics(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->copy = &copy;
	d->destroy = &destroy;
}
