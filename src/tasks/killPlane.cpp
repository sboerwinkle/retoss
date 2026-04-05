#include "../util.h"

#include "../gamestate.h"
#include "../task.h"
#include "../serialize.h"

static void step(gamestate *gs, void *data) {
	int64_t cutoff = *(int64_t*)data;
	rangeconst(i, gs->players.num) {
		player *p = &gs->players[i];
		int64_t z = p->m.pos[2] - cutoff;
		if (z < 0) killPlayer(p);
	}
}

static void trans(void **ptr) {
	int64_t *&data = *(int64_t**)ptr;
	if (seriz_reading) {
		data = (int64_t*)malloc(sizeof(int64_t));
	}
	trans64(data);
}

static void copy(void **to, void *from) {
	int64_t *data = (int64_t*)malloc(sizeof(int64_t));
	*to = data;
	*data = *(int64_t*)from;
}

void taskKillPlane_create(gamestate *gs, int64_t altitude) {
	taskInstance &task = gs->tasks.add();
	task.defn = taskLookup(TSK_KILL_PLANE);
	int64_t *data = (int64_t*)malloc(sizeof(int64_t));
	task.data = data;
	*data = altitude;
}

void defineTask_killPlane(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->copy = &copy;
	// This is a little cheeky, but it works fine, so why not
	d->destroy = &free;
}
