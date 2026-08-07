#include "../util.h"

#include "../gamestate.h"
#include "../player.h"
#include "../serialize.h"
#include "../task.h"

static char step(gamestate *gs, void *data) {
	rangeconst(i, gs->players.num) {
		playerUpdate(gs, &gs->players[i]);
	}

	rangeconst(i, gs->players.num) {
		playerAddBox(gs, &gs->players[i]);
	}

	rangeconst(i, gs->players.num) {
		if (gs->players[i].alive) {
			// This uses `bcast`, which requires stuff to have its
			// old position populated. Must run after other stuff.
			pl_postStep(gs, &gs->players[i]);
		}
	}

	return 0;
}

static char trans(gamestate *gs, void **ptr) {
	if (seriz_reading) *ptr = NULL;
	return 0;
}

static void copy(void **to, void *from) {
	*to = NULL;
}

static void destroy(void *data) {
	// no op, `data` will be NULL
}

void taskDoPlayers_define(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->copy = &copy;
	d->destroy = &destroy;
}
