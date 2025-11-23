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

void iquat_mult(iquat ret, iquat a, iquat b){
	ret[0]=(b[0] * a[0] - b[1] * a[1] - b[2] * a[2] - b[3] * a[3])/FIXP;
	ret[1]=(b[0] * a[1] + b[1] * a[0] + b[2] * a[3] - b[3] * a[2])/FIXP;
	ret[2]=(b[0] * a[2] - b[1] * a[3] + b[2] * a[0] + b[3] * a[1])/FIXP;
	ret[3]=(b[0] * a[3] + b[1] * a[2] - b[2] * a[1] + b[3] * a[0])/FIXP;
}

// Making an `iquat` version of this one is an especial headache.
// The trouble is that we're multiplying 3 values, which is not a
// super fun FIXP time. The good news is that I'm going to assume
// the I/O vectors are unit vectors (roughly enforced via type),
// but the bad news is there's also a `*2` knocking around.
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

// Our input components should all be in [-1,1].
// Our FIXP type can hold double-precision (i.e. multiplied)
// values roughly in the range of (-2,2), but we try to
// restrict it to [-1,1] since we want there to be some leeway.
//
// Honestly I'm not sure how often this will get used;
// if we've got multiple faces to check on a single solid,
// I think it quickly becomes better to convert the quat to a matrix.
void iquat_apply(int32_t dest[3], iquat q, int32_t src[3]) {
	// The `*2` we leave out, and apply it (sneaky like)
	// when we do the conversion back to single-precision later.
	int32_t xsqj = src[0] * q[2] / FIXP;
	int32_t xsqk = src[0] * q[3] / FIXP;
	int32_t ysqk = src[1] * q[3] / FIXP;
	int32_t ysqi = src[1] * q[1] / FIXP;
	int32_t zsqi = src[2] * q[1] / FIXP;
	int32_t zsqj = src[2] * q[2] / FIXP;
	dest[0] = src[0] + ( - xsqj*q[2] - xsqk*q[3]
		             - ysqk*q[0] + ysqi*q[2]
		             + zsqi*q[3] + zsqj*q[0] ) / (FIXP/2);
	dest[1] = src[1] + ( - ysqk*q[3] - ysqi*q[1]
		             - zsqi*q[0] + zsqj*q[3]
		             + xsqj*q[1] + xsqk*q[0] ) / (FIXP/2);
	dest[2] = src[2] + ( - zsqi*q[1] - zsqj*q[2]
		             - xsqj*q[0] + xsqk*q[1]
		             + ysqk*q[2] + ysqi*q[0] ) / (FIXP/2);
}

void quat_print(quat t){
	printf("%.2f %.2f %.2f %.2f\n", t[0],t[1],t[2],t[3]);
}
