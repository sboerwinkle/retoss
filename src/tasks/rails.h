
struct tskRailsPt {
	offset pos;
	iquat rot;
	int32_t time;
};

struct tskRailsInstructions {
	int refs;
	list<tskRailsPt> pts;
};

struct tskRailsData {
	tskRailsInstructions *instr;
	constelInst *ci;
	int32_t ic;
	int32_t time;
};

extern void tskRails_timeHelper(tskRailsData *data);
extern tskRailsData* tskRails_create(gamestate *gs, constelInst *ci);

extern void defineTask_rails(taskDefn *d);
