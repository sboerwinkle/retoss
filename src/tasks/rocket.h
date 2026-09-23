
struct taskRocket {
	mover m;
	offset vel;
	offset accel;
	bool dead, live;
	uint16_t ttl;
	uint16_t soundId;
};
#define rocketFromMover(x) ((taskRocket*)((char*)(x) - offsetof(taskRocket, m)))

extern void taskRocket_draw(void *_data);
extern void taskRocket_create(gamestate *gs, offset p1, offset vel, unitvec dir, box *parent, u16 soundId);
extern void taskRocket_define(taskDefn *t);
