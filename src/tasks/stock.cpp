#include "../util.h"

#include "../gamestate.h"

#include "../game_gamestate.h"
#include "../task.h"
#include "../random.h"
#include "../graphics.h"
#include "../serialize.h"

#include "grenades.h"
#include "tdmScore.h"

#include "stock.h"

#define RESPAWN_DELAY 45

static int getMinDeaths(taskStockData *data) {
	int result = 255;
	rangeconst(i, data->players.num) {
		if (data->players[i].deaths < result) {
			result = data->players[i].deaths;
		}
	}
	return result;
}

void taskStock_draw(gamestate *gs, void *_data) {
	taskStockData &data = *(taskStockData*)_data;
	// Max message is:
	// / # (##/##)
	// 123456789012
	char text[12];
	range(i, gs->players.num) {
		taskStockPlayer *pl = &data.players[i];
		player *p = &gs->players[i];
		char signifier = '?';
		if (p->skin && p->skin->descr.str[0]) {
			signifier = p->skin->descr.str[0];
		}
		snprintf(text, 12, "%c%2d", signifier, pl->deaths);
		drawText(text, 0, i*7);
	}

	u8 start, end;
	if (!data.step) {
		start = end = data.maxScore;
	} else if (data.step > 1) {
		start = data.maxScore + 1 - (data.nextLife / data.step);
		end = start + (start-1)/(data.step-1);
	} else {
		start = data.maxScore + 1 - data.nextLife;
		end = data.maxScore + 1;
	}

	for (int i = start; i <= end; i++) {
		if (i == data.maxScore) {
			defaultTextColor();
			snprintf(text, 12, "/%2d", i);
		} else {
			// TODO: default text brightness should be a constant so I can reference it here
			spriteColorMult(0.4, 0.4, 0.4, 1);
			if (i == data.maxScore + 1) {
				snprintf(text, 12, " %2d (%d/%d)", i, getMinDeaths(&data), data.nextLife);
			} else {
				snprintf(text, 12, " %2d", i);
			}
		}
		drawText(text, 0, (gs->players.num + i - start)*7 + 3);
	}

	defaultTextColor();
}

static uint32_t rand(uint32_t input) {
	return splitmix32(&input);
}

// During setup, this can be called before `data->players` has anything,
// so don't rely on that list!
static void respawn(gamestate *gs, taskStockData *data, int i) {
	player *p = &gs->players[i];

	softResetPlayer(p);

	int spawnIx = rand(gs->clock * gs->players.num + i) % data->spawns.num;
	int64_t *spawnPos = data->spawns[spawnIx].o;
	memcpy(p->m.pos, spawnPos, sizeof(offset));
	// IDK if we technically need this as well, seems reasonable
	memcpy(p->m.oldPos, spawnPos, sizeof(offset));
}

static void handleNewPlayers(gamestate *gs, taskStockData *data) {
	while (gs->players.num > data->players.num) {
		int i = data->players.num;
		taskStockPlayer *pl = &data->players.add();
		player *p = &gs->players[i];

		// First respawn is free if they're already
		// dead when we see them the first time.
		pl->deaths = 0;
		pl->timer = p->alive ? 0 : 1;

		p->team = 10+i;
		redoSkin(p);
	}
}

static char checkAddLife(taskStockData *data) {
	rangeconst(i, data->players.num) {
		if (data->players[i].deaths < data->nextLife) return 0;
	}
	return 1;
}

static char step(gamestate *gs, void *_data) {
	taskStockData *data = (taskStockData*)_data;
	if (!data->spawns.num) return 0;

	handleNewPlayers(gs, data);
	char addLife = 0;

	rangeconst(i, gs->players.num) {
		taskStockPlayer *pl = &data->players[i];
		player *p = &gs->players[i];

		if (!p->alive) {
			if (pl->timer <= RESPAWN_DELAY) {
				if (!pl->timer) {
					pl->deaths++;
					if (pl->deaths == data->nextLife) {
						addLife = checkAddLife(data);
					}
				}
				pl->timer++;
			}
			if (pl->timer > RESPAWN_DELAY && pl->deaths < data->maxScore) {
				pl->timer = 0;
				respawn(gs, data, i);
			}
		}
	}

	if (addLife) {
		data->maxScore++;
		data->nextLife += data->step;
	}

	return 0;
}

static char trans(gamestate *gs, void **ptr) {
	taskStockData *&data = *(taskStockData**)ptr;
	if (seriz_reading) {
		data = new taskStockData;
		data->players.init();
		data->spawns.init();
	}
	trans8(&data->maxScore);
	trans8(&data->nextLife);
	trans8(&data->step);

	transItemCount(&data->players);
	range(i, data->players.num) {
		trans8(&data->players[i].deaths);
		trans8(&data->players[i].timer);
	}

	transItemCount(&data->spawns);
	range(i, data->spawns.num) {
		transOffset(data->spawns[i].o);
	}
	return 0;
}

static void copy(void **to, void *from) {
	// Todo:
	// We could make `spawns` copy-on-write.
	// Could do the same in `tdmScore.cpp`.

	taskStockData *dest = new taskStockData;
	*to = dest;
	taskStockData *src = (taskStockData*)from;

	dest->maxScore = src->maxScore;
	dest->nextLife = src->nextLife;
	dest->step = src->step;
	dest->players.init(src->players);
	dest->spawns.init(src->spawns);
}

static void destroy(void *_data) {
	taskStockData *data = (taskStockData*)_data;
	data->players.destroy();
	data->spawns.destroy();
	delete data;
}

void taskStock_create(gamestate *gs, u8 maxScore, u8 step) {
	taskStockData *data;
	taskInstance *existingInst;

	if (NULL != (existingInst = findTaskStart(gs, TSK_TDM))) {
		data = new taskStockData;
		addTaskStart(gs, TSK_STOCK, data);
		data->spawns.init(((tskTdmData*)existingInst->data)->spawns);
		taskTdm_destroy(gs, existingInst);
	} else if (NULL != (existingInst = findTaskStart(gs, TSK_STOCK))) {
		data = (taskStockData*)existingInst->data;
		data->players.destroy();
	} else {
		// todo: Only log if `gs==rootState`
		puts("taskStock: No existing TSK_TDM or TSK_STOCK to pull spawns from, aborting");
		return;
	}

	data->maxScore = maxScore;
	data->nextLife = data->step = step;
	data->players.init();


	// Reset players and grenades, game starts immediately.
	// No fancy "get ready" text like for TDM.

	range(i, gs->players.num) respawn(gs, data, i);

	for (taskInstance *tsk = gs->tasks.next; tsk != &gs->tasks; tsk = tsk->next) {
		if (tsk->defn->id == TSK_GRENADES) {
			taskGrenades_clear2(tsk->data);
		}
	}
}

// Any impl of taskStock_destroy should be conscious of TSK_GUNGAME

void taskStock_define(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->copy = &copy;
	d->destroy = &destroy;
}
