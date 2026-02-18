
extern int64_t collide_check(player *p, offset dest, int32_t radius, solid *s, unitvec forceDir_out, offset contactVel_out);
extern char raycast(fraction *best, mover *m, offset const origin, unitvec const dir);
extern char raycast_interp(fraction *best, mover *m, offset const origin1, offset const origin2, unitvec const dir, float interpRatio);
