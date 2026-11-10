#include "../util.h"

#include "../gamestate.h"
#include "../player.h"
#include "../serialize.h"
#include "../task.h"

#include "stock.h"

#include "gunGame.h"

#define MAX_LIMIT 6
static char const * loadouts[MAX_LIMIT+1] = {
	NULL,
	(char const[]) {0},
	(char const[]) {1, 0},
	(char const[]) {2, 1, 0},
	(char const[]) {2, 1, 0, 4},
	(char const[]) {2, 1, 1, 0, 4},
	(char const[]) {2, 2, 1, 1, 0, 4},
};

static char step(gamestate *gs, void *_data) {
	taskStockData *data = (taskStockData*)_data;

	// Some effort here to handle game lengths longer than what's planned
	u8 max;
	if (data->maxScore < 1) max = 1;
	else if (data->maxScore > MAX_LIMIT) max = MAX_LIMIT;
	else max = data->maxScore;

	rangeconst(i, data->players.num) {
		player &p1 = gs->players[i];
		taskStockPlayer &p2 = data->players[i];

		u8 deaths;
		if (p2.deaths < 0) deaths = 0;
		else if (p2.deaths >= max) deaths = max-1;
		else deaths = p2.deaths;
		char loadout = loadouts[max][deaths];

		if (p1.loadout != loadout) {
			p1.loadout = loadout;
			rekitPlayer(&p1);
		}
	}

	return 0;
}

static char trans(gamestate *gs, void **ptr) {
	if (!seriz_reading) return 0;
	taskInstance *ti = taskInstFromData(ptr)->next;
	if (!ti->defn || ti->defn->id != TSK_STOCK) {
		if (seriz_error()) {
			puts("tasks/gunGame: trans: next task isn't TSK_STOCK");
		}
		return 1;
	}
	*ptr = ti->data;
	return 0;
}

static void copy(void **to, void *from) {
	taskInstance *ti = taskInstFromData(to)->next;
	if (!ti->defn || ti->defn->id != TSK_STOCK) {
		puts("tasks/gunGame: copy: next task isn't TSK_STOCK");
		*to = NULL;
		return;
	}
	*to = ti->data;
}

static void destroy(void *data) {
	// no op, `data` belongs to someone else
}

void taskGunGame_create(gamestate *gs, u8 maxScore) {
	taskStock_create(gs, maxScore);

	taskInstance *existingInst = findTaskStart(gs, TSK_STOCK);
	if (existingInst == NULL) {
		// todo: Only log if `gs==rootState`
		puts("taskGunGame: Requires TSK_STOCK, but can't find one. Did it fail? Aborting.");
		return;
	}

	if (NULL == findTaskStart(gs, TSK_GUNGAME)) {
		addTaskStart(gs, TSK_GUNGAME, existingInst->data);
	}
}

void taskGunGame_define(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->copy = &copy;
	d->destroy = &destroy;
}
