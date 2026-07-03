#pragma once

struct gamestate;

struct taskDefn {
	int id;
	void (*step)(gamestate *gs, void *data);
	char (*trans)(gamestate *gs, void **data);
	void (*copy)(void **to, void *from);
	void (*destroy)(void *data);
};

struct taskInstance {
	void *data;
	taskDefn *defn;
};

enum {
	TSK_TDM,
	TSK_KILL_PLANE,
	TSK_RAILS,
	TSK_DYNAMICS,
};

extern taskDefn* taskLookup(int id);

extern void task_init();
extern void task_destroy();
