#pragma once

struct gamestate;
struct player;
struct toolInst;

struct toolDefn {
	int id;
	void (*draw)(gamestate *gs, player *p, float y, toolInst *data);
	void (*use)(gamestate *gs, player *p, char input, toolInst *data);
	char (*trans)(toolInst **data);
	void (*copy)(toolInst **to, toolInst *from);
	void (*destroy)(toolInst *data);
};

struct toolInst {
	toolDefn *defn;
};

enum {
	TOOL_RIFLE,
};

extern toolDefn* toolLookup(int id);

extern void tool_trans(toolInst **data);
extern void tool_copy(toolInst **to, toolInst *from);
// Shares the same name as `tool_destroy()`, a little unfortunate, may fix later
extern void tool_destroy(toolInst *t);

extern void tool_init();
extern void tool_destroy();
