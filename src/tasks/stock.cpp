#include "../util.h"

#include "../gamestate.h"

#include "../game_gamestate.h"
#include "../main.h"
#include "../task.h"
#include "../random.h"
#include "../graphics.h"
#include "../serialize.h"

#include "grenades.h"
#include "tdmScore.h"

#include "stock.h"

#define RESPAWN_DELAY 45

#define TIMER_ALIVE 255

void taskStock_draw(gamestate *gs, void *_data) {
	taskStockData *data = (taskStockData*)_data;
	// For now we draw everything character-by-character,
	// because it's easier to think about and I'm very tired.
	// Can always improve this later lol
	char s[2];
	s[1] = '\0';
	range(j, data->maxScore+1) {
		s[0] = '0' + (data->maxScore-j)%10;
		if (j == data->nextLife) {
			// TODO: default text brightness should be a constant so I can reference it here
			spriteColorAdd(0, 0.25, 0, 0);
			// Holds until the end of the row.
		}
		drawText(s, j*5, 0);
	}
	spriteColorAdd(0, 0, 0, 0);

	range(i, gs->players.num) {
		taskStockPlayer *pl = &data->players[i];
		player *p = &gs->players[i];
		char signifier = '?';
		if (p->skin && p->skin->descr.str[0]) {
			signifier = p->skin->descr.str[0];
		}

		range(j, data->maxScore+1) {
			s[0] = (j == pl->deaths) ? signifier : ' ';
			if (j == data->nextLife) {
				spriteColorAdd(0, 0.25, 0, 0);
			}
			drawText(s, j*5, (i+1)*7);
		}
		spriteColorAdd(0, 0, 0, 0);
	}

	if (!data->start) return;

	u8 next = data->start;
	range(j, data->maxScore+1) {
		s[0] = ' ';
		if (j == next) {
			next += data->step;
			s[0] = '<';
			if (j == data->nextLife) {
				spriteColorAdd(0, 0.25, 0, 0);
				defaultTextColor();
			} else {
				spriteColorMult(0.3, 0.3, 0.3, 1);
			}
		}
		drawText(s, j*5, (gs->players.num+1)*7);
	}
	spriteColorAdd(0, 0, 0, 0);
	defaultTextColor();
}

static void shuffleSpawns(gamestate *gs, taskStockData *data, char force) {
	uint32_t seed = splitmix32(&gs->seed);
	// We can skip the shuffling in some cases since it won't be needed
	// immediately, but this is kind of silly since it's so quick to do
	// anyway. It was fun for me to think about, though, so here it is.
	if (!force && gs != rootState) return;

	for (int32_t i = data->spawns.num; i > 1; i--) {
		int32_t dest = splitmix32(&seed) % i;
		offset_t tmp = data->spawns[i-1];
		data->spawns[i-1] = data->spawns[dest];
		data->spawns[dest] = tmp;
	}
}

// During setup, this can be called before `data->players` has anything,
// so don't rely on that list!
static void respawn(gamestate *gs, taskStockData *data, int i) {
	player *p = &gs->players[i];
	softResetPlayer(p);

	// Doing the increment (and modulus) first prevents all kinds of OoB headaches
	// (malicious serialization payload, unexpected interaction with editor...)
	data->spawnIx = (data->spawnIx+1) % data->spawns.num;
	int64_t *spawnPos = data->spawns[data->spawnIx].o;

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
		pl->timer = p->alive ? TIMER_ALIVE : 0;

		p->team = 10+i;
		redoSkin(p);
	}
}

static char step(gamestate *gs, void *_data) {
	taskStockData *data = (taskStockData*)_data;
	if (!data->spawns.num) return 0;

	handleNewPlayers(gs, data);
	char addLife = 1;
	char respawnWorking = 0;
	char respawnFinished = 0;

	rangeconst(i, gs->players.num) {
		taskStockPlayer *pl = &data->players[i];
		player *p = &gs->players[i];

		if (!p->alive) {
			if (pl->timer == TIMER_ALIVE) {
				pl->timer = 0;
				pl->deaths++;
			}
			if (pl->deaths < data->maxScore) {
				if (pl->timer >= RESPAWN_DELAY) {
					respawnFinished = 1;
					pl->timer = TIMER_ALIVE;
					respawn(gs, data, i);
				} else {
					respawnWorking = 1;
					pl->timer++;
				}
			}
		}

		if (pl->deaths < data->nextLife) addLife = 0;
	}

	if (addLife) {
		rangeconst(i, gs->players.num) {
			data->players[i].deaths--;
		}
		data->nextLife += data->step;
	}

	if (respawnFinished && !respawnWorking) {
		shuffleSpawns(gs, data, 0);
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
	trans8(&data->start);
	trans8(&data->spawnIx);

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
	dest->start = src->start;
	dest->spawnIx = src->spawnIx;
	dest->players.init(src->players);
	dest->spawns.init(src->spawns);
}

static void destroy(void *_data) {
	taskStockData *data = (taskStockData*)_data;
	data->players.destroy();
	data->spawns.destroy();
	delete data;
}

void taskStock_create(gamestate *gs, u8 maxScore, u8 step, u8 start) {
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
	data->step = step;
	data->nextLife = data->start = start;
	if (!start) data->nextLife = 255;
	data->spawnIx = 0;
	data->players.init();


	// Reset players and grenades, game starts immediately.
	// No fancy "get ready" text like for TDM.

	shuffleSpawns(gs, data, 1);
	range(i, gs->players.num) respawn(gs, data, i);
	shuffleSpawns(gs, data, 0);

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
