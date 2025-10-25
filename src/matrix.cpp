/////////////////////////////////
// Most work by mboerwinkle,
// modified by sboerwinkle
/////////////////////////////////

#include "quaternion.h"

void mat4Multf(float* res, float* m1, float* m2){
	for(int x = 0; x < 4; x++){
		for(int y = 0; y < 4; y++){
			float v = 0;
			for(int i = 0; i < 4; i++){
				v += m1[y+4*i]*m2[i+4*x];
			}
			res[y+4*x] = v;
		}
	}
}

void mat3FromQuat(float* M, quat rot){
	M[0] = 1-2*rot[2]*rot[2]-2*rot[3]*rot[3];
	M[1] = 2*rot[1]*rot[2]+2*rot[0]*rot[3];
	M[2] = 2*rot[1]*rot[3]-2*rot[0]*rot[2];

	M[3] = 2*rot[1]*rot[2]-2*rot[0]*rot[3];
	M[4] = 1-2*rot[1]*rot[1]-2*rot[3]*rot[3];
	M[5] = 2*rot[2]*rot[3]+2*rot[0]*rot[1];

	M[6] = 2*rot[1]*rot[3]+2*rot[0]*rot[2];
	M[7] = 2*rot[2]*rot[3]-2*rot[0]*rot[1];
	M[8] = 1-2*rot[1]*rot[1]-2*rot[2]*rot[2];
}

void mat4FromQuat(float* M, quat rot){
	M[ 0] = 1-2*rot[2]*rot[2]-2*rot[3]*rot[3];
	M[ 1] = 2*rot[1]*rot[2]+2*rot[0]*rot[3];
	M[ 2] = 2*rot[1]*rot[3]-2*rot[0]*rot[2];
	M[ 3] = 0;
	M[ 4] = 2*rot[1]*rot[2]-2*rot[0]*rot[3];
	M[ 5] = 1-2*rot[1]*rot[1]-2*rot[3]*rot[3];
	M[ 6] = 2*rot[2]*rot[3]+2*rot[0]*rot[1];
	M[ 7] = 0;
	M[ 8] = 2*rot[1]*rot[3]+2*rot[0]*rot[2];
	M[ 9] = 2*rot[2]*rot[3]-2*rot[0]*rot[1];
	M[10] = 1-2*rot[1]*rot[1]-2*rot[2]*rot[2];
	M[11] = 0;
	M[12] = 0;
	M[13] = 0;
	M[14] = 0;
	M[15] = 1;
}

void matEmbiggen(float M[16], float in[9], float x, float y, float z) {
	M[ 0] = in[0];
	M[ 1] = in[1];
	M[ 2] = in[2];
	M[ 3] = 0;
	M[ 4] = in[3];
	M[ 5] = in[4];
	M[ 6] = in[5];
	M[ 7] = 0;
	M[ 8] = in[6];
	M[ 9] = in[7];
	M[10] = in[8];
	M[11] = 0;
	M[12] = x;
	M[13] = y;
	M[14] = z;
	M[15] = 1;
}

void mat4Transf(float* res, float x, float y, float z){
	res[12] += x;
	res[13] += y;
	res[14] += z;
}

//https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml
// Modified for infinite zFar, since the precision losses are finite and I really like
// really big zFar anyway.
// Layout is X_gives, Y_gives, Z_gives, W_gives
// (i.e. each group of 4 corresponds to one _input_ component)
void perspective(float* m, float invSlopeX, float invSlopeY, float zNear) {
	float const tmp_debug_offset = 0;
	m[ 0] =  invSlopeX; 
	m[ 1] =  0;
	m[ 2] =  0;
	m[ 3] =  0;

	m[ 4] =  0;
	m[ 5] =  0;
	m[ 6] =  1;
	m[ 7] =  1;

	m[ 8] =  0;
	m[ 9] =  invSlopeY;
	m[10] =  0;
	m[11] =  0;

	m[12] =  0;
	m[13] =  0;
	m[14] = -2*zNear + tmp_debug_offset;
	m[15] =  tmp_debug_offset;
}
