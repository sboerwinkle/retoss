
extern int64_t pl_tractMult;
extern int32_t pl_speed;
extern int64_t pl_maxNorm;
extern int64_t pl_gummy;
extern int64_t pl_jump;

extern void pl_phys_standard(unitvec const forceDir, offset const contactVel, int64_t dist, offset dest, player *p);
extern void pl_postStep(gamestate *gs, player *p);
