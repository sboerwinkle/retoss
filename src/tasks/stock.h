
// TSK_GUNGAME also reads to this task's data, and relies on it not being removed.

struct taskStockPlayer {
	char deaths;
	u8 timer;
};

// Might want an option to raise the death cap occasionally
// (each time "lowest number of deaths for any player" hits a new even number)
struct taskStockData {
	u8 maxScore;
	u8 nextLife;
	u8 step;
	list<taskStockPlayer> players;
	list<offset_t> spawns;
};

extern void taskStock_draw(gamestate *gs, void *_data);
extern void taskStock_create(gamestate *gs, u8 maxScore, u8 step);

extern void taskStock_define(taskDefn *d);
