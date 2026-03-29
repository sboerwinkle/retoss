#include "../util.h"

#include "../gamestate.h"
#include "../task.h"
#include "../random.h"
#include "../graphics.h"
#include "../serialize.h"

#include "tdmScore.h"

// Whole space avail
static void drawPrepStartText() {
	drawTextCentered("SET YOUR /TEAM", 0);
	drawTextCentered("SHOOT SOMEONE WHEN READY", 7);
}

// Whole space avail
static void drawSmallSnakes(tskTdmData const * data) {
	// I don't know offhand how printf handles rounding vs truncating,
	// so a bit of math to make sure truncate/round both look correct.
	float s0 = (3.0f * data->scores[0] + 1)/30;
	float s1 = (3.0f * data->scores[1] + 1)/30;
	char text[12];
	snprintf(text, 12, "%.1f v %.1f", s0, s1);
	drawText(text, 0, 0);
}

#define SNAKE_BASIC 2
#define SNAKE_OHNO  3
#define SNAKE_CHOMP_1 1
// Didn't wind up drawing a separate sprite, it re-uses SNAKE_BASIC
#define SNAKE_CHOMP_2 2

// Whole space avail
static void drawSnakes(tskTdmData const *data, u8 dests[2], u8 heads[2], float interp) {
	selectTex2d(11, 40, 40);
	// We have 5 display units per score point,
	// but 10 internal units per score point,
	// so we have to divide `scoreLimit` by 2.
	float left = (data->scoreLimit/2 + 20)/-2.0f;
	interp = (data->timer + interp) / 15;
	range(i, 2) {
		float amt = data->scores[i];
		amt += (dests[i]-amt)*interp;
		amt /= 2;
		int shift = i*10;
		float snakeY = -displayAreaBounds[1] + shift;
		// tail
		sprite2d(0, shift, 10, 10, left, snakeY);
		// segments
		for (int x = 0; x < amt; x += 5) {
			sprite2d(10, shift, 5, 10, left+10+x, snakeY);
		}
		// head
		float headX = 20*(heads[i] & 1) + 5;
		float headY = 10*(heads[i] & 2) + shift;
		sprite2d(headX, headY, 15, 10, left+amt+5, snakeY);
	}
}

// Leave space for `drawSnakes`
static void drawRoundWinner(char winner) {
	char text[13];
	char const * printMe;
	if (winner >= 0) {
		snprintf(text, 13, "%s SCORES!", winner ? "BLUE" : "RED");
		printMe = text;
	} else if (winner == -1) {
		printMe = "ROUND TIED";
	} else {
		printMe = "TEAMS SET";
	}
	drawTextCentered(printMe, 21);
}

// Leave space for `drawSnakes`
static void drawGameWinner(u8 winnerTeam) {
	char text[18];
	snprintf(text, 18, "%s TEAM WINS!!!", winnerTeam ? "BLUE" : "RED");
	drawTextCentered(text, 21);
}

// Leave space for `drawSnakes` and `drawRoundWinner`
static void drawPrepText() {
	drawTextCentered(">>> STARTING <<<", 28);
}

// For now, we can assume `setup2dText` was already called,
// and that it will be called again for us when we exit.
// Conveniently, this means we can ignore it.
void taskTdm_draw(void *_data, float interp) {
	tskTdmData const *data = (tskTdmData*)_data;
	u8 state = data->state;
	if (state == TSK_TDM_ST_PREP_START) {
		drawPrepStartText();
		return;
	} else if (state == TSK_TDM_ST_PLAY) {
		drawSmallSnakes(data);
		return;
	}
	u8 winnerTeam = !!data->winner;
	u8 dests[2];
	memcpy(dests, data->scores, sizeof(dests));
	u8 heads[2] = {SNAKE_BASIC, SNAKE_BASIC};
	if (state == TSK_TDM_ST_GROW_ANIM) {
		dests[winnerTeam] = data->animDest;
		drawRoundWinner(winnerTeam);
	} else if (state == TSK_TDM_ST_CHOMP_ANIM) {
		dests[1-winnerTeam] = data->animDest;
		heads[winnerTeam] = ((data->timer % 5) > 1) ? SNAKE_CHOMP_1 : SNAKE_CHOMP_2;
		heads[1-winnerTeam] = SNAKE_OHNO;
		drawRoundWinner(winnerTeam);
	} else if (state == TSK_TDM_ST_PREP_AGAIN) {
		// This state also handles ties and first round prep,
		// so we can't use `winnerTeam` (which assumes 0/1)
		drawRoundWinner(data->winner);
		// This state uses `animDest` as the timer destination.
		// Show the prep text for 2 seconds before the next round starts.
		if (data->timer >= data->animDest-30) {
			drawPrepText();
		}
	} else { // TSK_TDM_ST_DONE
		heads[1-winnerTeam] = SNAKE_OHNO;
		drawGameWinner(winnerTeam);
	}
	drawSnakes(data, dests, heads, interp);
}

static int32_t rand(uint32_t input) {
	return splitmix32(&input);
}

static char getSpawnFlip(gamestate const *gs) {
	return 1 & rand(gs->vb_root->end);
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
		data->animDest = data->scores[(u8)aliveTeam] + 10;
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

static void spawn(gamestate *gs, int i, tskTdmData *data, char flip) {
	player *p = &gs->players[i];
	if (data->numSpawns < 2) return;

	// We want to preserve their team
	char team = p->team;
	resetPlayer(gs, i);
	p->team = team;

	// However, for selecting spawns, we coerce to {0,1}.
	u8 spawn;
	if (team >= 0 && team < 2) {
		spawn = team ^ flip;
	} else {
		spawn = 1 & rand(gs->vb_root->end + i);
	}
	memcpy(p->m.pos, data->spawns[spawn], sizeof(offset));
	// IDK if we technically need this as well, seems reasonable
	memcpy(p->m.oldPos, data->spawns[spawn], sizeof(offset));
}

static void beginRound(gamestate *gs, tskTdmData *data) {
	char flip = getSpawnFlip(gs);
	rangeconst(i, gs->players.num) {
		spawn(gs, i, data, flip);
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
	char flip = getSpawnFlip(gs);
	rangeconst(i, gs->players.num) {
		if (!gs->players[i].alive) {
			spawn(gs, i, data, flip);
			hasDead = 1;
		}
		char team = gs->players[i].team;
		if (team < 0 || team >= 2) hasUndecided = 1;
	}

	// This is how we know people are happy with the teams: someone gets shot
	if (hasDead && !hasUndecided) {
		data->winner = -2;
		data->state = TSK_TDM_ST_PREP_AGAIN;
		data->timer = 0;
		// 3 seconds for initial startup
		data->animDest = 45;
	}
}

static void handleDone(gamestate *gs, tskTdmData *data) {
	char flip = getSpawnFlip(gs);
	rangeconst(i, gs->players.num) {
		if (!gs->players[i].alive) spawn(gs, i, data, flip);
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
		handleDone(gs, data);
	} else if (data->state == TSK_TDM_ST_PREP_START) {
		handlePrepStart(gs, data);
	}
}

static void trans(void **ptr) {
	tskTdmData *&data = *(tskTdmData**)ptr;
	if (seriz_reading) {
		data = (tskTdmData*)malloc(sizeof(tskTdmData));
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
		data->spawns = (offset*)malloc(data->numSpawns*sizeof(offset));
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

	tskTdmData *dest = (tskTdmData*)malloc(sizeof(tskTdmData));
	*to = dest;
	tskTdmData *src = (tskTdmData*)from;

	*dest = *src;

	size_t sz = dest->numSpawns * sizeof(offset);
	dest->spawns = (offset*)malloc(sz);
	memcpy(dest->spawns, src->spawns, sz);
}

static void destroy(void *data) {
	free(((tskTdmData*)data)->spawns);
	free(data);
}

tskTdmData* taskTdm_create(gamestate *gs, int numSpawns, int maxScore) {
	if (maxScore > 24) {
		printf("Currently, greatest allowable max score is 24! Replacing %d with 24.\n", maxScore);
		maxScore = 24;
	}
	taskInstance &task = gs->tasks.add();
	task.defn = taskLookup(TSK_TDM);
	tskTdmData *data = (tskTdmData*)malloc(sizeof(tskTdmData));
	task.data = data;
	data->scores[0] = data->scores[1] = data->winner = data->animDest = data->timer = 0;
	data->state = TSK_TDM_ST_PREP_START;
	data->scoreLimit = maxScore * 10;
	data->numSpawns = numSpawns;
	data->spawns = (offset*)malloc(numSpawns * sizeof(offset));
	return data;
}

void taskTdm_spawnAll(gamestate *gs, void *_data) {
	tskTdmData *data = (tskTdmData*)_data;
	char flip = getSpawnFlip(gs);
	rangeconst(i, gs->players.num) {
		if (gs->players[i].alive) spawn(gs, i, data, flip);
	}
}

void defineTask_tdmScore(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->copy = &copy;
	d->destroy = &destroy;
}
