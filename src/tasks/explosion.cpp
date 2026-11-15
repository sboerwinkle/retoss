#include "../util.h"

#include "../gamestate.h"

#include "../game.h"
#include "../task.h"
#include "../serialize.h"
#include "../player.h"

#include "grenades.h"

#include "explosion.h"

static char step(gamestate *gs, void *_data) {
	taskExplosionData *data = (taskExplosionData*)_data;

	offset queryV;
	range(i, 3) queryV[i] = data->p2[i] - data->p1[i];

	list<mover*> blastMovers;
	blastMovers.init();

	velbox_query(data->parent, data->p1, queryV, data->radius, &blastMovers);
	rangeconst(iter, blastMovers.num) {
		mover *m = blastMovers[iter];
		if (!(m->type & T_PLAYER)) continue;
		player *blastee = playerFromMover(m);
		offset d;
		range(i, 3) d[i] = blastee->m.pos[i] - data->p2[i];

		// This should almost always be small enough to safely square,
		// but for relativistic players maybe not.
		int64_t mg = mag(d);
		if (mg > data->radius || !mg) continue;
		range(i, 3) blastee->vel[i] += d[i]*data->launch/mg;
		player_hit(gs, gs->clock+1, blastee, data->dmg);
	}

	blastMovers.destroy();
	return 1;
}

static char trans(gamestate *gs, void **ptr) {
	if (seriz_error()) {
		puts("tasks/explosion should never be serialized!");
	}
	return 1;
}

static void copy(void **to, void *from) {
	puts("tasks/explosion should never be copied!");
	*to = NULL;
}

static void destroy(void *data) {
	delete (taskExplosionData*)data;
}

void taskExplosion_define(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->copy = &copy;
	d->destroy = &destroy;
}

void taskExplosion_add(gamestate *gs, offset p1, offset p2, int64_t radius, int64_t launch, int32_t dmg, box *parent) {
	taskExplosionData *data = new taskExplosionData;
	addTaskEnd(gs, TSK_EXPLOSION, data);

	memcpy(data->p1, p1, sizeof(offset));
	memcpy(data->p2, p2, sizeof(offset));
	data->radius = radius;
	data->launch = launch;
	data->dmg = dmg;
	data->parent = parent;
}
