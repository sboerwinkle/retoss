/////////////////////////////////
// Most work by mboerwinkle,
// modified by sboerwinkle
/////////////////////////////////

#include <stdint.h>

// Scaling factor for fixed-point arithmetic. Multiplying two
// fixed-point numbers doubles the precision, so intermediate
// values require more space (before we shift them back). The
// biggest power of 2 that still permits the result of (1*-1)
// to fit in an int32_t without messing up the sign is 1<<15.
#define FIXP 32768

typedef float quat[4];
typedef int32_t iquat[4];
typedef int32_t unitvec[3];
typedef int32_t imat[9];
typedef int64_t offset[3];

extern void quat_norm(quat t);
extern void iquat_norm(iquat x);
extern void quat_rotateBy(quat x, quat rot);
extern void iquat_rotateBy(iquat x, iquat const rot);
/* Need to re-evaluate what to do with `strt`
extern void quat_rotX(quat ret, quat strt, float r);
extern void quat_rotY(quat ret, quat strt, float r);
extern void quat_rotZ(quat ret, quat strt, float r);
*/
extern void quat_mult(quat ret, quat a, quat b);
extern void iquat_mult(iquat ret, iquat const a, iquat const b);

extern void quat_apply(float dest[3], quat q, float src[3]);
extern void iquat_apply(unitvec dest, iquat q, unitvec const src);
extern void iquat_applySm(offset dest, iquat q, offset const src);
extern void iquat_inv(iquat dest, iquat src);
extern void quat_print(quat t);

extern void mat4Multf(float* res, float* m1, float* m2);

extern void mat3FromQuat(float* M, quat rot);
extern void mat4FromQuat(float* M, quat rot);

extern void mat3FromIquat(float *M, iquat rot);
extern void imatFromIquatInv(imat M, iquat rot);

extern void imat_applySm(offset dest, imat rot, offset const src);

extern void matEmbiggen(float M[16], float in[9], float x, float y, float z);

extern void mat4Transf(float* res, float x, float y, float z);

extern void perspective(float* m, float invSlopeX, float invSlopeY, float zNear);
