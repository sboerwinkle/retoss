#pragma once

/////////////////////////////////
// Most work by mboerwinkle
/////////////////////////////////

// Scaling factor for fixed-point arithmetic. Multiplying two
// fixed-point numbers doubles the precision, so intermediate
// values require more space (before we shift them back). The
// biggest power of 2 that still permits the result of (1*-1)
// to fit in an int32_t without messing up the sign is 1<<15.
#define FIXP 32768

// TODO going to rework this some day to be int32_t...
typedef float quat[4];

extern void quat_norm(quat t);
extern void quat_rotateBy(quat x, quat rot);
/* Need to re-evaluate what to do with `strt`
extern void quat_rotX(quat ret, quat strt, float r);
extern void quat_rotY(quat ret, quat strt, float r);
extern void quat_rotZ(quat ret, quat strt, float r);
*/
extern void quat_mult(quat ret, quat a, quat b);

extern void quat_apply(float dest[3], quat q, float src[3]);
extern void quat_print(quat t);
