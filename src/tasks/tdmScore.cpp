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
	drawSnakes(data, dests, head, interp);
}

static void handlePlay(gamestate *gs, tskTdmData *data) {
	char aliveTeam = -1;
	rangeconst(i, gs->players.num) {
		player &p = gs->players[i];
		if (!p.alive) continue;

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
		// Win for `aliveTeam`.
		// Using this to index, coerce to 0-1 for memory safety.
		u8 winner = !!aliveTeam;
		data->winner = winner;
		data->state = TSK_TDM_ST_GROW_ANIM;
		data->timer = 0;
		// Add 1.0 pts, animate to that score.
		data->animDest = data->scores[winner] + 10;
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

static void handlePrepAgain(gamestate *gs, tskTdmData *data) {
	data->timer++;
	if (data->timer < data->animDest) return;

	// TODO: Respawn people or smthng

	data->state = TSK_TDM_ST_PLAY;
	data->timer = 0;
}

static void handleDone(gamestate *gs) {
	// TODO: Respawn anybody who dies I guess
}

static void handlePrepStart(gamestate *gs, tskTdmData *data) {
	// TODO: Wait until everybody has a valid team and somebody's dead.

	// Else, just respawn anybody who dies while people are still picking.
	// Conveniently, we have a method that happens to do exactly this...
	handleDone(gs);
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
	trans8(&data->score1);
	trans8(&data->score2);
	trans8(&data->animTimer);
	// TODO whatever values I have here lol
}

void defineTask_tdmScore(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->destroy = &free;
}
