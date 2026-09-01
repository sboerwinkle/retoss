
extern "C" void lv_playground(gamestate *gs);
extern "C" void lv_tdm1(gamestate *gs);
extern "C" void lv_swarm(gamestate *gs);
extern "C" void lv_peaks(gamestate *gs);

// This one's less a level and more a special custom loader,
// but it makes more sense here than anywhere else.
extern void lv_heightmap(gamestate *gs, char const *data, int len);
