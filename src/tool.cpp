#include "util.h"
#include "list.h"
#include "matrix.h"
#include "gamestate.h"
#include "serialize.h"

#include "tool.h"

#include "tools/rifle.h"
#include "tools/rl.h"

// This file is pretty similar to task.cpp

static list<toolDefn> toolDefns;

toolDefn* toolLookup(int id) {
	rangeconst(i, toolDefns.num) {
		if (toolDefns[i].id == id) return &toolDefns[i];
	}
	return NULL;
}

void tool_trans(toolInst **data) {
	if (seriz_reading) {
		int ix = read32();
		toolDefn *defn = toolLookup(ix);
		if (defn) {
			(*defn->trans)(data);
			(*data)->defn = defn;
		} else {
			if (seriz_error()) {
				printf("Bad tool index %d\n", ix);
			}
			// TODO: Initialize some dummy default tool here I guess
		}
	} else {
		toolDefn *defn = (*data)->defn;
		write32(defn->id);
		(*defn->trans)(data);
	}
}

void tool_copy(toolInst **to, toolInst *from) {
	toolDefn *defn = from->defn;
	(*defn->copy)(to, from);
	(*to)->defn = defn;
}

void tool_destroy(toolInst *t) {
	(*t->defn->destroy)(t);
}

static void add(int id, void (*f)(toolDefn*)) {
	toolDefn &defn = toolDefns.add();
	defn.id = id;
	(*f)(&defn);
}

void tool_init() {
	toolDefns.init();

	add(TOOL_RIFLE, &toolRifle_define);
	add(TOOL_RL, &toolRl_define);
}

void tool_destroy() {
	toolDefns.destroy();
}
