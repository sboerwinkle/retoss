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

extern void quat_norm(float* t);
/* Need to re-evaluate what to do with `strt`
extern void quat_rotX(float* ret, float* strt, float r);
extern void quat_rotY(float* ret, float* strt, float r);
extern void quat_rotZ(float* ret, float* strt, float r);
*/
extern void quat_mult(float* ret, float* a, float* b);
extern void quat_print(float* t);
