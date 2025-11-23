/////////////////////////////////
// Most work by mboerwinkle,
// modified by sboerwinkle
/////////////////////////////////

#include "quaternion.h"

extern void mat4Multf(float* res, float* m1, float* m2);

extern void mat3FromQuat(float* M, quat rot);
extern void mat4FromQuat(float* M, quat rot);

extern void mat3FromIquat(float *M, iquat rot);

extern void matEmbiggen(float M[16], float in[9], float x, float y, float z);

extern void mat4Transf(float* res, float x, float y, float z);

extern void perspective(float* m, float invSlopeX, float invSlopeY, float zNear);
