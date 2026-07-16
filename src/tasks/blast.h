
struct tskBlastBits {
	int refs;
	list<int64_t> bits;
};

struct tskBlastData {
	int32_t time;
	uint32_t seed;
	int64_t r;
	tskBlastBits *bb;
};

extern tskBlastData* tskBlast_create(gamestate *gs, buildCtx *b);

extern void defineTask_blast(taskDefn *d);
