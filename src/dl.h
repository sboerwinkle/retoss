#define DL_VARNAME_LEN 20
struct dl_updVar{
	char name[DL_VARNAME_LEN];
	int64_t value;
	int64_t incr;
	char inUse;
};

extern list<dl_updVar> dl_updVars;
extern int dl_updVarSelected;

extern void dl_resetVars(int version);
extern int64_t var(char const *name);
extern int64_t var(char const *name, int64_t val);

extern void dl_processFile(char const *filename, gamestate *gs);
extern void dl_upd(gamestate *gs);

extern void dl_init();
extern void dl_destroy();
