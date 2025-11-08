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
void quat_rotateBy(quat x, quat rot){
	quat t;
	quat_mult(t, x, rot);
	memcpy(x, t, sizeof(t));
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

void quat_apply(float dest[3], quat q, float src[3]) {
	float x2sqj = src[0] * 2 * q[2];
	float x2sqk = src[0] * 2 * q[3];
	float y2sqk = src[1] * 2 * q[3];
	float y2sqi = src[1] * 2 * q[1];
	float z2sqi = src[2] * 2 * q[1];
	float z2sqj = src[2] * 2 * q[2];
	dest[0] = src[0] - x2sqj*q[2] - x2sqk*q[3]
		         - y2sqk*q[0] + y2sqi*q[2]
		         + z2sqi*q[3] + z2sqj*q[0];
	dest[1] = src[1] - y2sqk*q[3] - y2sqi*q[1]
		         - z2sqi*q[0] + z2sqj*q[3]
		         + x2sqj*q[1] + x2sqk*q[0];
	dest[2] = src[2] - z2sqi*q[1] - z2sqj*q[2]
		         - x2sqj*q[0] + x2sqk*q[1]
		         + y2sqk*q[2] + y2sqi*q[0];
}

void quat_print(quat t){
	printf("%.2f %.2f %.2f %.2f\n", t[0],t[1],t[2],t[3]);
}
