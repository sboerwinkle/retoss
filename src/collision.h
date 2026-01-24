
struct fraction {
	int64_t numer;
	// Not sure what the type should be here. `denom` should probably come from a unitvec (so within FIXP),
	// but we're going to be doing lots of multiplications against `numer` so I assume it's faster to keep
	// it in the same type?
	int64_t denom;
	char lt(fraction const &other) const;
};

extern int64_t collide_check(player *p, offset dest, int32_t radius, solid *s, unitvec forceDir_out, offset contactVel_out);
extern char raycast(fraction *best, solid *s, offset origin, unitvec dir);
