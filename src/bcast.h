// Caller is responsible for choosing a `b` that's small enough that we can
// safely do `fraction` math on the distances involved.
// Also, assumes the vb leaf `mover`s have their oldPos / oldRot populated.
extern void bcast_start(box *b, unitvec const dir, offset const origin);
extern mover * bcast(fraction *out_time, unitvec const dir, offset const origin);

extern void bcast_init();
extern void bcast_destroy();
