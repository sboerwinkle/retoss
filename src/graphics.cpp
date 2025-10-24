#include <math.h>
#include <stdio.h>
#include <GL/gl.h>
#include <GLES3/gl3.h>
#include <string.h>

#include "list.h"
#include "file.h"
#include "util.h"
#include "quaternion.h"
#include "matrix.h"
#include "game_graphics.h"
#include "png.h"
#include "watch_gfx.h"

#include "graphics.h"

#define NUM_TEXS 2
char const * const texSrcFiles[NUM_TEXS] = {"dirt.png", "guy.png"};
GLuint textures[NUM_TEXS];

static void populateCubeVertexData(list<GLfloat> *data);
static void populateCubeVertexData2(list<GLfloat> *data);
static void populateSpriteVertexData();

int displayWidth = 0;
int displayHeight = 0;
float scaleX, scaleY;

static char startupFailed = 0;
static GLuint main_prog;
static GLuint sprite_prog;
static GLuint stipple_prog;
// static GLuint flat_prog; // Will need this later, but don't feel like reworking shader rn

static GLuint buffer_id, spr_buffer_id;
static GLuint vaos[2];

static GLint u_main_modelview;
static GLint u_main_rot;
static GLint u_main_texscale;
static GLint u_main_texoffset;
//static GLint u_main_tint;

static GLint u_spr_offset;
static GLint u_spr_size;
static GLint u_spr_scale;
static GLint u_spr_tex_offset;
static GLint u_spr_tex_scale;

// Where vertexes for a given shape start in our big buffer of vertex data.
// There's probably a more standard way of doing this!
static int vtxIdx_cubeOneFace = -1, vtxIdx_cubeSixFace = -1;

static char glMsgBuf[3000]; // Is allocating all of this statically a bad idea? IDK
static void printGLProgErrors(GLuint prog, const char *name){
	GLint ret = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &ret);
	if (ret != GL_TRUE) {
		printf("Link status for program \"%s\": %d\n", name, ret);
		startupFailed = 1;
	}
	//glGetProgramiv(prog, GL_ATTACHED_SHADERS, &ret);
	//printf("Attached Shaders: %d\n", ret);

	glGetProgramInfoLog(prog, 3000, NULL, glMsgBuf);
	// If the returned string is non-empty, print it
	if (glMsgBuf[0]) printf("GL Info Log for program \"%s\":\n%s\n", name, glMsgBuf);
}
static void printGLShaderErrors(GLuint shader, const char *path) {
	glGetShaderInfoLog(shader, 3000, NULL, glMsgBuf);
	// If the returned string is non-empty, print it
	if (glMsgBuf[0]) printf("GL Info Log for shader at %s:\n%s\n", path, glMsgBuf);
}
static GLint attrib(GLuint prog, char const *name) {
	GLint ret = glGetAttribLocation(prog, name);
	if (ret == -1) {
		printf("GL reports no attribute \"%s\" in requested program\n", name);
		startupFailed = 1;
	}
	return ret;
}

static void cerr(const char* msg){
	while(1) {
		int err = glGetError();
		if (!err) return;
		printf("Error (%s): %d\n", msg, err);
	}
}

void setDisplaySize(int width, int height){
	displayWidth = width;
	displayHeight = height;
	glViewport(0, 0, displayWidth, displayHeight);
}

static GLuint mkShader(GLenum type, const char* path) {
	GLuint shader = glCreateShader(type);

	list<char> contents;
	contents.init();
	readSystemFile(path, &contents);
	// Convert `int` to `GLint` (but they're probably the same size anyway...)
	GLint num = contents.num;
	// The '&'s here are because GL wants a couple arrays (of strings, and of lengths).
	// However, we specify only 1 element, so our "arrays" are just pointers to things.
	glShaderSource(shader, 1, &contents.items, &num);
	contents.destroy();

	glCompileShader(shader);
	printGLShaderErrors(shader, path);
	return shader;
}

static void loadTexture(int i) {
	char *imageData;
	int width, height;
	char path[200];
	snprintf(path, 200, "assets/%s", texSrcFiles[i]);
	png_read(&imageData, &width, &height, path);
	if (!imageData) {
		printf("Not loading texture %d\n", i);
	} else {
		glBindTexture(GL_TEXTURE_2D, textures[i]);
		// We're going to assume the correct texture is bound!
		// That's the nice thing about only using one texture.
		glTexImage2D(
			GL_TEXTURE_2D, 0, GL_RGBA8,
			width, height,
			0,
			GL_RGBA, GL_UNSIGNED_BYTE, imageData
		);
		// Apparently you can set manual mipmaps, but we don't care about that much
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	free(imageData);
}

static void loadAllTextures() {
	range(i, NUM_TEXS) loadTexture(i);
}

void initGraphics() {
	GLuint vertexShader = mkShader(GL_VERTEX_SHADER, "shaders/solid.vert");
	GLuint spriteShader = mkShader(GL_VERTEX_SHADER, "shaders/sprite.vert");
	//GLuint vertexShader2d = mkShader(GL_VERTEX_SHADER, "shaders/flat.vert");
	GLuint fragShader = mkShader(GL_FRAGMENT_SHADER, "shaders/color.frag");
	GLuint fragShaderSprite = mkShader(GL_FRAGMENT_SHADER, "shaders/texOnly.frag");
	GLuint fragShaderStipple = mkShader(GL_FRAGMENT_SHADER, "shaders/stipple.frag");

	main_prog = glCreateProgram();
	glAttachShader(main_prog, vertexShader);
	glAttachShader(main_prog, fragShader);
	glLinkProgram(main_prog);
	cerr("Post link");

	sprite_prog = glCreateProgram();
	glAttachShader(sprite_prog, spriteShader);
	glAttachShader(sprite_prog, fragShaderSprite);
	glLinkProgram(sprite_prog);
	cerr("Post link");

	stipple_prog = glCreateProgram();
	glAttachShader(stipple_prog, vertexShader);
	glAttachShader(stipple_prog, fragShaderStipple);
	glLinkProgram(stipple_prog);
	cerr("Post link");

	/*
	flat_prog = glCreateProgram();
	glAttachShader(flat_prog, vertexShader2d);
	glAttachShader(flat_prog, fragShader);
	glLinkProgram(flat_prog);
	cerr("Post link");
	*/

	// These will be the same attrib locations as stipple_prog, since they use the same vertex shader
	GLint a_pos_id = attrib(main_prog, "a_pos");
	GLint a_norm_id = attrib(main_prog, "a_norm");
	GLint a_tex_st_id = attrib(main_prog, "a_tex_st");
	// sprite_prog attribs
	GLint a_spr_loc = attrib(sprite_prog, "a_loc");

	// Uniforms
	u_main_modelview = glGetUniformLocation(main_prog, "u_modelview");
	u_main_rot = glGetUniformLocation(main_prog, "u_rot");
	u_main_texscale = glGetUniformLocation(main_prog, "u_texscale");
	u_main_texoffset = glGetUniformLocation(main_prog, "u_texoffset");
	//u_main_tint = glGetUniformLocation(main_prog, "u_tint");
	// sprite_prog uniforms
	u_spr_offset = glGetUniformLocation(sprite_prog, "u_offset");
	u_spr_size = glGetUniformLocation(sprite_prog, "u_size");
	u_spr_scale = glGetUniformLocation(sprite_prog, "u_scale");
	u_spr_tex_offset = glGetUniformLocation(sprite_prog, "u_tex_offset");
	u_spr_tex_scale = glGetUniformLocation(sprite_prog, "u_tex_scale");

	// Previously I checked that some uniforms are in the same spots across programs here,
	// and log + set startupFailed=1 if not.

	printGLProgErrors(main_prog, "main");
	printGLProgErrors(sprite_prog, "sprite");
	printGLProgErrors(stipple_prog, "stipple");
	//printGLProgErrors(flat_prog, "flat");

	if (startupFailed) {
		puts("Aborting due to one or more GL startup issues");
		exit(1);
	}

	glEnable(GL_CULL_FACE);
	glGenVertexArrays(2, vaos);
	glGenTextures(NUM_TEXS, textures);

	list<GLfloat> vtxData;
	vtxData.init();
	vtxIdx_cubeOneFace = 0;
	populateCubeVertexData(&vtxData);
	vtxIdx_cubeSixFace = vtxData.num / 8; // 8 is our "stride", I think it's called
	populateCubeVertexData2(&vtxData);

	// vaos[0]
	glBindVertexArray(vaos[0]);
	glEnableVertexAttribArray(a_pos_id);
	glEnableVertexAttribArray(a_norm_id);
	glEnableVertexAttribArray(a_tex_st_id);

	// I swear I had this written down somewhere... Anyway,
	// I'm pretty sure the bound buffer isn't part of VAO state, but it is recorded when
	// vertex attributes are configured (below). So vertex data doesn't care what's bound at draw time,
	// it already knows which buffer it's reading from.
	glGenBuffers(1, &buffer_id);
	glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*vtxData.num, vtxData.items, GL_STATIC_DRAW);
	vtxData.destroy();

	// Position data is first
	glVertexAttribPointer(a_pos_id, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 8, (void*) 0);
	// Followed by normal data
	glVertexAttribPointer(a_norm_id, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 8, (void*) (sizeof(GLfloat) * 3));
	// Followed by tex coord data
	glVertexAttribPointer(a_tex_st_id, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 8, (void*) (sizeof(GLfloat) * 6));
	cerr("End of vao 0 prep");

	// vaos[1]
	glBindVertexArray(vaos[1]);
	glEnableVertexAttribArray(a_spr_loc);
	glGenBuffers(1, &spr_buffer_id);
	glBindBuffer(GL_ARRAY_BUFFER, spr_buffer_id);
	populateSpriteVertexData();
	glVertexAttribPointer(a_spr_loc, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, (void*) 0);
	cerr("End of vao 1 prep");

	/*
	// vaos[1]
	glBindVertexArray(vaos[1]);
	glEnableVertexAttribArray(a_flat_loc_id);
	initFont();
	glVertexAttribPointer(a_flat_loc_id, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*) 0);
	cerr("End of vao 1 prep");
	*/

	// Turns out GL_REPEAT is the default.
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// When evaluating changes to the below filtering settings, you should at least evaluate the two scenarios:
	//  1. You are on a large, flat plane moving around.
	//    How crisp is it? How visible is the mipmap and sampling seam? How much does the sampling seam move as you move the camera only?
	//  2. There is a large object moving in the distance.
	//    How crisp is it? How does it look as it approaches the threshold of minification->magnification? Is there significant aliasing shimmer?
	range(i, NUM_TEXS) {
		glBindTexture(GL_TEXTURE_2D, textures[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}

	loadAllTextures();

	glClearColor(0.2, 0.2, 0.2, 1);

	/// Stuff for transparency
	// glEnable(GL_BLEND);
	// // It's possible to set the blend behavior of the alpha channel distinctly from that of the RGB channels.
	// // This would matter if we used the DEST_ALPHA at all, but we don't, so we don't care what happens to it.
	// glBlendEquation(GL_FUNC_ADD);
	// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	///

	cerr("End of graphics setup");
}

static void checkReload() {
	// I go into more detail about the use of std::memory_order in watch.cpp,
	// but the short version is that we're getting some guarantees similar to
	// a very simple semaphore.
	if (!texReloadFlag.load(std::memory_order::acquire)) return;

	range(i, NUM_TEXS) {
		if (!strcmp(texReloadPath, texSrcFiles[i])) {
			loadTexture(i);
			goto success;
		}
	}
	// Never thought I'd miss python's for-else...
	printf("We don't care about '%s'\n", texReloadPath);
	success:;

	texReloadFlag.store(0, std::memory_order::release);
}

void setupFrame() {
	checkReload();
	glUseProgram(main_prog);
	glBindVertexArray(vaos[0]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// We don't actually have to re-set this each frame,
	// but it isn't part of VAO state so it matters if you're flipping between those.
	glEnable(GL_DEPTH_TEST);

	// This will represent the rotation the world goes through
	// to sit it in front of the camera, more accurately.
	quat cameraRotation = {1,0,0,0};

	// TODO stuff with actual position haha

	GLfloat modelview_data[16], rot_data[9];

	quat thingRotation;
	memcpy(thingRotation, tmpGameRotation, sizeof(thingRotation));
	// This is intentionally before `cameraRotation` is applied,
	// lighting doesn't rotate with the camera.
	mat3FromQuat(rot_data, thingRotation);

	quat modelCamRotation;
	float modelCam_mat[16];
	quat_mult(modelCamRotation, thingRotation, cameraRotation);
	mat4FromQuat(modelCam_mat, modelCamRotation);
	//mat4Transf(modelCam_mat, 0, 0, 0); // TODO as stated above, actual position is not done haha

	float persp_mat[16];
	float fovThingIdk = 1/0.7;
	perspective(
		persp_mat,
		fovThingIdk*displayHeight/displayWidth,
		fovThingIdk,
		0.1 // zNear
	);
	mat4Multf(modelview_data, persp_mat, modelCam_mat);

	glUniformMatrix4fv(u_main_modelview, 1, GL_FALSE, modelview_data);
	glUniformMatrix3fv(u_main_rot, 1, GL_FALSE, rot_data);

	// Cube drawing stuff. Plenty to do here, like figure in model rotation.
	glUniform1f(u_main_texscale, 1);
	glUniform2f(u_main_texoffset, 0, 0);
	// Is this okay to be doing so often? Hope so!
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	// 6 faces * 2 tris/face * 3 vtx/tri = 36 vertexes to draw
	glDrawArrays(GL_TRIANGLES, vtxIdx_cubeSixFace, 36);

	/* 2D stuff, how passé

	// This one depends on the size of the texture.
	// Might consider shrinking this a few percent to fix issues at borders,
	// but I'd have to think about a bunch of stuff.
	glUniform2f(u_spr_tex_scale, 1.0/64, -1.0/64);
	scaleY = 1.0/256*exp(zoomLvl*0.2);
	scaleX = scaleY*displayHeight/displayWidth;
	glUniform2f(u_spr_scale, scaleX, scaleY);
	// For now all our rendered sprites are the same size, so we can set this in advance.
	glUniform2f(u_spr_size, 8, 8);

	*/

	//cerr("frame");
}

void drawSprite(int cameraX, int cameraY, int sprX, int sprY) {
	// Coordinates on the spritesheet
	glUniform2f(u_spr_tex_offset, 12+20*sprX, 12+20*sprY);
	glUniform2f(
		u_spr_offset,
		cameraX,
		cameraY
	);
	glDrawArrays(GL_TRIANGLES, 0, 6); // 6 vtx = 2 tri = 1 square
}

#define vtx(x, y, z, nx, ny, nz, s, t) \
        data->add(x); \
        data->add(y); \
        data->add(z); \
        data->add(nx); \
        data->add(ny); \
        data->add(nz); \
        data->add(s); \
        data->add(t); \
// END vtx

// At some point this may be a bit more dynamic if we have other
// geometry "primitives" we want to render
static void populateCubeVertexData(list<GLfloat> *data) {
	GLfloat L = -1, R = 1;
	GLfloat F = -1, B = 1;
	GLfloat D = -1, U = 1;
	// up
	vtx(L, B, U, 0, 0, U, 0, 0);
	vtx(L, F, U, 0, 0, U, 0, 1);
	vtx(R, B, U, 0, 0, U, 1, 0);
	vtx(R, F, U, 0, 0, U, 1, 1);
	vtx(R, B, U, 0, 0, U, 1, 0);
	vtx(L, F, U, 0, 0, U, 0, 1);
	// front
	vtx(L, F, U, 0, F, 0, 0, 0);
	vtx(L, F, D, 0, F, 0, 0, 1);
	vtx(R, F, U, 0, F, 0, 1, 0);
	vtx(R, F, D, 0, F, 0, 1, 1);
	vtx(R, F, U, 0, F, 0, 1, 0);
	vtx(L, F, D, 0, F, 0, 0, 1);
	// right
	vtx(R, F, U, R, 0, 0, 0, 0);
	vtx(R, F, D, R, 0, 0, 0, 1);
	vtx(R, B, U, R, 0, 0, 1, 0);
	vtx(R, B, D, R, 0, 0, 1, 1);
	vtx(R, B, U, R, 0, 0, 1, 0);
	vtx(R, F, D, R, 0, 0, 0, 1);
	// left
	vtx(L, B, U, L, 0, 0, 0, 0);
	vtx(L, B, D, L, 0, 0, 0, 1);
	vtx(L, F, U, L, 0, 0, 1, 0);
	vtx(L, F, D, L, 0, 0, 1, 1);
	vtx(L, F, U, L, 0, 0, 1, 0);
	vtx(L, B, D, L, 0, 0, 0, 1);
	// back
	vtx(R, B, U, 0, B, 0, 0, 0);
	vtx(R, B, D, 0, B, 0, 0, 1);
	vtx(L, B, U, 0, B, 0, 1, 0);
	vtx(L, B, D, 0, B, 0, 1, 1);
	vtx(L, B, U, 0, B, 0, 1, 0);
	vtx(R, B, D, 0, B, 0, 0, 1);
	// down
	vtx(L, F, D, 0, 0, D, 0, 0);
	vtx(L, B, D, 0, 0, D, 0, 1);
	vtx(R, F, D, 0, 0, D, 1, 0);
	vtx(R, B, D, 0, 0, D, 1, 1);
	vtx(R, F, D, 0, 0, D, 1, 0);
	vtx(L, B, D, 0, 0, D, 0, 1);
}

static void populateCubeVertexData2(list<GLfloat> *data) {
	GLfloat L = -1, R = 1;
	GLfloat F = -1, B = 1;
	GLfloat D = -1, U = 1;

	GLfloat p1 =   1.0/128;
	GLfloat p2 =  43.0/128;
	GLfloat p3 =  85.0/128;
	GLfloat p4 = 127.0/128;

	GLfloat p5 =  86.0/128;
	GLfloat p6 = 128.0/128;

	// up
	vtx(L, B, U, 0, 0, U, p2, p1);
	vtx(L, F, U, 0, 0, U, p2, p2);
	vtx(R, B, U, 0, 0, U, p3, p1);
	vtx(R, F, U, 0, 0, U, p3, p2);
	vtx(R, B, U, 0, 0, U, p3, p1);
	vtx(L, F, U, 0, 0, U, p2, p2);
	// front
	vtx(L, F, U, 0, F, 0, p2, p2);
	vtx(L, F, D, 0, F, 0, p2, p3);
	vtx(R, F, U, 0, F, 0, p3, p2);
	vtx(R, F, D, 0, F, 0, p3, p3);
	vtx(R, F, U, 0, F, 0, p3, p2);
	vtx(L, F, D, 0, F, 0, p2, p3);
	// right
	vtx(R, F, U, R, 0, 0, p3, p2);
	vtx(R, F, D, R, 0, 0, p3, p3);
	vtx(R, B, U, R, 0, 0, p4, p2);
	vtx(R, B, D, R, 0, 0, p4, p3);
	vtx(R, B, U, R, 0, 0, p4, p2);
	vtx(R, F, D, R, 0, 0, p3, p3);
	// left
	vtx(L, B, U, L, 0, 0, p1, p2);
	vtx(L, B, D, L, 0, 0, p1, p3);
	vtx(L, F, U, L, 0, 0, p2, p2);
	vtx(L, F, D, L, 0, 0, p2, p3);
	vtx(L, F, U, L, 0, 0, p2, p2);
	vtx(L, B, D, L, 0, 0, p1, p3);
	// back
	vtx(R, B, U, 0, B, 0, p5, p5);
	vtx(R, B, D, 0, B, 0, p5, p6);
	vtx(L, B, U, 0, B, 0, p6, p5);
	vtx(L, B, D, 0, B, 0, p6, p6);
	vtx(L, B, U, 0, B, 0, p6, p5);
	vtx(R, B, D, 0, B, 0, p5, p6);
	// down
	vtx(L, F, D, 0, 0, D, p2, p3);
	vtx(L, B, D, 0, 0, D, p2, p4);
	vtx(R, F, D, 0, 0, D, p3, p3);
	vtx(R, B, D, 0, 0, D, p3, p4);
	vtx(R, F, D, 0, 0, D, p3, p3);
	vtx(L, B, D, 0, 0, D, p2, p4);
}

#undef vtx

static void populateSpriteVertexData() {
	GLfloat data[12];
        GLfloat L = -1;
        GLfloat R =  1;
        GLfloat U = -1;
        GLfloat D =  1;
        int counter = 0;
#define vtx(x, y) \
        data[counter++] = x; \
        data[counter++] = y; \
// END vtx
	vtx(L,U);
	vtx(R,U);
	vtx(L,D);
	vtx(L,D);
	vtx(R,U);
	vtx(R,D);
	glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
#undef vtx
}
