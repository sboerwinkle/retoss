
enum {
	TSK_TDM_ST_PREP_START,
	TSK_TDM_ST_PLAY,
	TSK_TDM_ST_GROW_ANIM,
	TSK_TDM_ST_CHOMP_ANIM,
	TSK_TDM_ST_PREP_AGAIN,
	TSK_TDM_ST_DONE,
};

struct tskTdmData {
	u8 scores[2];
	u8 state;
	u8 winner;
	u8 animDest;
	u8 timer;
	u8 scoreLimit;
	list<offset_t> spawns;
};

extern void taskTdm_draw(void *_data, float interp);
extern tskTdmData* taskTdm_create(gamestate *gs, int maxScore);
extern void taskTdm_addSpawn(tskTdmData *data, offset const s);
extern void taskTdm_spawnAll(gamestate *gs, void *_data);

// Every `taskDefn` must have a `destroy` for memory handling, notably during gamestate destruction.
// However, whether a task can be destroyed while a game is running might vary from task to task
// (e.g., does anything else try to read its data?).
// Any writes to a task's data should be defined in the task's file, to serve as a sort of defined interface.
// "taskTdm" can be destroyed mid-game (e.g. by the creation of "taskStock").
extern void taskTdm_destroy(gamestate *gs, taskInstance *ti);

extern void defineTask_tdmScore(taskDefn *d);
