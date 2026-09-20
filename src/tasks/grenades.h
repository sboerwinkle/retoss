struct taskGrenade : disc {
	offset vel;
	u8 bounces;
	char team;
	// TODO Do we need some way for these to expire or be killed?
};
struct taskGrenadeExplosion {
	box *parent;
	offset p1;
	offset p2;
};
struct taskGrenadesData {
	list<taskGrenade> l;
};

extern void taskGrenades_draw(void *_data);

extern void taskGrenades_add(gamestate *gs, offset p1, offset vel, box *parent, char team, uint32_t soundId);

extern void taskGrenades_define(taskDefn *d);
