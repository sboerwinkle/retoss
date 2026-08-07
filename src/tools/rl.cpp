#include "../gamestate.h"

#include "../game.h"
#include "../game_gamestate.h"
#include "../graphics.h"
#include "../main.h"
#include "../player.h"
#include "../serialize.h"
#include "../tool.h"

#include "../tasks/rocket.h"

#include "rl.h"

static void draw(gamestate *gs, player *p, float y, toolInst *_data) {
	toolRl &data = *(toolRl*)_data;

	// We've got it on the "font" texture for now.
	// We double the resolution b/c we have to draw halves in some cases,
	// and need to split a pixel for that.
	selectTex2d(1, 128, 128);
	centeredGrid2d(256);
	y *= displayAreaBounds[1];
	if (data.cooldown) {
		// Split crosshair
		float distance = (data.cooldown - gfx_interpRatio)/4;
		// src coords, size, dest coords
		sprite2d(0, 10, 5, 10, -5-distance, y-5);
		sprite2d(5, 10, 5, 10,    distance, y-5);
	} else {
		// Could draw it as 2 halves in this case as well,
		// I'm just not sure if it might look funny b/c of
		// pixel nonsense.
		sprite2d(0, 10, 10, 10, -5, y-5);
	}
}

static void use(gamestate *gs, player *p, char input, toolInst *_data) {
	toolRl &data = *(toolRl*)_data;
	if (data.cooldown) data.cooldown--;
	if (input && !data.cooldown) {
		data.cooldown = 20;

		// Todo: Surely we'll need this more often, right? Save it somewhere?
		unitvec look;
		iquat_apply(look, p->m.rot, ((unitvec const){0, FIXP, 0}));
		offset vel;
		range(i, 3) vel[i] = p->m.pos[i] - p->m.oldPos[i];

		taskRocket_create(gs, p->m.oldPos, vel, look, p->prox);
	}
}

static char trans(toolInst **_data) {
	if (seriz_reading) {
		*_data = (toolRl*)malloc(sizeof(toolRl));
	}
	toolRl &data = *(toolRl*)*_data;
	trans32(&data.cooldown);
	return 0;
}

static void copy(toolInst **_to, toolInst *_from) {
	*_to = (toolRl*)malloc(sizeof(toolRl));
	toolRl &to = *(toolRl*)*_to;
	toolRl &from = *(toolRl*)_from;
	to.cooldown = from.cooldown;
}

static void destroy(toolInst *data) {
	free(data);
}

void toolRl_create(toolInst **_data) {
	*_data = (toolRl*)malloc(sizeof(toolRl));
	toolRl &data = *(toolRl*)*_data;
	data.defn = toolLookup(TOOL_RL);
	data.cooldown = 0;
}

void toolRl_define(toolDefn *defn) {
	defn->draw = draw;
	defn->use = use;
	defn->trans = trans;
	defn->copy = copy;
	defn->destroy = destroy;
}
