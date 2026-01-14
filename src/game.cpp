#include <GLFW/glfw3.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "util.h"
#include "queue.h"
#include "matrix.h"
#include "mtx.h"

#include "main.h"
#include "gamestate.h"
#include "graphics.h"
#include "watch_flags.h"
#include "dl.h"
#include "dl_game.h"
#include "bctx.h"

#include "game.h"
#include "game_callbacks.h"

struct timing {
	long minNanos, maxNanos;
	long nextMin, nextMax;
	int counter;
};
static timing logicTiming = {0}, renderTiming = {0};
static void updateTiming(timing *t, long nanos);

static char mouseGrabbed = 0;
static char mouseDragMode = 0;
static int mouseDragSize = 30, mouseDragSteps = 0;
static double mouseX = 0, mouseY = 0;
static char ctrlPressed = 0, shiftPressed = 0;
static char renderStats = 0;
quat tmpGameRotation = {1,0,0,0};
quat quatCamRotation = {1,0,0,0};

static char editMenuState = -1;
static int editMouseAmt = 0, editMouseShiftAmt = 0;
// TODO Really clumsy to have these `char`s that translate to commands;
//      should just have a queue of commands we can move over while `mtx`-locked.
static char doLookGp = 0;
// For now this is just for editing, will need to update it some if it becomes for other stuff too
static int numberPressed = 0;

struct {
	char u, d, l, r, z, Z;
} activeInputs = {}, sharedInputs = {};

void game_init() {
	initGraphics();
	velbox_init();
	gamestate_init();
	dl_init();
	bctx_init();
}

gamestate* game_init2() {
	gamestate *gs = (gamestate*)malloc(sizeof(gamestate));
	// It's okay to do level gen in this method, but correct initialization of the gamestate
	// is handled by gamestate.cpp (in this call)
	init(gs);

	addSolid(gs, gs->vb_root,     0, 3000,    0,  1000, 0, 2+32);
	addSolid(gs, gs->vb_root,  1000, 4000, 1000,  1000, 0, 4);
	addSolid(gs, gs->vb_root, 31000, 9000, 1000, 15000, 1, 4);
	iquat r1 = {(int32_t)(FIXP*0.9801), (int32_t)(FIXP*0.1987), 0, 0}; // Just me with a lil' rotation lol
	memcpy(gs->solids[2]->rot, r1, sizeof(r1)); // Array types are weird in C

	return gs;
}

void game_destroy2() {}
void game_destroy() {
	bctx_destroy();
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
	else if (key == GLFW_KEY_SPACE) activeInputs.z = action;
	else if (key == GLFW_KEY_C)     activeInputs.Z = action;
	else if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL) {
		ctrlPressed = action;
	} else if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT) {
		shiftPressed = action;
	} else if (action) {
		if (key == GLFW_KEY_F3) {
			renderStats ^= 1;
		} else if (key == GLFW_KEY_E) {
			if (ctrlPressed) {
				editMenuState = ~editMenuState;
			}
		} else if (key == GLFW_KEY_F) {
			if (ctrlPressed) {
				doLookGp = 1;
			}
		} else if (key == GLFW_KEY_ESCAPE) {
			mouseGrab(0);
			mouseGrabbed = 0;
		} else if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) { // We exclude 0 because it's less than 1 and I'm paranoid
			numberPressed = key+1-GLFW_KEY_1;
		}
	}
}

static void handleLook(double dx, double dy) {
	float dist = sqrt(dx*dx + dy*dy);
	float scale = -0.0008;
	float radians = dist * scale;
	// Because of quaternion math, this represents a rotation of `radians*2`.
	// We cap it at under a half rotation, after which I'm not sure my
	// quaternions keep behaving haha
	if (radians > 1.5) radians = 1.5;
	float s = sin(radians);
	// (Not sure if the below notes are still right actually:)
	// Positive X -> mouse to the right -> rotate about +Z
	// Positive Y -> mouse down -> rotate around +X

	quat r = {cos(radians), (float)(s*dy/dist), 0, (float)(s*dx/dist)};
	quat o;
	quat_mult(o, quatCamRotation, r);
	quat_norm(o);
	memcpy(quatCamRotation, o, sizeof(quat));
}

static void handleDrag(double x, double y) {
	if (mouseDragMode == 1) {
		// Still need to pick a direction
		if (fabs(x-mouseX) >= mouseDragSize) {
			mouseDragMode = 2;
		} else if (fabs(y-mouseY) >= mouseDragSize) {
			mouseDragMode = 3;
		} else {
			return;
		}
	}
	// We know which direction we're looking at (X or Y)
	double *a = mouseDragMode == 2 ? &mouseX : &mouseY;
	double b = mouseDragMode == 2 ? x : y;
	int steps = (b-*a)/mouseDragSize;
	*a += mouseDragSize * steps;
	mouseDragSteps += steps;
}

void cursor_position_callback(GLFWwindow *window, double x, double y) {
	if (mouseGrabbed) {
		if (mouseDragMode == 0) {
			handleLook(x - mouseX, y - mouseY);
		} else if (mouseDragMode == -1) {
			// Don't look this frame, just update mouseX/mouseY
			mouseDragMode = 0;
		} else {
			handleDrag(x, y);
			return;
		}
	}
	mouseX = x;
	mouseY = y;
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
	if (!mouseGrabbed && action == GLFW_PRESS) {
		mouseGrab(1);
		mouseGrabbed = 1;
		return;
	}
	if (mouseDragMode > 0 && button == GLFW_MOUSE_BUTTON_LEFT && !action) {
		if (editMenuState == 0 && mouseDragMode == 1) {
			// Mouse wasn't actually dragged (this is just a click),
			// and the edit menu is visible and at the top level.
			// Go into the deeper level.
			editMenuState = 1;
		}
		// Stop dragging the mouse.
		// -1 is a short-lived state that prevents the view jumping.
		mouseDragMode = -1;
		return;
	}
	if (ctrlPressed && editMenuState >= 0) {
		if (!action) return;
		if (button == 0) {
			mouseDragMode = 1;
		} else if (button == 1) {
			if (editMenuState == 1) editMenuState = 0;
		}
		return;
	}

	/*
	if (action == GLFW_PRESS && (button == 0 || button == 1)) mouseDown = button+1;
	else mouseDown = 0;
	*/
}
void scroll_callback(GLFWwindow *window, double x, double y) {
	if (ctrlPressed) {
		if (shiftPressed) editMouseShiftAmt += y;
		else editMouseAmt += y;
	}
}
void window_focus_callback(GLFWwindow *window, int focused) {
	if (!focused) {
		mouseGrab(0);
		mouseGrabbed = 0;
	}
}

void copyInputs() {
	// This matter a lot more if you're doing anything non-atomic writes,
	// or if serializing the inputs means writing something
	// (like for commands you want to be sent out exactly once).
	sharedInputs = activeInputs;

	if (editMenuState >= 0) {
		char const *cmd;
		if (editMouseAmt) {
			if (editMenuState == 0) cmd = "/dlVarSel";
			else cmd = "/v@+ 1";
			snprintf(outboundTextQueue.add().items, TEXT_BUF_LEN, "%s %d", cmd, editMouseAmt);
		}
		if (editMouseShiftAmt) {
			cmd = "/dlVarInc";
			snprintf(outboundTextQueue.add().items, TEXT_BUF_LEN, "%s %d", cmd, editMouseShiftAmt);
		}
		if (numberPressed) {
			snprintf(outboundTextQueue.add().items, TEXT_BUF_LEN, "/hotbar %d", numberPressed);
		}
		if (doLookGp) {
			strcpy(loopbackCommandBuffer, "/lookAtGp");
		}
	}
	editMouseAmt = editMouseShiftAmt = 0;
	numberPressed = 0;
	doLookGp = 0;

	if (mouseDragSteps) {
		if (mouseDragMode > 1) {
			int axis;
			if (mouseDragMode == 2) {
				axis = 0;
			} else {
				axis = 2;
				mouseDragSteps *= -1;
			}
			snprintf(outboundTextQueue.add().items, TEXT_BUF_LEN, "/v@+ %d %d", axis, mouseDragSteps);
		}
		mouseDragSteps = 0;
	}
}

// In theory I think this would let us have a dynamic size of input data per-frame.
// In practice we don't use that, partly because anything that *might* winds up
// being better implemented as a command, which ensures delivery (even if late,
// or stacked up with commands from other frames).
int getInputsSize() { return 3*sizeof(int32_t); }
void serializeInputs(char * dest) {
	// TODO I have all this nice stuff for serializing, and I'm going to ignore it
	int32_t *p = (int32_t*) dest;

	float dirKeyboard[3] = {
		(float)(sharedInputs.r-sharedInputs.l),
		(float)(sharedInputs.u-sharedInputs.d),
		(float)(sharedInputs.z-sharedInputs.Z),
	};
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
	if (isCmd(buf, "/dlVarSel")) {
		char const *pos = buf + 9;
		int x;
		if (getNum(&pos, &x)) {
			int num = dl_selectedGroup->vars.num;
			// Flip sign on x, since we display them top-to-bottom
			dl_selectedVar = ((dl_selectedVar-x)%num+num)%num;
		}
		return 1;
	}
	if (isCmd(buf, "/v@+")) {
		char const *pos = buf + 4;
		int axis, amt;
		if (!getNum(&pos, &axis) || !getNum(&pos, &amt)) {
			puts("/v@+ requires 2 (numeric) args!");
			return 1;
		}
		if (!(axis >= 0 && axis < 3)) {
			puts("/v@+ invalid axis");
			return 1;
		}
		dl_selectedGroup->touched = 1;
		dl_var &v = dl_selectedGroup->vars[dl_selectedVar];
		v.touched = 1;
		if (v.type == VAR_T_INT) {
			v.value.integer += amt * v.incr;
		} else if (v.type == VAR_T_POS) {
			quat composedRotation;
			quat_mult(composedRotation, quatCamRotation, v.value.position.rot);
			float cameraSpace[3] = {0,0,0};
			cameraSpace[axis] = 1;
			float variableSpace[3];
			quat_apply(variableSpace, composedRotation, cameraSpace);
			int bestAxis = 0;
			float bestComponent = 0;
			int bestSign = 0;
			range(i, 3) {
				float component = fabs(variableSpace[i]);
				if (component > bestComponent) {
					bestComponent = component;
					bestAxis = i;
					bestSign = variableSpace[i] > 0 ? 1 : -1;
				}
			}
			v.value.position.vec[bestAxis] += bestSign * v.incr * amt;
		} else if (v.type == VAR_T_ROT) {
			// Input system maps up-down as Z (axis 2) and scroll as Y (axis 1),
			// but we want these two flipped for rotation inputs
			// (e.g. up-down is pitch (axis 1))
			if (axis > 0) axis = 3-axis;
			int32_t angle = v.value.rotation.angles[axis];
			angle += amt * v.incr;
			while (angle >  180) angle -= 360;
			while (angle < -180) angle += 360;
			v.value.rotation.angles[axis] = angle;
			v.value.rotation.rotParams[axis] = round(FIXP*sin((double)angle/2/180*M_PI));
		}
		strcpy(loopbackCommandBuffer, "/dlUpd");
		return 1;
	}
	if (isCmd(buf, "/dlVarInc")) {
		char const *pos = buf + 9;
		int x;
		if (getNum(&pos, &x)) {
			dl_var &v = dl_selectedGroup->vars[dl_selectedVar];
			if (v.type == VAR_T_ROT) {
				// Only increments for rotations are 1/15/90.
				// If you can do that more elegantly, go for it.
				if (x > 0) {
					if (x == 1 && v.incr < 15) v.incr = 15;
					else v.incr = 90;
				} else if (x < 0) {
					if (x == -1 && v.incr > 15) v.incr = 15;
					else v.incr = 1;
				}
			} else {
				while (x > 0) {
					x--;
					v.incr *= 10;
				}
				while (x < 0) {
					x++;
					v.incr /= 10;
				}
				if (!v.incr) v.incr = 1;
			}
		}
		return 1;
	}
	if (!strncmp(buf, "/gp ", 4)) {
		dl_selectGp(buf+4);
		return 1;
	}
	if (isCmd(buf, "/bake")) {
		dl_bake();
		return 1;
	}
	if (isCmd(buf, "/hotbar")) {
		if (buf[7]) dl_hotbar(buf+8);
		else dl_hotbar("");
		return 1;
	}
	return 0;
}

char customLoopbackCommand(gamestate *gs, int myPlayer, char const * str) {
	if (isCmd(str, "/dl")) {
		if (!str[3]) {
			puts("/dl requires an arg!");
			return 1;
		}
		dl_processFile(str+4, gs, myPlayer);
		return 1;
	}
	if (isCmd(str, "/dlUpd")) {
		dl_upd(gs, myPlayer);
		return 1;
	}
	if (isCmd(str, "/lookAtGp")) {
		dl_lookAtGp(gs, myPlayer);
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
		int mesh = s->shape + (s->tex & 32); // jank, just hits the cases we need atm
		drawCube(s, s->tex & 31, mesh);
	}

	setup2d();

	// Crosshair:
	// Want a pixel to be 1/256 of vertical screen space (so +/-128)
	centeredGrid2d(128);
	// We've got it on the "font" texture for now
	selectTex2d(1, 64, 64);
	// src coords+size, dest coords
	sprite2d(0, 59, 5, 5, -2.5, -2.5);

	setup2dText();
	if (main_typingLen >= 0) {
		drawText(main_textBuffer, 1, 1);
	}
	char msg[20];

	player *p = &gs->players[myPlayer];
	snprintf(msg, 20, "t: %5d", p->tmp);
	drawText(msg, 1, 8);
	snprintf(msg, 20, "(%5ld,%5ld,%5ld)", p->prox->pos[0], p->prox->pos[1], p->prox->pos[2]);
	drawText(msg, 1, 15);
	snprintf(msg, 20, "r %6ld", p->prox->r);
	drawText(msg, 1, 22);

	if (editMenuState >= 0) {
		mtx_lock(dl_varMtx);
		if (editMenuState == 0) {
			list<dl_var> &vars = dl_selectedGroup->vars;
			rangeconst(i, vars.num) {
				dl_var &v = vars[i];
				if (v.type == VAR_T_INT) {
					snprintf(msg, 20, "%s: %ld", v.name, v.value.integer);
				} else if (v.type == VAR_T_POS) {
					snprintf(
						msg, 20, "%s: %ld, %ld, %ld",
						v.name,
						v.value.position.vec[0],
						v.value.position.vec[1],
						v.value.position.vec[2]
					);
				} else if (v.type == VAR_T_ROT) {
					snprintf(
						msg, 20, "%s: %d, %d, %d",
						v.name,
						v.value.rotation.angles[0],
						v.value.rotation.angles[1],
						v.value.rotation.angles[2]
					);
				}
				drawText(msg, 7, 1+7*(i+4));
			}
			drawText(">", 1, 1+7*(dl_selectedVar+4));
		} else {
			dl_var &v = dl_selectedGroup->vars[dl_selectedVar];
			snprintf(msg, 20, "%s (+/-%ld)", v.name, v.incr);
			drawText(msg, 7, 29);
			if (v.type == VAR_T_INT) {
				snprintf(msg, 20, "%ld", v.value.integer);
				drawText(msg, 7, 36);
			} else if (v.type == VAR_T_POS) {
				range(i, 3) {
					snprintf(
						msg, 20, "%ld",
						v.value.position.vec[i]
					);
					drawText(msg, 7, 36 + 7*i);
				}
			} else if (v.type == VAR_T_ROT) {
				range(i, 3) {
					snprintf(
						msg, 20, "%d",
						v.value.rotation.angles[i]
					);
					drawText(msg, 7, 36 + 7*i);
				}
			}
		}

		{
			char const *gpName = dl_selectedGroup->name;
			int len = strlen(gpName);
			drawText(gpName, displayAreaBounds[0]*2-5*len, 1);
		}

		mtx_unlock(dl_varMtx);
	}

	if (renderStats) {
		if (!totalNanos) totalNanos = 1; // Whatever I guess
		snprintf(
			msg, 20, "%3ld%%%3ld%%%3ld%%%3ld%%",
			100*logicTiming.minNanos/FASTER_NANOS,
			100*logicTiming.maxNanos/FASTER_NANOS,
			100*renderTiming.minNanos/totalNanos,
			100*renderTiming.maxNanos/totalNanos
		);
		drawText(msg, 1, displayAreaBounds[1]*2-8);
	}
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
