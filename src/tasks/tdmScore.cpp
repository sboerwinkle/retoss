#include "../util.h"
#include "../tasks.h"
#include "tdmScore.h"

void taskTdm_draw(void *_data, float interp) {
	tskTdmData *data = (tskTdmData*)_data;
	u8 state = data->state;
	if (state == TSK_TDM_ST_PREP_START) {
		drawPrepStartText();
		return;
	} else if (state == TSK_TDM_ST_PLAY) {
		drawSmallSnakes(data);
		return;
	}
	char winner = !!data->winner;
	u8 dests[2];
	memcpy(dests, data->scores, sizeof(dests));
	u8 heads[2] = {SNAKE_BASIC, SNAKE_BASIC};
	if (state == TSK_TDM_ST_GROW_ANIM) {
		dests[winner] = data->animDest;
		drawRoundWinner(data);
	} else if (state == TSK_TDM_ST_CHOMP_ANIM) {
		dests[1-winner] = data->animDest;
		heads[winner] = ((data->timer % 15) >= 5) ? SNAKE_CHOMP_1 : SNAKE_CHOMP_2;
		heads[1-winner] = SNAKE_OHNO;
		drawRoundWinner(data);
	} else if (state == TSK_TDM_ST_PREP_AGAIN) {
		drawRoundWinner(data);
		drawPrepText();
	} else { // TSK_TDM_ST_DONE
		heads[1-winner] = SNAKE_OHNO;
		drawGameWinner(winner);
	}
	drawSnakes(data, dests, heads, interp);
}

static void handlePlay(gamestate *gs, tskTdmData *data) {
	char aliveTeam = -1;
	rangeconst(i, gs->players.num) {
		player &p = gs->players[i];
		if (!p.alive || p.team < 0 || p.team >= 2) continue;

		// First live player seen
		if (aliveTeam == -1) aliveTeam = p.team;
		// Boring
		if (aliveTeam == p.team) continue;
		// Else we have multiple teams alive, nothing to do.
		return;
	}
	if (aliveTeam == -1) {
		// Ties are declared as soon as everyone's dead. No delay.
		data->winner = -1;
		data->state = TSK_TDM_ST_PREP_AGAIN;
		data->timer = 0;
		// PREP_AGAIN uses this to know how long to wait,
		// since animated states take time and we'd like the
		// total to be consistent.
		data->animDest = 75;
	} else {
		// Looks like we have a winnner, wait a sec to see if they die.
		// Right now there's nothing more delayed than dying of a gunshot wound,
		// so we don't have to wait very long.
		data->timer++;
		if (data->timer < 15) return;
		// We know `aliveTeam` must be 0 or 1 at this point
		data->winner = aliveTeam;
		data->state = TSK_TDM_ST_GROW_ANIM;
		data->timer = 0;
		// Add 1.0 pts, animate to that score.
		data->animDest = data->scores[aliveTeam] + 10;
	}
}

static void handleGrowAnim(tskTdmData *data) {
	// 1 second spent on this anim
	data->timer++;
	if (data->timer < 15) return;

	u8 winner = !!data->winner;
	data->scores[winner] = data->animDest;
	// Okay, time to see what state is next...
	u8 loserScore = data->scores[1-winner];
	// Store scores as 8 bits because I'm coocoo.
	// This means a limit of 24 points per round,
	// because if it was 25, winner could go from
	// 24.9 -> 25.9 pts (overflows: 25.9 == 259).
	// Score diff must be in a wider type so that
	// we can represent negatives.
	int diff = (int)loserScore - data->animDest;
	if (diff > 0) {
		data->state = TSK_TDM_ST_CHOMP_ANIM;
		data->timer = 0;
		data->animDest = loserScore - (diff+1)/2;
	} else if (data->animDest >= data->scoreLimit) {
		data->state = TSK_TDM_ST_DONE;
		// No need for further data in this state
	} else {
		data->state = TSK_TDM_ST_PREP_AGAIN;
		data->timer = 0;
		// 5 sec intermission - 1 sec grow = 4 seconds here
		data->animDest = 60;
	}
}

static void handleChompAnim(tskTdmData *data) {
	// 1 second spent on this anim
	data->timer++;
	if (data->timer < 15) return;

	u8 winner = !!data->winner;
	data->scores[1-winner] = data->animDest;

	data->state = TSK_TDM_ST_PREP_AGAIN;
	data->timer = 0;
	// 5 sec intermission - 1 sec grow - 1 sec chomp = 3 seconds here
	data->animDest = 45;
}

static void spawn(gamestate *gs, int i, tskTdmData *data) {
	// TODO pick spawn for each team randomly per round
	player *p = &gs->players[i];
	if (data->numSpawns < 2) return;

	// We want to preserve their team
	char team = p->team;
	resetPlayer(gs, i);
	p->team = team;

	// However, for selecting spawns, we coerce to {0,1}.
	u8 spawn;
	if (team >= 0 && team < 1) {
		spawn = team; // TODO convert team to spawn (random mapping from earlier?)
	} else {
		spawn = 0; // TODO random spawn
	}
	memcpy(p->m.pos, data->spawns[spawn], sizeof(offset));
	// IDK if we technically need this as well, seems reasonable
	memcpy(p->m.oldPos, data->spawns[spawn], sizeof(offset));
}

static void beginRound(gamestate *gs, tskTdmData *data) {
	rangeconst(i, gs->players.num) {
		spawn(gs, i, data);
	}

	data->state = TSK_TDM_ST_PLAY;
	data->timer = 0;
}

static void handlePrepAgain(gamestate *gs, tskTdmData *data) {
	data->timer++;
	if (data->timer < data->animDest) return;

	beginRound(gs, data);
}

static void handlePrepStart(gamestate *gs, tskTdmData *data) {
	char hasUndecided = 0;
	char hasDead = 0;
	rangeconst(i, gs->players.num) {
		if (!gs->players[i].alive) {
			spawn(gs, i, data);
			hasDead = 1;
		}
		char team = gs->players[i].team;
		if (team < 0 || team >= 2) hasUndecided = 1;
	}

	// This is how we know people are happy with the teams: someone gets shot
	if (hasDead && !hasUndecided) beginRound(gs, data);
}

static void handleDone(gamestate *gs) {
	rangeconst(i, gs->players.num) {
		if (!gs->players[i].alive) spawn(gs, i, data);
	}
}

static void step(gamestate *gs, void *_data) {
	tskTdmData *data = (tskTdmData*)_data;
	// Todo If I really cared that much I could do a bounds check on `data->state` and do a lookup lol
	if (data->state == TSK_TDM_ST_PLAY) {
		handlePlay(gs, data);
	} else if (data->state == TSK_TDM_ST_GROW_ANIM) {
		handleGrowAnim(data);
	} else if (data->state == TSK_TDM_ST_CHOMP_ANIM) {
		handleChompAnim(data);
	} else if (data->state == TSK_TDM_ST_PREP_AGAIN) {
		handlePrepAgain(gs, data);
	} else if (data->state == TSK_TDM_ST_DONE) {
		handleDone(gs);
	} else if (data->state == TSK_TDM_ST_PREP_START) {
		handlePrepStart(gs, data);
	}
}

static void trans(void **ptr) {
	tskTdmData *&data = *(tskTdmData**)ptr;
	if (seriz_reading) {
		data = malloc(sizeof(tskTdmData));
	}
	trans8(&data->scores[0]);
	trans8(&data->scores[1]);
	trans8(&data->state);
	trans8(&data->winner);
	trans8(&data->animDest);
	trans8(&data->timer);
	trans8(&data->scoreLimit);
	trans8(&data->numSpawns);
	if (seriz_reading) {
		// Normally we'd want a safety on the size we're allocating here
		// (don't trust data that comes from the network),
		// but since it's just an 8-bit counter it doesn't go that high.
		data->spawns = malloc(data->numSpawns*sizeof(offset));
	}
	range(i, data->numSpawns) {
		range(j, 3) trans64(&data->spawns[i][j]);
	}
}

static void copy(void **to, void *from) {
	// There's no reason to be copying this data around every frame, especially `spawns`.
	// Heck, if we updated `timer` to be a destination frame # (and not an offset),
	// we'd usually not be updating anything, and the whole thing could be copy-on-write.
	// ...
	// but for now I just want to get this to work.

	tskTdmData *dest = malloc(sizeof(tskTdmData));
	*to = dest;
	tskTdmData *src = (tskTdmData*)from;

	*dest = *src;

	size_t sz = dest->numSpawns * sizeof(offset);
	dest->spawns = malloc(sz);
	memcpy(dest->spawns, src->spawns, sz);
}

static void destroy(void *data) {
	free(((tskTdmData*)data)->spawns);
	free(data);
}

void defineTask_tdmScore(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->copy = &copy;
	d->destroy = &destroy;
}
