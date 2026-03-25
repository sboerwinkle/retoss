
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
	u8 numSpawns;
	offset *spawns;
}

extern void taskTdm_draw(void *_data, float interp);

extern void defineTask_tdmScore(taskDefn *d);
