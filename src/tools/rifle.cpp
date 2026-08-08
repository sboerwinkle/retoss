#include "../gamestate.h"

#include "../graphics.h"
#include "../serialize.h"
#include "../task.h"
#include "../tool.h"

#include "rifle.h"

static void draw(gamestate *gs, player *p, float y, toolInst *_data) {
	toolRifle &data = *(toolRifle*)_data;

	// We've got it on the "font" texture for now.
	// We double the resolution b/c we have to draw halves in some cases,
	// and need to split a pixel for that.
	selectTex2d(1, 128, 128);
	centeredGrid2d(256);
	y *= displayAreaBounds[1];
	if (data.cooldown) {
		// Split crosshair
		float distance = (data.cooldown - gfx_interpRatio)/2;
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
	toolRifle &data = *(toolRifle*)_data;
	if (data.cooldown) data.cooldown--;
	if (input && !data.cooldown) {
		data.cooldown = 10;
		addTaskEnd(gs, TSK_RIFLE_SHOT, p);
	}
}

static char trans(toolInst **_data) {
	if (seriz_reading) {
		*_data = (toolRifle*)malloc(sizeof(toolRifle));
	}
	toolRifle &data = *(toolRifle*)*_data;
	trans32(&data.cooldown);
	return 0;
}

static void copy(toolInst **_to, toolInst *_from) {
	*_to = (toolRifle*)malloc(sizeof(toolRifle));
	toolRifle &to = *(toolRifle*)*_to;
	toolRifle &from = *(toolRifle*)_from;
	to.cooldown = from.cooldown;
}

static void destroy(toolInst *data) {
	free(data);
}

void toolRifle_create(toolInst **_data) {
	*_data = (toolRifle*)malloc(sizeof(toolRifle));
	toolRifle &data = *(toolRifle*)*_data;
	data.defn = toolLookup(TOOL_RIFLE);
	data.cooldown = 0;
}

void toolRifle_define(toolDefn *defn) {
	defn->draw = draw;
	defn->use = use;
	defn->trans = trans;
	defn->copy = copy;
	defn->destroy = destroy;
}
