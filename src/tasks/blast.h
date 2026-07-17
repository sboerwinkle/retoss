struct tskBlastItem;

struct tskBlastBits {
	int refs;
	int32_t time;
	uint32_t seed;
	int64_t r;
	offset pos, vel;
	list<tskBlastItem> bits;
};

// Later on this will probably hold some transient state,
// once I get around to the huge TODO over tskBlast_draw.
// For now it looks pretty goofy.
struct tskBlastData {
	tskBlastBits *bb;
};

extern tskBlastData* tskBlast_create(gamestate *gs, offset oldPos, offset vel);
extern void tskBlast_draw(void *data, int32_t now);

extern void defineTask_blast(taskDefn *d);
