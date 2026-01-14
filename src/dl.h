#include "mtx.h"

#define DL_VARNAME_LEN 20
enum {
	VAR_T_UNSET,
	VAR_T_INT,
	VAR_T_POS,
	VAR_T_ROT,
};
struct dl_var {
	char name[DL_VARNAME_LEN];
	int64_t incr;
	char seen;
	char touched;
	char type;
	union {
		int64_t integer;
		struct {
			offset vec;
			quat rot;
		} position;
		struct {
			int32_t rotParams[3];
			int angles[3];
		} rotation;
	} value;
};
struct dl_varGroup {
	char name[DL_VARNAME_LEN];
	char seen;
	char touched;
	list<dl_var> vars;

	void init();
	void destroy();
};

extern mtx_t dl_varMtx;
extern dl_varGroup *dl_selectedGroup;
extern int dl_selectedVar;

extern int64_t* look(int64_t dist);

extern void gp(char const* groupName);
extern void dl_selectGp(char const* groupName);

extern int64_t var(char const *name);
extern int64_t var(char const *name, int64_t val);
extern int64_t const * pvar(char const *name);
extern int64_t const * pvar(char const *name, offset const val);
extern int32_t const * rvar(char const *name);
extern int32_t const * rvar(char const *name, int32_t const val[3]);

extern void dl_processFile(char const *filename, gamestate *gs, int myPlayer);
extern void dl_upd(gamestate *gs, int myPlayer);
extern void dl_lookAtGp(gamestate *gs, int myPlayer);
extern void dl_bake();
extern void dl_hotbar(char const *name);

extern void dl_init();
extern void dl_destroy();
