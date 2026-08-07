
struct taskRocket {
	mover m;
	offset vel;
	offset accel;
	int32_t ttl;
};
#define rocketFromMover(x) ((taskRocket*)((char*)(x) - offsetof(taskRocket, m)))

extern void taskRocket_draw(void *_data);
extern void taskRocket_create(gamestate *gs, offset p1, offset vel, unitvec dir, box *parent);
extern void taskRocket_define(taskDefn *t);
