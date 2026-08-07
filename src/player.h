
extern void playerUpdate(gamestate *gs, player *p);
extern void playerAddBox(gamestate *gs, player *p);
extern void pl_phys_standard(gamestate *gs, unitvec const forceDir, offset const contactVel, int64_t dist, offset dest, player *p);
extern void pl_postStep(gamestate *gs, player *p);
extern void player_hit(player *p, int hits);

extern void player_init();
extern void player_destroy();
