
extern int64_t collide_check(offset const oldPos, offset const dest, int32_t radius, solid *s, unitvec forceDir_out, offset contactVel_out, int32_t *time_out);

extern char collide_sphere(offset const p1, offset const p2, int64_t radius, mover *m, int32_t *time);
extern int64_t raycast_sphere(offset const pos, unitvec const dir, int64_t radius);

extern char raycast(fraction *best, mover *m, offset const origin, unitvec const dir);
extern char raycast_interp(fraction *best, mover *m, offset const origin1, offset const origin2, unitvec const dir, float interpRatio);
