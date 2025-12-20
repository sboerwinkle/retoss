#include <GLFW/glfw3.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "util.h"
#include "queue.h"
#include "matrix.h"

#include "main.h"
#include "gamestate.h"
#include "graphics.h"
#include "watch_flags.h"
#include "dl.h"

#include "game.h"
#include "game_callbacks.h"

struct timing {
	long minNanos, maxNanos;
	long nextMin, nextMax;
	int counter;
};
static timing logicTiming = {0}, renderTiming = {0};
static void updateTiming(timing *t, long nanos);

static int mouseX = 0, mouseY = 0;
static char mouseDown = 0;
static char debugPrint;
static char renderStats = 0;
quat tmpGameRotation = {1,0,0,0};
quat quatCamRotation = {1,0,0,0};

struct {
	char u, d, l, r;
} activeInputs = {}, sharedInputs = {};

void game_init() {
	initGraphics();
	velbox_init();
	gamestate_init();
	dl_init();
}

// TODO gamestate init logic should be the responsibility of gamestate.cpp.
//      `game_init2` can be responsible for level gen if it wants to,
//      but not data integrity.
gamestate* game_init2() {
	gamestate *gs = (gamestate*)malloc(sizeof(gamestate));
	init(gs);

	addSolid(gs, gs->vb_root,     0, 3000,    0,  1000, 2+32);
	addSolid(gs, gs->vb_root,  1000, 4000, 1000,  1000, 4);
	addSolid(gs, gs->vb_root, 31000, 9000, 1000, 15000, 4);
	iquat r1 = {(int32_t)(FIXP*0.9801), (int32_t)(FIXP*0.1987), 0, 0}; // Just me with a lil' rotation lol
	memcpy(gs->solids[2]->rot, r1, sizeof(r1)); // Array types are weird in C

	return gs;
}

void game_destroy2() {}
void game_destroy() {
	dl_destroy();
	gamestate_destroy();
	velbox_destroy();
	// no "destroy" call for graphics at present, maybe should make a stub for symmetry's sake?
}

void timekeeping(long inputs_nanos, long update_nanos, long follow_nanos) {
	long totalNanos = inputs_nanos + update_nanos + follow_nanos;
	updateTiming(&logicTiming, totalNanos);

	// I should probably make a better place to put this nonsense, 
}

void handleKey(int key, int action) {
	// I can't imagine a scenario where I actually need to know about repeat events
	if (action == GLFW_REPEAT) return;

	     if (key == GLFW_KEY_D)     activeInputs.r = action;
	else if (key == GLFW_KEY_A)     activeInputs.l = action;
	else if (key == GLFW_KEY_S)     activeInputs.d = action;
	else if (key == GLFW_KEY_W)     activeInputs.u = action;
	else if (key == GLFW_KEY_X)	debugPrint = 1;
	else if (action) {
		if (key == GLFW_KEY_F3) renderStats ^= 1;
	}
}

void cursor_position_callback(GLFWwindow *window, double xpos, double ypos) {
	int x = xpos;
	int y = ypos;
	if (mouseDown) {
		float dx = x - mouseX;
		float dy = y - mouseY;
		float dist = sqrt(dx*dx + dy*dy);
		float scale = mouseDown == 1 ? 0.002 : -0.0008;
		float radians = dist * scale;
		// Because of quaternion math, this represents a rotation of `radians*2`.
		// We cap it at under a half rotation, after which I'm not sure my
		// quaternions keep behaving haha
		if (radians > 1.5) radians = 1.5;
		float s = sin(radians);
		// Positive X -> mouse to the right -> rotate about +Z
		// Positive Y -> mouse down -> rotate around +X

		quat r = {cos(radians), s*dy/dist, 0, s*dx/dist};
		if (mouseDown == 1) {
			quat_rotateBy(tmpGameRotation, r);
			quat_norm(tmpGameRotation);
		} else {
			quat o;
			quat_mult(o, r, quatCamRotation);
			quat_norm(o);
			memcpy(quatCamRotation, o, sizeof(quat));
		}
	}
	mouseX = x;
	mouseY = y;
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
	if (action == GLFW_PRESS && (button == 0 || button == 1)) mouseDown = button+1;
	else mouseDown = 0;
}
void scroll_callback(GLFWwindow *window, double x, double y) {}
void window_focus_callback(GLFWwindow *window, int focused) {}

void copyInputs() {
	// This matter a lot more if you're doing anything non-atomic writes,
	// or if serializing the inputs means writing something
	// (like for commands you want to be sent out exactly once).
	sharedInputs = activeInputs;
}

// In theory I think this would let us have a dynamic size of input data per-frame.
// In practice we don't use that, partly because anything that *might* winds up
// being better implemented as a command, which ensures delivery (even if late,
// or stacked up with commands from other frames).
int getInputsSize() { return 3*sizeof(int32_t); }
void serializeInputs(char * dest) {
	// TODO I have all this nice stuff for serializing, and I'm going to ignore it
	int32_t *p = (int32_t*) dest;

	float keyboard_x = (float)(sharedInputs.r-sharedInputs.l);
	float keyboard_y = (float)(sharedInputs.u-sharedInputs.d);
	float dirKeyboard[3] = {keyboard_x, keyboard_y, 0};
	float dirWorld[3];
	quat_apply(dirWorld, quatCamRotation, dirKeyboard);
	range(i, 3) p[i] = 10*dirWorld[i];

	if (watch_dlFlag.load(std::memory_order::acquire)) {
		snprintf(outboundTextQueue.add().items, TEXT_BUF_LEN, "/dl %s", watch_dlPath);
		watch_dlFlag.store(0, std::memory_order::release);
	}
}

int playerInputs(player *p, list<char> const * data) {
	// TODO is this handling players we don't have data for?
	//      I think we re-use inputs in that case...
	//      Does this handle players that have never yet sent anything?
	//      Because that's maybe something we need to handle,
	//      like if we don't properly init `inputs`...
	if (data->num < 12) {
		range(i, 3) p->inputs[i] = 0;
		return 0;
	}

	int32_t *ptr = (int32_t*)(data->items);
	range(i, 3) p->inputs[i] = ptr[i];
	return 12;
}


//// Text command stuff ////

char handleLocalCommand(char * buf, list<char> * outData) {
	if (isCmd(buf, "/dl")) {
		strcpy(loopbackCommandBuffer, buf);
		return 1;
	}
	return 0;
}

char customLoopbackCommand(gamestate *gs, char const * str) {
	if (isCmd(str, "/dl")) {
		if (!str[3]) {
			puts("/dl requires an arg!");
			return 1;
		}
		dl_processFile(str+4, gs);
		return 1;
	}
	return 0;
}

char processBinCmd(gamestate *gs, player *p, char const *data, int chars, char isMe, char isReal) {
	return 0;
}

char processTxtCmd(gamestate *gs, player *p, char *str, char isMe, char isReal) {
	if (isCmd(str, "/c")) {
		// We're restricting this to only run for ourselves to artificially force a desync.
		// Also we don't have player camera direction as part of tracked state, so we
		// actually can't do it in synchrony right now.
		if (isMe) {
			iquat objectRotation;
			range(i, 4) objectRotation[i] = FIXP*quatCamRotation[i];
			mkSolidAtPlayer(gs, p, objectRotation);
		}
	} else {
		// If unprocessed, "main.cpp" puts this in a text chat buffer.
		// We don't render that though, so it's basically lost.
		if (isReal) printf("Unprocessed cmd: %s\n", str);
		return 0;
	}
	// We hit one of the earlier blocks, the command was processed.
	return 1;
}

void prefsToCmds(queue<strbuf> *cmds) {
	// No prefs that need to be sent for now
}

//// graphics stuff! ////

// The supplied gamestate is not being changed by anyone else (owned by the graphics thread),
// but the game thread *can* be cloning it (`dup`) if there's no newer server data yet.
// Graphics thread must bear this in mind if it wants to do any writes to data in `gs`.
void draw(gamestate *gs, int myPlayer, float interpRatio, long drawingNanos, long totalNanos) {
	updateTiming(&renderTiming, drawingNanos);
	setupFrame(gs->players[myPlayer].pos);

	rangeconst(i, gs->solids.num) {
		solid *s = gs->solids[i];
		drawCube(s, s->tex & 31, !!(s->tex & 32));
	}

	setup2d();
	setup2dText();
	char msg[20];
	snprintf(msg, 20, "Hello, World!1! %2d.", gs->vb_root->kids.num);
	drawText(msg, 1, 1);

	player *p = &gs->players[myPlayer];
	snprintf(msg, 20, "t: %5d", p->tmp);
	drawText(msg, 1, 8);
	snprintf(msg, 20, "(%5ld,%5ld,%5ld)", p->prox->pos[0], p->prox->pos[1], p->prox->pos[2]);
	drawText(msg, 1, 15);
	snprintf(msg, 20, "r %6ld", p->prox->r);
	drawText(msg, 1, 22);
	if (renderStats) {
		if (!totalNanos) totalNanos = 1; // Whatever I guess
		snprintf(
			msg, 20, "%3ld%%%3ld%%%3ld%%%3ld%%",
			100*logicTiming.minNanos/FASTER_NANOS,
			100*logicTiming.maxNanos/FASTER_NANOS,
			100*renderTiming.minNanos/totalNanos,
			100*renderTiming.maxNanos/totalNanos
		);
		drawText(msg, 1, textAreaBounds[1]*2-8);
	}


	if (debugPrint) debugPrint = 0; // lol
	// We'll do more stuff here eventually (again)!

	// TODO: Render text chat buffer that main.cpp maintains
}

static void updateTiming(timing *t, long nanos) {
	if (!t->counter) {
		t->counter = 14;
		t->minNanos = t->nextMin;
		t->maxNanos = t->nextMax;
		t->nextMin = t->nextMax = nanos;
	} else {
		t->counter--;
		if (nanos < t->nextMin) t->nextMin = nanos;
		if (nanos > t->nextMax) t->nextMax = nanos;
	}
}
