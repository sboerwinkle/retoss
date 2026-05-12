// Stuff designed for ease of use in level-building stuff.
// However, you may not want a bunch of short names in your namespace,
// so this isn't in the main "dl.h" file.

extern int64_t* look(int64_t dist);
extern void gp(char const* groupName);
extern int64_t var(char const *name);
extern int64_t var(char const *name, int64_t val);
extern int64_t const * pvar(char const *name);
extern int64_t const * pvar(char const *name, offset const val);
extern int32_t const * rvar(char const *name);
extern int32_t const * rvar(char const *name, int32_t const val[3]);
extern void pinNext();
extern void pushVarIgnore();
extern void popVarIgnore();
