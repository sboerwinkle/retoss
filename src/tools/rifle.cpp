#include "../gamestate.h"
#include "../serialize.h"
#include "../tool.h"

#include "rifle.h"

static void draw(gamestate *gs, player *p, toolInst *_data) {
	toolRifle &data = *(toolRifle*)_data;
}

static void use(gamestate *gs, player *p, toolInst *_data) {
	toolRifle &data = *(toolRifle*)_data;
}

static char trans(toolInst **_data) {
	if (seriz_reading) {
		*_data = new toolRifle;
	}
	toolRifle &data = *(toolRifle*)*_data;
	return 0;
}

static void copy(toolInst **_to, toolInst *_from) {
	*_to = new toolRifle;
	toolRifle &data = *(toolRifle*)*_to;
}

static void destroy(toolInst *data) {
	delete data;
}

void toolRifle_create(toolInst **_data) {
	*_data = new toolRifle;
	toolRifle &data = *(toolRifle*)*_data;
	data.defn = toolLookup(TOOL_RIFLE);
}

void toolRifle_define(toolDefn *defn) {
	defn->draw = draw;
	defn->use = use;
	defn->trans = trans;
	defn->copy = copy;
	defn->destroy = destroy;
}
