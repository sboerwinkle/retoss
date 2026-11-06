
struct taskStockPlayer {
	char deaths;
	u8 timer;
};

// Might want an option to raise the death cap occasionally
// (each time "lowest number of deaths for any player" hits a new even number)
struct taskStockData {
	u8 maxScore;
	list<taskStockPlayer> players;
	list<offset_t> spawns;
};

extern void taskStock_draw(gamestate *gs, void *_data);
extern void taskStock_create(gamestate *gs, u8 maxScore);

extern void taskStock_define(taskDefn *d);
