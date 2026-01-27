#include <math.h>
#include <stdio.h>
#include <GL/gl.h>
#include <GLES3/gl3.h>
#include <string.h>

#include "list.h"
#include "file.h"
#include "util.h"
#include "matrix.h"
#include "game_graphics.h"
#include "png.h"
#include "watch_flags.h"
#include "gamestate.h"

#include "graphics.h"
#include "graphics_callbacks.h"

#define NUM_TEXS 6
#define TEX_MOTTLE 0
#define TEX_FONT 1
char const * const texSrcFiles[NUM_TEXS] = {"", "font.png", "dirt.png", "guy.png", "stop.png", "wall.png"};
GLuint textures[NUM_TEXS];

static void populateCubeVertexData(list<GLfloat> *data, float x, float y, float z);
static void populateCubeVertexData2(list<GLfloat> *data);
static void populateSpriteVertexData();

int displayWidth = 0;
int displayHeight = 0;
float scaleX, scaleY;
float displayAreaBounds[2];

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

static GLint u_spr_size;
static GLint u_spr_scale;
static GLint u_spr_screen_offset;
static GLint u_spr_tex_scale;
static GLint u_spr_tex_offset;

// Where vertexes for a given shape start in our big buffer of vertex data.
// There's probably a more standard way of doing this!
static int vtxIdx_cubeOneFace = -1;
static int vtxIdx_slabOneFace = -1;
static int vtxIdx_poleOneFace = -1;
static int vtxIdx_cubeSixFace = -1;

static float matWorldToScreen[16];
static int64_t *camPos;

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

static void loadMottleTex() {
	int const res = 64;
	uint8_t tex_noise_data[res*res];
	uint32_t rstate = 59423;
	for(int idx = 0; idx < res*res; idx++){
		// Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs"
		rstate ^= rstate << 13;
		rstate ^= rstate >> 17;
		rstate ^= rstate << 5;
		tex_noise_data[idx] = rstate & 0xFF;
	}
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, textures[TEX_MOTTLE]);
	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_R8,
		res, res,
		0,
		GL_RED, GL_UNSIGNED_BYTE, tex_noise_data
	);
	glGenerateMipmap(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);
}

static void loadAllTextures() {
	loadMottleTex();
	for (int i = 1; i < NUM_TEXS; i++) loadTexture(i);
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
	u_spr_size = glGetUniformLocation(sprite_prog, "u_size");
	u_spr_scale = glGetUniformLocation(sprite_prog, "u_scale");
	u_spr_screen_offset = glGetUniformLocation(sprite_prog, "u_offset");
	u_spr_tex_scale = glGetUniformLocation(sprite_prog, "u_tex_scale");
	u_spr_tex_offset = glGetUniformLocation(sprite_prog, "u_tex_offset");

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
	populateCubeVertexData(&vtxData, 1, 1, 1);
	vtxIdx_slabOneFace = vtxData.num / 8; // 8 is our "stride", I think it's called
	populateCubeVertexData(&vtxData, 1, 1, 1.0/8);
	vtxIdx_poleOneFace = vtxData.num / 8;
	populateCubeVertexData(&vtxData, 1.0/8, 1, 1.0/8);
	vtxIdx_cubeSixFace = vtxData.num / 8;
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

	// I think these settings stick around, but they don't "apply" until
	// we enable GL_BLEND.
	// It's possible to set the blend behavior of the alpha channel distinctly from that of the RGB channels.
	// This would matter if we used the DEST_ALPHA at all, but we don't, so we don't care what happens to it.
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	cerr("End of graphics setup");
}

static void checkReload() {
	// I go into more detail about the use of std::memory_order in watch.cpp,
	// but the short version is that we're getting some guarantees similar to
	// a very simple semaphore.
	if (!texReloadFlag.load(std::memory_order::acquire)) return;

	// Skip mottle tex, it isn't read from file
	for (int i = 1; i < NUM_TEXS; i++) {
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

void setupFrame(int64_t *_camPos) {
	checkReload();
	glUseProgram(main_prog);
	glBindVertexArray(vaos[0]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Some settings differ from the 2D drawing state.
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	float matWorldToCam[16];
	// Grab the camera rotation and reverse it
	quat quatWorldToCam;
	memcpy(quatWorldToCam, quatCamRotation, sizeof(quat));
	quatWorldToCam[1] *= -1;
	quatWorldToCam[2] *= -1;
	quatWorldToCam[3] *= -1;
	mat4FromQuat(matWorldToCam, quatWorldToCam);

	// Todo I'm sure I'm wasting some multiplications here.
	//      Maybe a pointless optimization, but for both of the 4x4 matrix multiplications
	//      we do, we know some of the values are constants like 0 or 1 going in.

	// This is the same no matter what (or where) we're rending, so I can do that here...
	float matPersp[16];
	float fovThingIdk = 1/0.7;
	perspective(
		matPersp,
		fovThingIdk*displayHeight/displayWidth,
		fovThingIdk,
		100 // zNear
	);

	mat4Multf(matWorldToScreen, matPersp, matWorldToCam);

	camPos = _camPos;
}

void drawCube(mover *m, int64_t scale, int tex, int mesh, float interpRatio) {
	// Will need scaling (and mottling) eventually
	if (tex < 0 || tex >= NUM_TEXS) {
		printf("ERROR: Invalid tex %d\n", tex);
		return;
	}
	// The rotation of the thing itself (used for lighting).
	// Todo: Use `interpRatio` here somehow.
	GLfloat rot_data[9];
	mat3FromIquat(rot_data, m->rot);
	glUniformMatrix3fv(u_main_rot, 1, GL_FALSE, rot_data);

	// Add in scaling / translation...
	range(i, 9) rot_data[i] *= scale;
	float matWorld[16];
	float translate[3];
	range(i, 3) translate[i] = m->oldPos[i] - camPos[i] + interpRatio*(m->pos[i] - m->oldPos[i]);
	matEmbiggen(matWorld, rot_data, translate[0], translate[1], translate[2]);

	// And finally apply the transform we computed during `setupFrame`
	float matScreen[16];
	mat4Multf(matScreen, matWorldToScreen, matWorld);
	// Then we have to translate it and rotate for the camera
	glUniformMatrix4fv(u_main_modelview, 1, GL_FALSE, matScreen);

	// Set texture and tex-related uniforms
	glBindTexture(GL_TEXTURE_2D, textures[tex]); // Is this okay to be doing so often? Hope so!
	glUniform1f(u_main_texscale, scale/1000);
	glUniform2f(u_main_texoffset, 0, 0);

	int32_t vertexIndex;
	if (mesh == 0) {
		vertexIndex = vtxIdx_cubeOneFace;
	} else if (mesh == 1) {
		vertexIndex = vtxIdx_slabOneFace;
	} else if (mesh == 2) {
		vertexIndex = vtxIdx_poleOneFace;
	} else {
		vertexIndex = vtxIdx_cubeSixFace;
	}

	// For now, all our meshes have the same number of vertices:
	// 6 faces * 2 tris/face * 3 vtx/tri = 36 vertexes to draw
	glDrawArrays(GL_TRIANGLES, vertexIndex, 36);
}

void setup2d() {
	glUseProgram(sprite_prog);
	glBindVertexArray(vaos[1]);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
}

// Can make variants of this -
// what dimension(s) to specify,
// whether to assume a square grid,
// where to put the origin, etc.
// This assumes a square grid with the origin at the center, and accepts the Y size.
void centeredGrid2d(float boundsY) {
	// TODO I'm flipping this because I think I have a "front face" issue...
	displayAreaBounds[1] = -boundsY;
	displayAreaBounds[0] = boundsY*displayWidth/displayHeight;
	glUniform2f(u_spr_scale, 1.0/displayAreaBounds[0], 1.0/displayAreaBounds[1]);
}

// `texW` and `texH` are the full width/height of the texture, in pixels.
void selectTex2d(int tex, int texW, int texH) {
#ifndef NODEBUG
	// TODO this needs to be enforced when we deserialize stuff
	//      (really it needs to be enforced as a custom type but whatever)
	if (tex < 0 || tex >= NUM_TEXS) {
		printf("Invalid tex: %d\n", tex);
		exit(1);
	}
#endif
	glBindTexture(GL_TEXTURE_2D, textures[tex]);
	glUniform2f(u_spr_tex_scale, 1.0/texW, 1.0/texH);
}

void sprite2d(int spr_off_x, int spr_off_y, int spr_w, int spr_h, float x, float y) {
	glUniform2f(u_spr_tex_offset, spr_off_x, spr_off_y);
	glUniform2f(u_spr_size, spr_w, spr_h);
	glUniform2f(u_spr_screen_offset, x, y);

	glDrawArrays(GL_TRIANGLES, 0, 6); // 6 vtx = 2 tri = 1 square
}

void setup2dText() {
	// This part I *could* convert to a call to `centeredGrid2d`,
	// but this code pre-dates that function. It wouldn't be a
	// drop-in replacement, so I'm leaving it be for now.
	float textSize = 4;
	displayAreaBounds[0] = displayWidth/2.0/textSize;
	displayAreaBounds[1] = displayHeight/2.0/textSize;
	// We put (0,0) in the upper-left of the letter, at least for now.
	// That means we need Y to increase downwards on the screen, so we flip that axis here.
	glUniform2f(u_spr_scale, 1.0/displayAreaBounds[0], -1.0/displayAreaBounds[1]);

	// Our texture is 64x64, and I belive tex coords go 0-1
	glUniform2f(u_spr_tex_scale, 1.0/64, 1.0/64);
	glBindTexture(GL_TEXTURE_2D, textures[TEX_FONT]);
}

void drawText(char const* str, int x, int y) {
	// Text is 4x6, stride is 5x7, and the copied piece is 5x8 (including blank pixels above, below, and to the right).

	float cursorX = -displayAreaBounds[0]+x;
	float cursorY = -displayAreaBounds[1]+y;
	// Each letter has a blank column copied to the right, but the first blank column (to the left) is done manually.
	glUniform2f(u_spr_size, 1, 8);
	glUniform2f(u_spr_screen_offset, cursorX, cursorY);
	cursorX++;
	glUniform2f(u_spr_tex_offset, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6); // 6 vtx = 2 tri = 1 square

	glUniform2f(u_spr_size, 5, 8);

	for (int idx = 0;; idx++){
		int letter = str[idx];
		if (!letter) return;

		glUniform2f(u_spr_screen_offset, cursorX, cursorY);
		cursorX += 5;

		letter -= 32;
		if (letter < 0 || letter >= 96) continue;

		int texRow = letter/12;
		int texCol = letter%12;
		glUniform2f(u_spr_tex_offset, 1+texCol*5, texRow*7);
		glDrawArrays(GL_TRIANGLES, 0, 6); // 6 vtx = 2 tri = 1 square
	}
}

/*
void drawHudRect(double x, double y, double w, double h, float *color) {
	// I've added one more font character (at postition 94, corresponding to ASCII 127 / DEL)
	// which is just 2 tris as a square. This is because I am lazy, and want to draw rectangles easily.

	glUniform2f(u_flat_offset_id, x, y);
	glUniform2f(u_flat_scale_id, w, h);
	glUniform3fv(u_flat_color_id, 1, color);

	glDrawElements(GL_TRIANGLES, myfont.letterLen[94], GL_UNSIGNED_SHORT, (void*)(sizeof(short)*myfont.letterStart[94]));
}
*/

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

// This handles the single-textured "cube" and "slab" meshes,
// and will probably handle a "rod"/"pillar" at some point as well
static void populateCubeVertexData(list<GLfloat> *data, float x, float y, float z) {
	GLfloat L = -x, R = x;
	GLfloat F = -y, B = y;
	GLfloat D = -z, U = z;
	// up
	vtx(L, B, U, 0, 0, U, 0, 0);
	vtx(L, F, U, 0, 0, U, 0, y);
	vtx(R, B, U, 0, 0, U, x, 0);
	vtx(R, F, U, 0, 0, U, x, y);
	vtx(R, B, U, 0, 0, U, x, 0);
	vtx(L, F, U, 0, 0, U, 0, y);
	// front
	vtx(L, F, U, 0, F, 0, 0, 0);
	vtx(L, F, D, 0, F, 0, 0, z);
	vtx(R, F, U, 0, F, 0, x, 0);
	vtx(R, F, D, 0, F, 0, x, z);
	vtx(R, F, U, 0, F, 0, x, 0);
	vtx(L, F, D, 0, F, 0, 0, z);
	// right
	vtx(R, F, U, R, 0, 0, 0, 0);
	vtx(R, F, D, R, 0, 0, 0, z);
	vtx(R, B, U, R, 0, 0, y, 0);
	vtx(R, B, D, R, 0, 0, y, z);
	vtx(R, B, U, R, 0, 0, y, 0);
	vtx(R, F, D, R, 0, 0, 0, z);
	// left
	vtx(L, B, U, L, 0, 0, 0, 0);
	vtx(L, B, D, L, 0, 0, 0, z);
	vtx(L, F, U, L, 0, 0, y, 0);
	vtx(L, F, D, L, 0, 0, y, z);
	vtx(L, F, U, L, 0, 0, y, 0);
	vtx(L, B, D, L, 0, 0, 0, z);
	// back
	vtx(R, B, U, 0, B, 0, 0, 0);
	vtx(R, B, D, 0, B, 0, 0, z);
	vtx(L, B, U, 0, B, 0, x, 0);
	vtx(L, B, D, 0, B, 0, x, z);
	vtx(L, B, U, 0, B, 0, x, 0);
	vtx(R, B, D, 0, B, 0, 0, z);
	// down
	vtx(L, F, D, 0, 0, D, 0, 0);
	vtx(L, B, D, 0, 0, D, 0, y);
	vtx(R, F, D, 0, 0, D, x, 0);
	vtx(R, B, D, 0, 0, D, x, y);
	vtx(R, F, D, 0, 0, D, x, 0);
	vtx(L, B, D, 0, 0, D, 0, y);
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
	// Previously these ranged from -1 to 1.
	// For text, easier to work with 0 to 1.
	// Could always have 2 versions in the array back-to-back if we want both!
        GLfloat L =  0;
        GLfloat R =  1;
        GLfloat U =  0;
        GLfloat D =  1;
        int counter = 0;
#define vtx(x, y) \
        data[counter++] = x; \
        data[counter++] = y; \
// END vtx
	vtx(L,U);
	vtx(L,D);
	vtx(R,U);
	vtx(R,D);
	vtx(R,U);
	vtx(L,D);
	glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
#undef vtx
}
