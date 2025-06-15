// TODO aiming to convert this to fixed-point decimals,
//      so the multiplication can be done in int32_t.

/////////////////////////////////
// Most work by mboerwinkle
/////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "quaternion.h"

void quat_norm(quat t){
	float len = t[0]*t[0]+t[1]*t[1]+t[2]*t[2]+t[3]*t[3];
	t[0]/=len;
	t[1]/=len;
	t[2]/=len;
	t[3]/=len;
}
void quat_rotateBy(quat ret, quat by){
	quat t;
	quat_mult(t, ret, by);
	memcpy(ret, t, sizeof(t));
}

// These are all busted, need to re-evaluate what I'm expecting to do with `strt`.
void quat_rotX(quat ret, quat strt, float r){
	r /= 2.0;
	float tmp[4] = {cos(r), sin(r), 0, 0};
	quat_rotateBy(ret, tmp);
}
void quat_rotY(quat ret, quat strt, float r){
	r /= 2.0;
	float tmp[4] = {cos(r), 0, sin(r), 0};
	quat_rotateBy(ret, tmp);
}
void quat_rotZ(quat ret, quat strt, float r){
	r /= 2.0;
	float tmp[4] = {cos(r), 0, 0, sin(r)};
	quat_rotateBy(ret, tmp);
}

void quat_mult(quat ret, quat a, quat b){
	ret[0]=(b[0] * a[0] - b[1] * a[1] - b[2] * a[2] - b[3] * a[3]);
	ret[1]=(b[0] * a[1] + b[1] * a[0] + b[2] * a[3] - b[3] * a[2]);
	ret[2]=(b[0] * a[2] - b[1] * a[3] + b[2] * a[0] + b[3] * a[1]);
	ret[3]=(b[0] * a[3] + b[1] * a[2] - b[2] * a[1] + b[3] * a[0]);
}
void quat_print(quat t){
	printf("%.2f %.2f %.2f %.2f\n", t[0],t[1],t[2],t[3]);
}
