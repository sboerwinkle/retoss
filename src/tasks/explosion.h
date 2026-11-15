
struct taskExplosionData {
	offset p1, p2;
	int64_t radius;
	int64_t launch;
	int32_t dmg;
	box *parent;
};

extern void taskExplosion_define(taskDefn *d);
extern void taskExplosion_add(gamestate *gs, offset p1, offset p2, int64_t radius, int64_t launch, int32_t dmg, box *parent);
