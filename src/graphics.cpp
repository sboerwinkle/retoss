#include <math.h>
#include <stdio.h>
#include <string.h>

#include <glad/gl.h>
// Import GLFW after glad
#include <GLFW/glfw3.h>

#include "list.h"
#include "mtx.h"
#include "util.h"

#include "file.h"
#include "matrix.h"
#include "main.h"
#include "png.h"
#include "gamestate.h"
#include "game.h"
#include "game_graphics.h"
#include "main_graphics.h"
#include "collision.h" // For raycasting, for camera position
#include "mypoll.h"
#include "player.h"

#include "graphics.h"
#include "graphics_callbacks.h"

#define JUMP_GUIDE_LEN 100

struct dyntex_texture {
	GLuint tex;
	dyntex_description descr;
	int holders;
};

static list<dyntex_texture> dyntexs;

#define TEX_MOTTLE 0
#define TEX_FONT 1
#define TEX_BLACK 2
#define TEX_TRAIL 6
char const * const texSrcFiles[NUM_TEXS] = {
	NULL,
	"font.png",
	"black.png",
	"guy.3.png",
	"stop.png",
	"wall.png",
	"trail.png",
	"grass.png",
	"metal.png",
	"stone.png",
	"guy.png",
	"guy.2.png",
	"snakes.png",
};
GLuint textures[NUM_TEXS];

static void _sprite2d(float s_x, float s_y, float s_w, float s_h, float x, float y);
static void setupTextDrawingInner();
static void populatePaneVertexData(list<GLfloat> *data);
static void populateCubeVertexData(list<GLfloat> *data);
static void populateCubeVertexData2(list<GLfloat> *data);
static void populateJumpGuideData(list<GLfloat> *data);
static void populateSpriteVertexData();

float gfx_camDist;
float gfx_interpRatio = 0;
// Corresponds to 15 degrees, maybe make this configurable later.
float gfx_camHoverCos = 0.9659;
float gfx_camHoverSin = 0.2588;
float gfx_lookDir[3];

int displayWidth = 0;
int displayHeight = 0;
float scaleX, scaleY;
float displayAreaBounds[2];
offset gfx_camPos1;
offset gfx_camPos2;
list<mover*> gfx_nearMovers;

static char startupFailed = 0;
static GLuint main_prog;
static GLuint sprite_prog;
// static GLuint flat_prog; // Will need this later, but don't feel like reworking shader rn

static GLuint buffer_id, spr_buffer_id;
static GLuint fb_id;
static GLuint vaos[2];

static GLint u_main_modelview;
static GLint u_main_rot;
static GLint u_main_skin_size;
static GLint u_main_tex_offset;
static GLint u_main_mottle_scale;
static GLint u_main_one;
static GLint u_main_tint;
static GLint u_main_transparency;
static GLint u_main_fog;
static GLint u_main_fog_mult;

static GLint u_spr_size;
static GLint u_spr_scale;
static GLint u_spr_screen_offset;
static GLint u_spr_tex_scale;
static GLint u_spr_tex_offset;
static GLint u_spr_color_mult;
static GLint u_spr_color_add;

// Where vertexes for a given shape start in our big buffer of vertex data.
// There's probably a more standard way of doing this!
static int vtxIdx_cubeOneFace = -1;
static int vtxIdx_cubeSixFace = -1;
static int vtxIdx_pane = -1;
static int vtxIdx_jumpGuide = -1;

static float matWorldToScreen[16];
static float camHoverDir[3];
static float ifovX, ifovY;

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

static void setTexFilterParams() {
	// When evaluating changes to the below filtering settings, you should at least evaluate the two scenarios:
	//  1. You are on a large, flat plane moving around.
	//    How crisp is it? How visible is the mipmap and sampling seam? How much does the sampling seam move as you move the camera only?
	//  2. There is a large object moving in the distance.
	//    How crisp is it? How does it look as it approaches the threshold of minification->magnification? Is there significant aliasing shimmer?
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	// Kick it back one mipmap level.
	// I'm doing this to deal with textures viewed at an angle.
	// Could fix this better if I required GL 4.6 and used anisotropic filtering,
	// or could probably do almost exactly as good if I want to get into querying for
	// optional GL extensions.
	// This is probably good enough for now, might just result in some aliasing.
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -1);
}

static void drawDyntex(GLuint tex, dyntex_description *_descr) {
	dyntex_description &descr = *_descr;
	int width = 128, height = 128;

	glBindTexture(GL_TEXTURE_2D, tex);
	// One tutorial used GL_NEAREST for both filtering modes while drawing to the tex,
	// but IDK how much it matters. Going to just assign the final params now and
	// assume it's good enough for this phase as well.
	setTexFilterParams();
	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_RGBA8,
		width, height,
		0,
		GL_RGBA, GL_UNSIGNED_BYTE, NULL
	);

	glBindFramebuffer(GL_FRAMEBUFFER, fb_id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glViewport(0, 0, width, height);

	//printf("Got %d, want %d\n", glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

	glDepthMask(0); // Not really sure if I need this one
	setup2dDrawing();

	// Sadly we don't have access to nice stuff like `glCopyImageSubData`,
	// so I'm just going to draw the texture onto the texture.
	glUniform2f(u_spr_scale, 1, 1);
	selectTex2d(descr.baseTex, 2, 2);
	_sprite2d(0, 2, 2, 2, -1, 1);

	// This affects the font size, since this is the only way
	// it knows the "resolution" of the target texture.
	displayAreaBounds[0] = 64;
	displayAreaBounds[1] = 64;
	setupTextDrawingInner();
	//spriteColorMult(1.00, 0.50, 0.50, 1); // Could set a color here

	/*
	From populateCubeVertexData2, back face (which is front on player model?) has:
	X from 64-128
	Y from 0-42
	*/

	char monogram[2] = {descr.str[0], '\0'};
	// Ugh y is still inverted here
	drawText(monogram, 118, 128-18);
	// Conveniently, the back of the player is centered on the sprite,
	// so centering on the back is easy.
	drawTextCentered(descr.str, 64);

	glBindTexture(GL_TEXTURE_2D, tex);
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindFramebuffer(GL_FRAMEBUFFER, 0); // Bind back to screen's framebuffer
	// Probably need to restore this
	glViewport(0, 0, displayWidth, displayHeight);
}

static void cleanupAll(list<ggc_msg> *l) {
	rangeconst(i, l->num) {
		ggcDestroy(&(*l)[i]);
	}
	l->num = 0;
}

void newDyntexHolder(dyntex_holder *h) {
	// Simple linear search through our existing textures
	// to see if any match the requested description
	rangeconst(i, dyntexs.num) {
		dyntex_texture &t = dyntexs[i];
		if (t.descr.baseTex != h->descr.baseTex) continue;
		if (strcmp(t.descr.str, h->descr.str)) continue;
		// We have a match
		h->tex = t.tex;
		t.holders++;
		return;
	}
	// No match, have to allocate (and draw) a new texture
	dyntex_texture &t = dyntexs.add();
	// I feel like I should check that this succeeded,
	// but I'm not clear on when it would even fail!
	// Neither `glGenTextures` nor `glBindTexture`
	// describe any such errors.
	glGenTextures(1, &t.tex);
	t.holders = 1;
	// Copy description wholesale
	t.descr = h->descr;

	h->tex = t.tex;
	drawDyntex(t.tex, &t.descr);
}

void oldDyntexHolder(dyntex_holder *h) {
	rangeconst(i, dyntexs.num) {
		dyntex_texture &t = dyntexs[i];
		if (h->tex != t.tex) continue;
		t.holders--;
		if (!t.holders) {
			glDeleteTextures(1, &t.tex);
			dyntexs.quickRmAt(i);
		}
		return;
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
	if (!texSrcFiles[i]) return;
	char *imageData;
	int width, height;
	char path[200];
	snprintf(path, 200, "assets/%s", texSrcFiles[i]);
	png_read(&imageData, &width, &height, path);
	if (!imageData) {
		printf("Not loading texture %d\n", i);
	} else {
		glBindTexture(GL_TEXTURE_2D, textures[i]);
		// (TODO wtf did I ever mean here)
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
	dyntexs.init();
	gfx_nearMovers.init();

	GLuint vertexShader = mkShader(GL_VERTEX_SHADER, "assets/shaders/solid.vert");
	GLuint spriteShader = mkShader(GL_VERTEX_SHADER, "assets/shaders/sprite.vert");
	GLuint fragShader = mkShader(GL_FRAGMENT_SHADER, "assets/shaders/color.frag");
	GLuint fragShaderSprite = mkShader(GL_FRAGMENT_SHADER, "assets/shaders/texOnly.frag");

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

	GLint a_pos_id = attrib(main_prog, "a_pos");
	GLint a_norm_id = attrib(main_prog, "a_norm");
	GLint a_tex_id_0 = attrib(main_prog, "a_tex");
	// sprite_prog attribs
	GLint a_spr_loc = attrib(sprite_prog, "a_loc");

	// Uniforms
	u_main_modelview    = glGetUniformLocation(main_prog, "u_modelview");
	u_main_rot          = glGetUniformLocation(main_prog, "u_rot");
	u_main_skin_size    = glGetUniformLocation(main_prog, "u_skin_size");
	u_main_tex_offset   = glGetUniformLocation(main_prog, "u_tex_offset");
	u_main_mottle_scale = glGetUniformLocation(main_prog, "u_mottle_scale");
	u_main_one          = glGetUniformLocation(main_prog, "u_one");
	u_main_tint         = glGetUniformLocation(main_prog, "u_tint");
	u_main_transparency = glGetUniformLocation(main_prog, "u_transparency");
	u_main_fog          = glGetUniformLocation(main_prog, "u_fog");
	u_main_fog_mult     = glGetUniformLocation(main_prog, "u_fog_mult");
	// sprite_prog uniforms
	u_spr_size          = glGetUniformLocation(sprite_prog, "u_size");
	u_spr_scale         = glGetUniformLocation(sprite_prog, "u_scale");
	u_spr_screen_offset = glGetUniformLocation(sprite_prog, "u_offset");
	u_spr_tex_scale     = glGetUniformLocation(sprite_prog, "u_tex_scale");
	u_spr_tex_offset    = glGetUniformLocation(sprite_prog, "u_tex_offset");
	u_spr_color_mult    = glGetUniformLocation(sprite_prog, "u_c_mult");
	u_spr_color_add     = glGetUniformLocation(sprite_prog, "u_c_add");

	// Previously I checked that some uniforms are in the same spots across programs here,
	// and log + set startupFailed=1 if not.

	printGLProgErrors(main_prog, "main");
	printGLProgErrors(sprite_prog, "sprite");
	//printGLProgErrors(flat_prog, "flat");

	if (startupFailed) {
		puts("Aborting due to one or more GL startup issues");
		exit(1);
	}

	glEnable(GL_CULL_FACE);
	glGenVertexArrays(2, vaos);
	glGenTextures(NUM_TEXS, textures);

#define STRIDE 12
	list<GLfloat> vtxData;
	vtxData.init();
	vtxIdx_cubeOneFace = 0;
	populateCubeVertexData(&vtxData);
	vtxIdx_cubeSixFace = vtxData.num / STRIDE;
	populateCubeVertexData2(&vtxData);
	vtxIdx_pane = vtxData.num / STRIDE;
	populatePaneVertexData(&vtxData);
	vtxIdx_jumpGuide = vtxData.num / STRIDE;
	populateJumpGuideData(&vtxData);

	// vaos[0]
	glBindVertexArray(vaos[0]);
	glEnableVertexAttribArray(a_pos_id);
	glEnableVertexAttribArray(a_norm_id);
	glEnableVertexAttribArray(a_tex_id_0);
	glEnableVertexAttribArray(a_tex_id_0+1);
	glEnableVertexAttribArray(a_tex_id_0+2);

	// I swear I had this written down somewhere... Anyway,
	// I'm pretty sure the bound buffer isn't part of VAO state, but it is recorded when
	// vertex attributes are configured (below). So vertex data doesn't care what's bound at draw time,
	// it already knows which buffer it's reading from.
	glGenBuffers(1, &buffer_id);
	glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*vtxData.num, vtxData.items, GL_STATIC_DRAW);
	vtxData.destroy();

	// Position data is first
	glVertexAttribPointer(a_pos_id, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * STRIDE, (void*) 0);
	// Followed by normal data
	glVertexAttribPointer(a_norm_id, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * STRIDE, (void*) (sizeof(GLfloat) * 3));
	// Followed by tex matrix data
	glVertexAttribPointer(a_tex_id_0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * STRIDE, (void*) (sizeof(GLfloat) * 6));
	glVertexAttribPointer(a_tex_id_0+1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * STRIDE, (void*) (sizeof(GLfloat) * 8));
	glVertexAttribPointer(a_tex_id_0+2, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * STRIDE, (void*) (sizeof(GLfloat) * 10));
	cerr("End of vao 0 prep");
#undef STRIDE

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

	range(i, NUM_TEXS) {
		glBindTexture(GL_TEXTURE_2D, textures[i]);
		setTexFilterParams();
	}

	loadAllTextures();

	glClearColor(0.5, 0.5, 0.5, 1);

	glEnable(GL_BLEND);
	// It's possible to set the blend behavior of the alpha channel distinctly from that of the RGB channels.
	// This would matter if we used the DEST_ALPHA at all, but we don't, so we don't care what happens to it.
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Framebuffer setup so we can draw to a texture!
	glGenFramebuffers(1, &fb_id);

	cerr("End of graphics setup");
}

void gfx_destroy() {
	gfx_nearMovers.destroy();

	// At this poing the gfx thread is already killed, so
	// probably no point in trying to tell GL we're done
	// with these textures. We don't return our "static"
	// textures anyway!
	dyntexs.destroy();

	// All threads are stopped, and all gamestates have been destroyed.
	// Clean up any messages, whichever list they're in.
	cleanupAll(msgs_game);
	cleanupAll(msgs_gfx);
	cleanupAll(msgs_exchange);
	// `main.cpp` cleans up the lists themselves.
}

static void checkReload() {
	// I go into more detail about the use of std::memory_order in watch.cpp,
	// but the short version is that we're getting some guarantees similar to
	// a very simple semaphore.
	if (!texReloadFlag.load(std::memory_order::acquire)) return;

	// Skip mottle tex, it isn't read from file
	for (int i = 1; i < NUM_TEXS; i++) {
		if (texSrcFiles[i] && !strcmp(texReloadPath, texSrcFiles[i])) {
			loadTexture(i);
			goto success;
		}
	}
	// Never thought I'd miss python's for-else...
	printf("We don't care about '%s'\n", texReloadPath);
	success:;

	texReloadFlag.store(0, std::memory_order::release);
}

static float calcCamDist(float *matWorldToCam, offset const p1, offset const p2, gamestate *gs, float fovInverse, int64_t hovDist) {
	unitvec dir;
	range(i, 3) dir[i] = camHoverDir[i]*FIXP;
	float y = GFX_Z_NEAR;
	float z = GFX_Z_NEAR/fovInverse;
	float x = z*displayWidth/displayHeight;
	// Pad things out a little to account for rounding errors.
	// A couple units seems to be plenty.
	x += 2;
	z += 2;

	offset v;
	range(i, 3) v[i] = p2[i] - p1[i];
	gfx_nearMovers.num = 0;
	// Our search radius has to account for how far away the camera sits, plus the distance to the corners
	// of the screen (since that's what we're really checking against).
	int64_t padding = sqrtf(x*x + y*y + z*z) + 1;
	int64_t radius = std::max((int64_t)PLAYER_INTERACT_RANGE, hovDist + padding);
	velbox_query_ts(gs->vb_root, p1, v, radius, &gfx_nearMovers, gs->clock);

	offset corners1[4];
	offset corners2[4];
	// This could be like a few matrix multiplications probably.
	// This is faster and much less legible, clearly a win.
	range(i, 3) {
		float forward = y*matWorldToCam[1+4*i];
		float side = x*matWorldToCam[0+4*i];
		float vert = z*matWorldToCam[2+4*i];
		int64_t a = forward - side - vert;
		int64_t b = forward - side + vert;
		int64_t c = forward + side + vert;
		int64_t d = forward + side - vert;
		corners1[0][i] = p1[i] + a;
		corners1[1][i] = p1[i] + b;
		corners1[2][i] = p1[i] + c;
		corners1[3][i] = p1[i] + d;
		corners2[0][i] = p2[i] + a;
		corners2[1][i] = p2[i] + b;
		corners2[2][i] = p2[i] + c;
		corners2[3][i] = p2[i] + d;
	}
	fraction best = {.numer=hovDist, .denom=FIXP};
	rangeconst(i, gfx_nearMovers.num) {
		mover *m = gfx_nearMovers[i];
		// Treat players and projectiles as transparent for these purposes
		if ((m->type & T_MASK) != 0) continue;
		range(j, 4) {
			raycast_interp(&best, m, corners1[j], corners2[j], dir, gfx_interpRatio);
		}
	}
	return (float)best.numer*FIXP/best.denom;
}

void resetAfterBillboards() {
	glUniform2f(u_main_tex_offset, 0, 0);
}

void setupFrame(player const *p, gamestate *gs, lookConfig *lookCfg) {
	checkReload();
	glUseProgram(main_prog);
	glBindVertexArray(vaos[0]);
	glDepthMask(1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	tint(0, 0, 0, 0);
	resetAfterBillboards();
	glUniform3f(u_main_fog, 0.5, 0.5, 0.5);
	glUniform1f(u_main_fog_mult, -1.0 / gs->fogDist);
	glUniform1f(u_main_one, 1);

	// GL stuff that we change over the course of drawing a frame
	glEnable(GL_DEPTH_TEST);

	float matWorldToCam[16];
	// Grab the camera rotation and reverse it
	quat quatWorldToCam;
	memcpy(quatWorldToCam, quatCamRotation, sizeof(quat));
	quatWorldToCam[1] *= -1;
	quatWorldToCam[2] *= -1;
	quatWorldToCam[3] *= -1;
	mat4FromQuat(matWorldToCam, quatWorldToCam);
	// Could also calculate the `camHoverDir` by applying the quaternion to a vector,
	// but instead I'm doing this (linear combination of rotated axes).
	camHoverDir[0] = -matWorldToCam[1] * lookCfg->hovCos + matWorldToCam[ 2] * lookCfg->hovSin;
	camHoverDir[1] = -matWorldToCam[5] * lookCfg->hovCos + matWorldToCam[ 6] * lookCfg->hovSin;
	camHoverDir[2] = -matWorldToCam[9] * lookCfg->hovCos + matWorldToCam[10] * lookCfg->hovSin;
	gfx_lookDir[0] = matWorldToCam[1];
	gfx_lookDir[1] = matWorldToCam[5];
	gfx_lookDir[2] = matWorldToCam[9];

	memcpy(gfx_camPos1, p->m.oldPos, sizeof(offset));
	memcpy(gfx_camPos2, p->m.pos, sizeof(offset));
	if (lookCfg->hovDist && p->alive) {
		gfx_camDist = calcCamDist(matWorldToCam, p->m.oldPos, p->m.pos, gs, lookCfg->fovInv, lookCfg->hovDist);
		// Previously I'd use `p1` and `p2` directly and just set
		// `gfx_camDist` into `matWorldToCam[13]`. This was neat,
		// but meant our calculations for how to draw trails were
		// all messed up.
		range(i, 3) {
			int64_t shift = camHoverDir[i]*gfx_camDist;
			gfx_camPos1[i] += shift;
			gfx_camPos2[i] += shift;
		}
	} else {
		gfx_camDist = 0;
	}

	// Todo I'm sure I'm wasting some multiplications here.
	//      Maybe a pointless optimization, but for both of the 4x4 matrix multiplications
	//      we do, we know some of the values are constants like 0 or 1 going in.

	// This is the same no matter what (or where) we're rendering, so I can do that here...
	ifovX = lookCfg->fovInv*displayHeight/displayWidth;
	ifovY = lookCfg->fovInv;
	float matPersp[16];
	perspective(
		matPersp,
		ifovX,
		ifovY,
		GFX_Z_NEAR
	);

	mat4Multf(matWorldToScreen, matPersp, matWorldToCam);
}

void tint(float r, float g, float b, float a) {
	GLfloat _r = r*a;
	GLfloat _g = g*a;
	GLfloat _b = b*a;
	GLfloat _a = 1-a;
	glUniform4f(u_main_tint, _r, _g, _b, _a);
}

void drawCube(mover *m, offset const dimens, int tex, int mode, float alpha) {
	if (mode & 32) {
		mode &= ~32;
	} else {
#ifdef DEBUG
		if (tex < 0 || tex >= NUM_TEXS) {
			printf("ERROR: Invalid tex %d\n", tex);
			exit(1);
		}
#endif
		tex = textures[tex];
	}

	// The rotation of the thing itself (used for lighting).
	GLfloat rot_data[9], rot_data2[9];
	mat3FromIquat(rot_data, m->oldRot);
	mat3FromIquat(rot_data2, m->rot);
	range(i, 9) rot_data[i] += gfx_interpRatio*(rot_data2[i] - rot_data[i]);
	glUniformMatrix3fv(u_main_rot, 1, GL_FALSE, rot_data);

	// Add in scaling / translation...
	range(i, 9) rot_data[i] *= dimens[i/3];
	float matWorld[16];
	float translate[3];
	range(i, 3) {
		int64_t x1 = m->oldPos[i] - gfx_camPos1[i];
		int64_t x2 = m->pos[i]    - gfx_camPos2[i];
		translate[i] = x1 + gfx_interpRatio*(x2-x1);
	}
	matEmbiggen(matWorld, rot_data, translate[0], translate[1], translate[2]);

	// And finally apply the transform we computed during `setupFrame`
	float matScreen[16];
	mat4Multf(matScreen, matWorldToScreen, matWorld);
	// Then we have to translate it and rotate for the camera
	glUniformMatrix4fv(u_main_modelview, 1, GL_FALSE, matScreen);

	// Set texture and tex-related uniforms
	glBindTexture(GL_TEXTURE_2D, tex); // Is this okay to be doing so often? Hope so!
	glUniform1f(u_main_transparency, alpha);

	if (!mode) {
		glUniform3f(u_main_skin_size, 1, (float)dimens[1]/dimens[0], (float)dimens[2]/dimens[0]);
		glUniform1f(u_main_mottle_scale, dimens[0]/2'000.0);
	} else {
		glUniform3f(u_main_skin_size, 1, 1, 1);
		// The six-faced cube skin gets 42 pixels (out of 128) per side,
		// so we have to account for that when figureing the mottle scale.
		glUniform1f(u_main_mottle_scale, dimens[0]/2'000.0 * (128.0/42));
	}

	int32_t vertexIndex;
	if (!mode) {
		vertexIndex = vtxIdx_cubeOneFace;
	} else {
		vertexIndex = vtxIdx_cubeSixFace;
	}

	// For now, all our meshes have the same number of vertices:
	// 6 faces * 2 tris/face * 3 vtx/tri = 36 vertexes to draw
	glDrawArrays(GL_TRIANGLES, vertexIndex, 36);
}

void drawBillboard(offset p1, offset p2, int tex, float x, float y, float w, int64_t r) {
	x += 1.0f / (1<<8);
	y += 1.0f / (1<<8);
	w -= 1.0f / (1<<7);
	glBindTexture(GL_TEXTURE_2D, textures[tex]);
	glUniform2f(u_main_tex_offset, x, y);
	glUniform3f(u_main_skin_size, w, w, 0);
	glUniform1f(u_main_mottle_scale, r/2'000.0/w);
	glUniform1f(u_main_transparency, 1.0);
	// We skip `u_main_rot` - it's just for lighting,
	// and the "pane" mesh's normals are all 0 anyway.

	// TODO I'm sure I can do better than a matrix mult where almost everything is 0 lol
	float matWorld[16] = {0};
	matWorld[15] = 1;
	range(i, 3) {
		int64_t x1 = p1[i] - gfx_camPos1[i];
		int64_t x2 = p2[i] - gfx_camPos2[i];
		matWorld[12+i] = x1 + gfx_interpRatio*(x2-x1);
	}
	float matScreen[16];
	mat4Multf(matScreen, matWorldToScreen, matWorld);

	matScreen[0] = r*ifovX;
	matScreen[9] = r*ifovY;
	glUniformMatrix4fv(u_main_modelview, 1, GL_FALSE, matScreen);

	glDrawArrays(GL_TRIANGLES, vtxIdx_pane, 6);
}

void drawJumpGuide(offset p1, double yaw) {
	// Hm. For now I'll leave glDepthMask on, I feel like
	// trails drawing over the guide from behind it might look weird.
	// (I mean, short of depth-sorting these there will be weirdness regardless...)
	// (Unless I do stippling again, but consider: I don't wanna)
	glBindTexture(GL_TEXTURE_2D, textures[TEX_BLACK]);
	glUniform1f(u_main_transparency, 0.7);
	// Over-saturating the green makes the mottling look a little funny,
	// but it does make the guide noticably more visible. Worth it.
	tint(0.7, 1.1, 1.0, 1);
	glUniform3f(u_main_skin_size, 1, 0, 0);
	glUniform1f(u_main_mottle_scale, 0.5);

	float matWorld[16] = {0};
	double c = cos(yaw);
	double s = sin(yaw);
	matWorld[0] = matWorld[5] = c;
	matWorld[1] = s;
	matWorld[4] = -s;
	matWorld[10] = matWorld[15] = 1;
	range(i, 3) matWorld[12+i] = p1[i] - gfx_camPos1[i];

	float matScreen[16];
	mat4Multf(matScreen, matWorldToScreen, matWorld);
	glUniformMatrix4fv(u_main_modelview, 1, GL_FALSE, matScreen);

	// I could maybe do this by drawing both sides of triangles and rendering it in one pass
	// from far to near? But this works too, and I don't have to look up how self-occlusion
	// within one render call is handled.
	glFrontFace(GL_CW);
	glDrawArrays(GL_TRIANGLE_STRIP, vtxIdx_jumpGuide, JUMP_GUIDE_LEN * 2 + 1);
	glFrontFace(GL_CCW);
	glDrawArrays(GL_TRIANGLE_STRIP, vtxIdx_jumpGuide, JUMP_GUIDE_LEN * 2 + 1);

	tint(0, 0, 0, 0);
}

void drawTrail(offset const start, unitvec const dir, int64_t len, float age_interp) {
	glDepthMask(0);
	glBindTexture(GL_TEXTURE_2D, textures[TEX_TRAIL]);
	glUniform3f(u_main_skin_size, 1, 0, 0);
	glUniform1f(u_main_mottle_scale, 0.5);
	glUniform1f(u_main_transparency, 1.0-age_interp);
	// We skip `u_main_rot` - it's just for lighting,
	// and the "pane" mesh's normals are all 0 anyway.
	float matWorld[16];
	float * const sideways = matWorld;
	// Our "trail" mesh doesn't have any nonzero Y components, so we don't bother with `matWorld+4`.
	float * const forward = matWorld+8;
	float * const translate = matWorld+12;
	// Fill in the other spaces in the matrix...
	matWorld[3] = 0;
	matWorld[4] = matWorld[5] = matWorld[6] = matWorld[7] = 0;
	matWorld[11] = 0;
	matWorld[15] = 1;

	// `forward` and `translate` we can produce without much fuss.
	range(i, 3) forward[i] = (float)dir[i]*len/(FIXP*2);
	range(i, 3) {
		// These `cam` values don't depend on the trail, so I guess I could pre-calculate them
		// if that actually represented any useful savings. Things that aren't trails don't use
		// these values, since they don't play well with floats at high speeds. Trails don't
		// move though, so they won't play well at high speeds anyway.
		int64_t cam = gfx_camPos1[i] + gfx_interpRatio*(gfx_camPos2[i]-gfx_camPos1[i]);
		translate[i] = start[i]-cam+forward[i];
	}
	// `sideways` is the trickiest to produce,
	// since it depends on the angle you're looking at the trail from.
	float fdir[3];
	range(i, 3) fdir[i] = dir[i]*(0.5/FIXP); // scale factor here relates to distance from trail
	cross(sideways, translate, fdir);
	float magnitude = sqrt(sideways[0]*sideways[0] + sideways[1]*sideways[1] + sideways[2]*sideways[2]);
	float trail_width = -10.0f/(age_interp+0.1f)+110.0f;
	// If camera lines up too closely w/ trail,
	// it will shrink to nothing,
	// instead of spinning around dramatically as the camera makes its near approach
	if (magnitude > trail_width) {
		float x = trail_width/magnitude;
		range(i, 3) sideways[i] *= x;
	}
	// And finally apply the transform we computed during `setupFrame`
	float matScreen[16];
	mat4Multf(matScreen, matWorldToScreen, matWorld);
	glUniformMatrix4fv(u_main_modelview, 1, GL_FALSE, matScreen);

	glDrawArrays(GL_TRIANGLES, vtxIdx_pane, 6);
	glDepthMask(1);
}

void setup2dDrawing() {
	glUseProgram(sprite_prog);
	glBindVertexArray(vaos[1]);
	spriteColorMult(1, 1, 1, 1);
	spriteColorAdd(0, 0, 0, 0);

	glDisable(GL_DEPTH_TEST);
}

void spriteColorMult(float r, float g, float b, float a) {
	glUniform4f(u_spr_color_mult, r, g, b, a);
}

void spriteColorAdd(float r, float g, float b, float a) {
	glUniform4f(u_spr_color_add, r, g, b, a);
}

// Can make variants of this -
// what dimension(s) to specify,
// whether to assume a square grid,
// where to put the origin, etc.
// This assumes a square grid with the origin at the center, and accepts the Y size.
void centeredGrid2d(float boundsY) {
	displayAreaBounds[1] = boundsY;
	displayAreaBounds[0] = boundsY*displayWidth/displayHeight;
	glUniform2f(u_spr_scale, 1.0/displayAreaBounds[0], 1.0/displayAreaBounds[1]);
}

// `texW` and `texH` are the full width/height of the texture, in pixels.
void selectTex2d(int tex, int texW, int texH) {
#ifdef DEBUG
	if (tex < 0 || tex >= NUM_TEXS) {
		printf("ERROR: Invalid tex: %d\n", tex);
		exit(1);
	}
#endif
	glBindTexture(GL_TEXTURE_2D, textures[tex]);
	glUniform2f(u_spr_tex_scale, 1.0/texW, 1.0/texH);
}

// Inner version that doesn't mess with bounds
static void _sprite2d(float s_x, float s_y, float s_w, float s_h, float x, float y) {
	glUniform2f(u_spr_tex_offset, s_x, s_y);
	glUniform2f(u_spr_size, s_w, s_h);
	glUniform2f(u_spr_screen_offset, x, y);

	glDrawArrays(GL_TRIANGLES, 0, 6); // 6 vtx = 2 tri = 1 square
}

void sprite2d(int spr_off_x, int spr_off_y, int spr_w, int spr_h, float x, float y) {
	// Our convention is a little weird for how we do w/h here,
	// and as a result we actually *subtract* our small delta from Y
	float s_x = spr_off_x + (1.0f / (1<<8));
	float s_y = spr_off_y - (1.0f / (1<<8));
	float s_w = spr_w - (1.0f / (1<<7));
	float s_h = spr_h - (1.0f / (1<<7));
	// I had some coordinate systems flipped, and had to get that straightened out.
	// Really I should go through and flip all the callers of this method
	// so everything finally agrees, but for now I'm just changing this to `-y`.
	_sprite2d(s_x, s_y, s_w, s_h, x, -y);
}

static void setupTextDrawingInner() {
	glUniform2f(u_spr_scale, 1.0/displayAreaBounds[0], 1.0/displayAreaBounds[1]);

	// Our texture is 64x64, and tex coords go 0-1
	glUniform2f(u_spr_tex_scale, 1.0/64, 1.0/64);
	glBindTexture(GL_TEXTURE_2D, textures[TEX_FONT]);

	defaultTextColor();
}

void setup2dTextDrawing() {
	// Could convert this part to `centeredGrid2d`,
	// but I don't want to.
	float textSize = 4;
	displayAreaBounds[0] = displayWidth/2.0/textSize;
	displayAreaBounds[1] = displayHeight/2.0/textSize;

	setupTextDrawingInner();
}

void defaultTextColor() {
	spriteColorMult(0.75, 0.75, 0.75, 1);
}

void drawTextCentered(char const *str, int y) {
	drawText(str, displayAreaBounds[0] - (1 + strlen(str)*5)/2, y);
}

void drawTextRight(char const *str, float x, int y) {
	drawText(str, x - (1 + strlen(str)*5), y);
}

void drawText(char const* str, float x, int y) {
	// Text is 4x6, stride is 5x7, and the copied piece is 5x8 (including blank pixels above, below, and to the right).

	float cursorX = -displayAreaBounds[0]+x;
	// Need to go through and flip callers of this method at some point
	float cursorY =  displayAreaBounds[1]-y;
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
		glUniform2f(u_spr_tex_offset, 1+texCol*5, 64-texRow*7);
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

#define vtx(x, y, z, nx, ny, nz, s0, t0, s1, t1, s2, t2) \
        data->add(x); \
        data->add(y); \
        data->add(z); \
        data->add(nx); \
        data->add(ny); \
        data->add(nz); \
        data->add(s0); \
        data->add(t0); \
        data->add(s1); \
        data->add(t1); \
        data->add(s2); \
        data->add(t2); \
// END vtx
#define vtx2(x, y, z, nx, ny, nz, s, t) \
	vtx(x, y, z, nx, ny, nz, s==1, t==1, s==2, t==2, s==3, t==3)

static void populatePaneVertexData(list<GLfloat> *data) {
	// Hm. This may change later, but for now I'm not going to put normal data
	// on these vertexes. That's just for lighting, and currently we're only
	// using this shape for bullet trails (which shouldn't be lit)
	GLfloat L = -1, R = 1;
	GLfloat D = -1, U = 1;
	// We don't support non-uniformly scaled textures on panes,
	// can mess with the a_tex values here if we want that later.
	vtx2(L, 0, U, 0, 0, 0, 0, 2);
	vtx2(L, 0, D, 0, 0, 0, 0, 0);
	vtx2(R, 0, U, 0, 0, 0, 1, 2);
	vtx2(R, 0, D, 0, 0, 0, 1, 0);
	vtx2(R, 0, U, 0, 0, 0, 1, 2);
	vtx2(L, 0, D, 0, 0, 0, 0, 0);
}

// This handles the single-textured "cube" and "slab" meshes,
// and will probably handle a "rod"/"pillar" at some point as well
static void populateCubeVertexData(list<GLfloat> *data) {
	GLfloat L = -1, R = 1;
	GLfloat F = -1, B = 1;
	GLfloat D = -1, U = 1;
	// up
	vtx2(L, B, U, 0, 0, 1, 0, 2);
	vtx2(L, F, U, 0, 0, 1, 0, 0);
	vtx2(R, B, U, 0, 0, 1, 1, 2);
	vtx2(R, F, U, 0, 0, 1, 1, 0);
	vtx2(R, B, U, 0, 0, 1, 1, 2);
	vtx2(L, F, U, 0, 0, 1, 0, 0);
	// front
	vtx2(L, F, U, 0, -1, 0, 0, 3);
	vtx2(L, F, D, 0, -1, 0, 0, 0);
	vtx2(R, F, U, 0, -1, 0, 1, 3);
	vtx2(R, F, D, 0, -1, 0, 1, 0);
	vtx2(R, F, U, 0, -1, 0, 1, 3);
	vtx2(L, F, D, 0, -1, 0, 0, 0);
	// right
	vtx2(R, F, U, 1, 0, 0, 0, 3);
	vtx2(R, F, D, 1, 0, 0, 0, 0);
	vtx2(R, B, U, 1, 0, 0, 2, 3);
	vtx2(R, B, D, 1, 0, 0, 2, 0);
	vtx2(R, B, U, 1, 0, 0, 2, 3);
	vtx2(R, F, D, 1, 0, 0, 0, 0);
	// left
	vtx2(L, B, U, -1, 0, 0, 0, 3);
	vtx2(L, B, D, -1, 0, 0, 0, 0);
	vtx2(L, F, U, -1, 0, 0, 2, 3);
	vtx2(L, F, D, -1, 0, 0, 2, 0);
	vtx2(L, F, U, -1, 0, 0, 2, 3);
	vtx2(L, B, D, -1, 0, 0, 0, 0);
	// back
	vtx2(R, B, U, 0, 1, 0, 0, 3);
	vtx2(R, B, D, 0, 1, 0, 0, 0);
	vtx2(L, B, U, 0, 1, 0, 1, 3);
	vtx2(L, B, D, 0, 1, 0, 1, 0);
	vtx2(L, B, U, 0, 1, 0, 1, 3);
	vtx2(R, B, D, 0, 1, 0, 0, 0);
	// down
	vtx2(L, F, D, 0, 0, -1, 0, 2);
	vtx2(L, B, D, 0, 0, -1, 0, 0);
	vtx2(R, F, D, 0, 0, -1, 1, 2);
	vtx2(R, B, D, 0, 0, -1, 1, 0);
	vtx2(R, F, D, 0, 0, -1, 1, 2);
	vtx2(L, B, D, 0, 0, -1, 0, 0);
}

static void populateCubeVertexData2(list<GLfloat> *data) {
	GLfloat L = -1, R = 1;
	GLfloat F = -1, B = 1;
	GLfloat D = -1, U = 1;

	GLfloat p1 =   1.0/128;
	GLfloat p2 =  43.0/128;
	GLfloat p3 =  85.0/128;
	GLfloat p4 = 127.0/128;

	GLfloat x1 =  86.0/128;
	GLfloat x2 = 128.0/128;
	GLfloat y1 =   0.0/128;
	GLfloat y2 =  42.0/128;

	// UGH obviously you can't scale something that's a net independently along each axis.
	// We'll just lie for now, this is a headache.

	// up
	vtx(L, B, U, 0, 0, U, p2, p4, 0,0,0,0);
	vtx(L, F, U, 0, 0, U, p2, p3, 0,0,0,0);
	vtx(R, B, U, 0, 0, U, p3, p4, 0,0,0,0);
	vtx(R, F, U, 0, 0, U, p3, p3, 0,0,0,0);
	vtx(R, B, U, 0, 0, U, p3, p4, 0,0,0,0);
	vtx(L, F, U, 0, 0, U, p2, p3, 0,0,0,0);
	// front
	vtx(L, F, U, 0, F, 0, p2, p3, 0,0,0,0);
	vtx(L, F, D, 0, F, 0, p2, p2, 0,0,0,0);
	vtx(R, F, U, 0, F, 0, p3, p3, 0,0,0,0);
	vtx(R, F, D, 0, F, 0, p3, p2, 0,0,0,0);
	vtx(R, F, U, 0, F, 0, p3, p3, 0,0,0,0);
	vtx(L, F, D, 0, F, 0, p2, p2, 0,0,0,0);
	// right
	vtx(R, F, U, R, 0, 0, p3, p3, 0,0,0,0);
	vtx(R, F, D, R, 0, 0, p3, p2, 0,0,0,0);
	vtx(R, B, U, R, 0, 0, p4, p3, 0,0,0,0);
	vtx(R, B, D, R, 0, 0, p4, p2, 0,0,0,0);
	vtx(R, B, U, R, 0, 0, p4, p3, 0,0,0,0);
	vtx(R, F, D, R, 0, 0, p3, p2, 0,0,0,0);
	// left
	vtx(L, B, U, L, 0, 0, p1, p3, 0,0,0,0);
	vtx(L, B, D, L, 0, 0, p1, p2, 0,0,0,0);
	vtx(L, F, U, L, 0, 0, p2, p3, 0,0,0,0);
	vtx(L, F, D, L, 0, 0, p2, p2, 0,0,0,0);
	vtx(L, F, U, L, 0, 0, p2, p3, 0,0,0,0);
	vtx(L, B, D, L, 0, 0, p1, p2, 0,0,0,0);
	// back
	vtx(R, B, U, 0, B, 0, x1, y2, 0,0,0,0);
	vtx(R, B, D, 0, B, 0, x1, y1, 0,0,0,0);
	vtx(L, B, U, 0, B, 0, x2, y2, 0,0,0,0);
	vtx(L, B, D, 0, B, 0, x2, y1, 0,0,0,0);
	vtx(L, B, U, 0, B, 0, x2, y2, 0,0,0,0);
	vtx(R, B, D, 0, B, 0, x1, y1, 0,0,0,0);
	// down
	vtx(L, F, D, 0, 0, D, p2, p2, 0,0,0,0);
	vtx(L, B, D, 0, 0, D, p2, p1, 0,0,0,0);
	vtx(R, F, D, 0, 0, D, p3, p2, 0,0,0,0);
	vtx(R, B, D, 0, 0, D, p3, p1, 0,0,0,0);
	vtx(R, F, D, 0, 0, D, p3, p2, 0,0,0,0);
	vtx(L, B, D, 0, 0, D, p2, p1, 0,0,0,0);
}

static void populateJumpGuideData(list<GLfloat> *data) {
	float sqrt2 = sqrtf(2.0);
	float y = PLAYER_SHAPE_RADIUS / sqrt2;
	float z = -PLAYER_SHAPE_RADIUS / sqrt2;
	float vy = PL_JUMP_SPD / sqrt2;
	float vz = PL_JUMP_SPD / sqrt2;
	float mot_x = PLAYER_SHAPE_RADIUS / 2'000.0;
	float mot_y = 0;
	range(i, JUMP_GUIDE_LEN) {
		vtx(-PLAYER_SHAPE_RADIUS, y, z, 0,0,0,     0,mot_y, 0,0,0,0);
		vtx( PLAYER_SHAPE_RADIUS, y, z, 0,0,0, mot_x,mot_y, 0,0,0,0);
		vz -= GRAVITY;
		y += vy;
		z += vz;
		mot_y += sqrt(vy*vy+vz*vz)/2/2'000.0;
	}
	// Point at the end of the path
	vtx(0, y, z, 0,0,0, mot_x/2,mot_y, 0,0,0,0);
}

#undef vtx2
#undef vtx

static void populateSpriteVertexData() {
	GLfloat data[12];
	// Previously these ranged from -1 to 1.
	// For text, easier to work with 0 to 1.
	// Could always have 2 versions in the array back-to-back if we want both!
        GLfloat L =  0;
        GLfloat R =  1;
        GLfloat U =  0;
        GLfloat D = -1;
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
