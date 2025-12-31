#define DL_VARNAME_LEN 20
enum {
	VAR_T_UNSET,
	VAR_T_INT,
	VAR_T_POS,
};
struct dl_updVar{
	char name[DL_VARNAME_LEN];
	int64_t incr;
	char seen;
	char type;
	union {
		int64_t integer;
		struct {
			offset vec;
			quat rot;
		} position;
	} value;
};

extern list<dl_updVar> dl_updVars;
extern int dl_updVarSelected;

extern int64_t* look(int64_t dist);

extern void dl_resetVars(int version);
extern int64_t var(char const *name);
extern int64_t var(char const *name, int64_t val);
extern int64_t const * pvar(char const *name, offset const val);

extern void dl_processFile(char const *filename, gamestate *gs, int myPlayer);
extern void dl_upd(gamestate *gs, int myPlayer);
extern void dl_bake(char const *name);
extern void dl_hotbar(char const *name);

extern void dl_init();
extern void dl_destroy();
