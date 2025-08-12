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
#include "game.h"
#include "png.h"
#include "watch_gfx.h"

#include "graphics.h"

#define MOTTLE_TEX_RESOLUTION 64

static void populateCubeVertexData();
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
static GLint u_main_tint;

static GLint u_spr_offset;
static GLint u_spr_size;
static GLint u_spr_scale;
static GLint u_spr_tex_offset;
static GLint u_spr_tex_scale;

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

static void loadTexture() {
	char *imageData;
	int width, height;
	png_read(&imageData, &width, &height, "assets/dirt.png");
	if (!imageData) { //  || width != MOTTLE_TEX_RESOLUTION || height != MOTTLE_TEX_RESOLUTION) {
		puts("Not loading texture");
	} else {
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

void initGraphics() {
	GLuint vertexShader = mkShader(GL_VERTEX_SHADER, "shaders/solid.vert");
	GLuint spriteShader = mkShader(GL_VERTEX_SHADER, "shaders/sprite.vert");
	//GLuint vertexShader2d = mkShader(GL_VERTEX_SHADER, "shaders/flat.vert");
	GLuint fragShader = mkShader(GL_FRAGMENT_SHADER, "shaders/color.frag");
	GLuint fragShaderStipple = mkShader(GL_FRAGMENT_SHADER, "shaders/stipple.frag");

	main_prog = glCreateProgram();
	glAttachShader(main_prog, vertexShader);
	glAttachShader(main_prog, fragShader);
	glLinkProgram(main_prog);
	cerr("Post link");

	sprite_prog = glCreateProgram();
	glAttachShader(sprite_prog, spriteShader);
	glAttachShader(sprite_prog, fragShader);
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
	//GLint a_norm_id = attrib(main_prog, "a_norm"); // Temporarily out of commission haha
	GLint a_tex_st_id = attrib(main_prog, "a_tex_st");
	// sprite_prog attribs
	GLint a_spr_loc = attrib(sprite_prog, "a_loc");

	// Uniforms
	u_main_modelview = glGetUniformLocation(main_prog, "u_modelview");
	u_main_rot = glGetUniformLocation(main_prog, "u_rot");
	u_main_texscale = glGetUniformLocation(main_prog, "u_texscale");
	u_main_texoffset = glGetUniformLocation(main_prog, "u_texoffset");
	u_main_tint = glGetUniformLocation(main_prog, "u_tint");
	// sprite_prog uniforms
	u_spr_offset = glGetUniformLocation(sprite_prog, "u_offset");
	u_spr_size = glGetUniformLocation(sprite_prog, "u_size");
	u_spr_scale = glGetUniformLocation(sprite_prog, "u_scale");
	u_spr_tex_offset = glGetUniformLocation(sprite_prog, "u_tex_offset");
	u_spr_tex_scale = glGetUniformLocation(sprite_prog, "u_tex_scale");

	// Previously I checked that some uniforms are in the same spots across programs here,
	// and log + set startupFailed=1 if not.

	printGLProgErrors(main_prog, "main");
	printGLProgErrors(stipple_prog, "stipple");
	//printGLProgErrors(flat_prog, "flat");

	if (startupFailed) {
		puts("Aborting due to one or more GL startup issues");
		exit(1);
	}

	glEnable(GL_CULL_FACE);
	glGenVertexArrays(2, vaos);
	GLuint tex_noise;
	glGenTextures(1, &tex_noise);

	// vaos[0]
	glBindVertexArray(vaos[0]);
	glBindTexture(GL_TEXTURE_2D, tex_noise);
	glEnableVertexAttribArray(a_pos_id);
	//glEnableVertexAttribArray(a_norm_id);
	glEnableVertexAttribArray(a_tex_st_id);
	glGenBuffers(1, &buffer_id);
	glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
	populateCubeVertexData();
	// Position data is first
	glVertexAttribPointer(a_pos_id, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 8, (void*) 0);
	// Followed by normal data
	//glVertexAttribPointer(a_norm_id, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 8, (void*) (sizeof(GLfloat) * 3));
	// Followed by tex coord data
	glVertexAttribPointer(a_tex_st_id, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 8, (void*) (sizeof(GLfloat) * 6));
	cerr("End of vao 0 prep");
	// vaos[1]
	glBindVertexArray(vaos[1]);
	glBindTexture(GL_TEXTURE_2D, tex_noise); // Do we need to bind this once per VAO? I'm guessing so, need to check.
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

	// TODO Currently texture work is a frankenstein of the
	// random mottling texture and some "png from file" work
	// I'm doing. Eventually we may want both? Or at least
	// clean this up some.

	/*
	uint8_t tex_noise_data[MOTTLE_TEX_RESOLUTION*MOTTLE_TEX_RESOLUTION];
	uint32_t rstate = 59423;
	for(int idx = 0; idx < MOTTLE_TEX_RESOLUTION*MOTTLE_TEX_RESOLUTION; idx++){
		// Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs"
		rstate ^= rstate << 13;
		rstate ^= rstate >> 17;
		rstate ^= rstate << 5;
		tex_noise_data[idx] = (rstate & 0x1F) + 0xE0;
	}
	*/
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// When evaluating changes to the below filtering settings, you should at least evaluate the two scenarios:
	//  1. You are on a large, flat plane moving around.
	//    How crisp is it? How visible is the mipmap and sampling seam? How much does the sampling seam move as you move the camera only?
	//  2. There is a large object moving in the distance.
	//    How crisp is it? How does it look as it approaches the threshold of minification->magnification? Is there significant aliasing shimmer?
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	loadTexture();

	glClearColor(0.8, 0.8, 0.8, 1);
	glEnable(GL_BLEND);
	// It's possible to set the blend behavior of the alpha channel distinctly from that of the RGB channels.
	// This would matter if we used the DEST_ALPHA at all, but we don't, so we don't care what happens to it.
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	cerr("End of graphics setup");
}

static void checkReload() {
	// TODO document what's going on here
	if (!texReloadFlag.load(std::memory_order::acquire)) return;
	// TODO actually use texReloadPath
	loadTexture();
	texReloadFlag.store(0, std::memory_order::release);
}

void setupFrame() {
	checkReload();
	glUseProgram(sprite_prog);
	glBindVertexArray(vaos[1]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// We don't actually have to re-set this each frame,
	// but it isn't part of VAO state so it matters if you're flipping between those.
	glDisable(GL_DEPTH_TEST);

	/* Rotation stuff, how tedious. Don't need this in 2D mode!

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
	glUniform3f(u_main_tint, 1, 0.2, 0);
	// 6 faces * 2 tris/face * 3 vtx/tri = 36 vertexes to draw
	glDrawArrays(GL_TRIANGLES, 0, 36);

	*/

	// This one depends on the size of the texture.
	// Might consider shrinking this a few percent to fix issues at borders,
	// but I'd have to think about a bunch of stuff.
	glUniform2f(u_spr_tex_scale, 1.0/64, -1.0/64);
	scaleY = 1.0/256*exp(zoomLvl*0.2);
	scaleX = scaleY*displayHeight/displayWidth;
	glUniform2f(u_spr_scale, scaleX, scaleY);
	// For now all our rendered sprites are the same size, so we can set this in advance.
	glUniform2f(u_spr_size, 8, 8);

	//cerr("frame");
}

void drawSprite(int cameraX, int cameraY, int sprX, int sprY) {
	// Coordinates on the spritesheet (which is currently only 2 16x16 sprites lol)
	glUniform2f(u_spr_tex_offset, 12+20*sprX, 12+20*sprY);
	glUniform2f(
		u_spr_offset,
		cameraX,
		cameraY
	);
	glDrawArrays(GL_TRIANGLES, 0, 6); // 6 vtx = 2 tri = 1 square
}

// At some point this may be a bit more dynamic if we have other
// geometry "primitives" we want to render
static void populateCubeVertexData() {
	GLfloat boxData[36*8];
        GLfloat L = -1;
        GLfloat R =  1;
        GLfloat F = -1;
        GLfloat B =  1;
        GLfloat U =  1;
        GLfloat D = -1;
        int counter = 0;
#define vtx(x, y, z, nx, ny, nz, s, t) \
        boxData[counter++] = x; \
        boxData[counter++] = y; \
        boxData[counter++] = z; \
        boxData[counter++] = nx; \
        boxData[counter++] = ny; \
        boxData[counter++] = nz; \
        boxData[counter++] = s; \
        boxData[counter++] = t; \
// END vtx
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
	glBufferData(GL_ARRAY_BUFFER, sizeof(boxData), boxData, GL_STATIC_DRAW);
#undef vtx
}

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
