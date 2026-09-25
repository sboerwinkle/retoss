struct taskGrenade : disc {
	offset vel;
	u8 bounces;
	char armed;
	u16 soundId;
	// Todo Do we need some way for these to expire or be killed?
};
struct taskGrenadeExplosion {
	box *parent;
	offset p1;
	offset p2;
	u16 soundId;
};
struct taskGrenadesData {
	list<taskGrenade> l;
};

extern void taskGrenades_clear(void *_data);
extern void taskGrenades_draw(void *_data, int32_t now);

extern void taskGrenades_add(gamestate *gs, offset p1, offset vel, box *parent, u16 soundId);

extern void taskGrenades_define(taskDefn *d);
