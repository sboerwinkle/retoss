
struct tskDynamicsData {
	solid s;
	offset vel;
	iquat rvel;
};

struct buildCtx;
extern void tskDynamics_create(gamestate *gs, buildCtx *c, offset const vel, iquat const rvel);

extern void defineTask_dynamics(taskDefn *d);
