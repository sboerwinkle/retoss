#include <GLFW/glfw3.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "util.h"
#include "queue.h"
#include "matrix.h"
#include "mtx.h"

// TODO sort some of this
#include "main.h"
#include "gamestate.h"
#include "game_graphics.h" // needs gamestate
#include "graphics.h"
#include "dl.h"
#include "dl_game.h"
#include "bctx.h"
#include "constel.h"
#include "http.h"
#include "lv.h"
#include "mypoll.h"
#include "player.h"
#include "sound.h" // needs game_graphics
#include "bcast.h"
#include "task.h"
#include "config.h"

#include "collision.h" // For raycasting

#include "tasks/tdmScore.h"

#include "game.h"
#include "game_callbacks.h"
#include "main_graphics.h"

struct timing {
	long minNanos, maxNanos;
	long nextMin, nextMax;
	int counter;
};
static timing logicTiming = {0}, renderTiming = {0};
static void updateTiming(timing *t, long nanos);
static int getTeamShirt(char team);

static char mouseGrabbed = 0;
static char mouseDragMode = 0;
static int mouseDragSize = 30, mouseDragSteps = 0;
static double mouseX = 0, mouseY = 0;
static char ctrlPressed = 0, shiftPressed = 0;
enum { AIM_NONE, AIM_LOW, AIM_HIGH };
static lookConfig look1, look2;
static lookConfig* look = &look1;
quat quatCamRotation = {1,0,0,0};
// "dome" as distinct from 6DoF free-look. Felt descriptive to me.
static double domeYaw = 0, domePitch = 0;
// I'm pretty sure writes to a `float` are already atomic on most architectures,
// so I think this compiles about the same as a normal float?
// Todo: Look into std::atomic<float>::is_always_lock_free, maybe warn at compile time if false.
static std::atomic<float> aimAtCamTan = 0;
static list<mover*> crosshairCandidates;
static char renderStats = 0;

static char editMenuState = -1;
static int editMouseAmt = 0, editMouseShiftAmt = 0;
// TODO Really clumsy to have these `char`s that translate to commands;
//      should just have a queue of commands we can move over while `mtx`-locked.
static char doLookGp = 0;
// For now this is just for editing, will need to update it some if it becomes for other stuff too
static int numberPressed = 0;

char shotPredictionRules[2];

struct {
	struct {
		char u, d, l, r, z, Z;
		char jump, shoot;
	} state;
	struct {
		char jump, shoot;
	} event;
} activeInputs = {}, sharedInputs = {};
static char sentJumpState = 0, sentShootState = 0;

//// Config parsing stuff ////

// Could move this to config.cpp,
// but at the moment that file doesn't concern itself with
// the *meaning* of any config values.
// Todo: Maybe a config2.cpp.

static u8 parseAimType(char const *str) {
	if (!strcmp(str, "NONE")) return AIM_NONE;
	if (!strcmp(str, "LOW")) return AIM_LOW;
	if (!strcmp(str, "HIGH")) return AIM_HIGH;
	printf(
		"Unknown aim option \"%s\". Defaulting to \"LOW\". Options are:\n"
		"\tNONE: Crosshair drawn in center of screen, player shoots forwards.\n"
		"\tLOW: Crosshair drawn where player would shoot.\n"
		"\tHIGH: Crosshair drawn in center of screen, player shoots at crosshair.\n"
		,
		str
	);
	return AIM_LOW;
}

static cfg_item& fallback(cfg_item &a, cfg_item &b) {
	return a.present ? a : b;
}

static void readPredictionConfigs() {
	if (cfg_pred_shot_self.present) {
		shotPredictionRules[0] = cfg_pred_shot_self.getDouble();
	} else {
		shotPredictionRules[0] = 2;
	}
	if (cfg_pred_shot_others.present) {
		shotPredictionRules[1] = cfg_pred_shot_others.getDouble();
	} else {
		shotPredictionRules[1] = 2;
	}
}

static void readLookConfigs() {
	look1.sensitivity = cfg_sensitivity_1.getDouble();
	look2.sensitivity = fallback(cfg_sensitivity_2, cfg_sensitivity_1).getDouble();

	// Our "fovInv" corresponds to the cotangent.
	// Rather than compute `1/tan(x)`, we compute `tan(90-x)` (b/c division sux).
	// `/2` is because we need the angle from the center, not the whole fov.
	// And of course the last bit is just degrees-to-radians conversion.
	look1.fovInv = tan((90 - cfg_fov_1.getDouble()/2)*M_PI/180);
	look2.fovInv = tan((90 - fallback(cfg_fov_2, cfg_fov_1).getDouble()/2)*M_PI/180);

	double rads = cfg_cam_angle_1.getDouble()*M_PI/180;
	look1.hovCos = cos(rads);
	look1.hovSin = sin(rads);
	rads = fallback(cfg_cam_angle_2, cfg_cam_angle_1).getDouble()*M_PI/180;
	look2.hovCos = cos(rads);
	look2.hovSin = sin(rads);

	look1.hovDist = cfg_cam_dist_1.getDouble();
	look2.hovDist = fallback(cfg_cam_dist_2, cfg_cam_dist_1).getDouble();

	look1.aimType = parseAimType(cfg_aim_1.get());
	look2.aimType = parseAimType(fallback(cfg_aim_2, cfg_aim_1).get());
}

static void initConfigs() {
	if (!cfg_sensitivity_1.present) {
		cfg_sensitivity_1.set("0.0016");
		if (!cfg_sensitivity_2.present) {
			cfg_sensitivity_2.set("0.0012");
		}
	}
	if (!cfg_fov_1.present) {
		cfg_fov_1.set("70");
		if (!cfg_fov_2.present) {
			cfg_fov_2.set("55.4");
		}
	}
	if (!cfg_cam_angle_1.present) {
		cfg_cam_angle_1.set("15");
	}
	if (!cfg_cam_dist_1.present) {
		cfg_cam_dist_1.set("4000");
		if (!cfg_cam_dist_2.present) {
			cfg_cam_dist_2.set("200");
		}
	}
	if (!cfg_aim_1.present) {
		cfg_aim_1.set("HIGH");
	}

	readLookConfigs();
	readPredictionConfigs();
}

//// Boring init stuff ////

void game_init() {
	crosshairCandidates.init();

	initGraphics();
	task_init();
	velbox_init();
	gamestate_init();
	dl_init();
	bctx_init();
	bcast_init();
	constel_init();

	http_init();
	sound_init();
}

gamestate* game_init2() {
	initConfigs();

	gamestate *gs = (gamestate*)malloc(sizeof(gamestate));
	// It's okay to do level gen in this method, but correct initialization of the gamestate
	// is handled by gamestate.cpp (in this call)
	init(gs);

	lv_tdm1(gs);
	if (!cfg_no_ui.present) http_spawnClient();

	return gs;
}

void game_destroy2() {}
void game_destroy() {
	sound_destroy();
	http_destroy();

	constel_destroy();
	bcast_destroy();
	bctx_destroy();
	dl_destroy();
	gamestate_destroy();
	velbox_destroy();
	task_destroy();
	gfx_destroy();

	crosshairCandidates.destroy();
}

//// Game-Graphics-Communication stuff ////

void ggcDestroy(ggc_msg *msg) {
	if (msg->type == GGC_DYNTEX_OLD) {
		delete msg->data.texHolder;
	}
}

void addGgcMsg(int type, dyntex_holder *data) {
	ggc_msg &x = msgs_game->add();
	x.type = type;
	x.data.texHolder = data;
}

static ggc_msg *addSoundMsg(int32_t time, uint32_t id, int sound) {
	ggc_msg &x = msgs_game->add();
	x.type = GGC_SND;
	x.data.snd.time = time;
	x.data.snd.id = id;
	x.data.snd.sound = sound;
	return &x;
}

void addSound(int32_t time, offset pos, offset vel, uint32_t id, int sound) {
	ggc_msg &x = *addSoundMsg(time, id, sound);
	memcpy(x.data.snd.pos, pos, sizeof(offset));
	memcpy(x.data.snd.vel, vel, sizeof(offset));
	x.data.snd.posType = SND_POS_COORDS;
}

void addPlayerSound(int32_t time, int who, uint32_t id, int sound) {
	ggc_msg &x = *addSoundMsg(time, id, sound);
	x.data.snd.pos[0] = who;
	x.data.snd.posType = SND_POS_PLAYER;
}

//// Input callbacks + related stuff ////

void timekeeping(long inputs_nanos, long update_nanos, long follow_nanos) {
	long totalNanos = inputs_nanos + update_nanos + follow_nanos;
	updateTiming(&logicTiming, totalNanos);

	// I should probably make a better place to put this nonsense, 
}

void handleKey(int key, int action) {
	// I can't imagine a scenario where I actually need to know about repeat events
	if (action == GLFW_REPEAT) return;

	     if (key == GLFW_KEY_D)     activeInputs.state.r = action;
	else if (key == GLFW_KEY_A)     activeInputs.state.l = action;
	else if (key == GLFW_KEY_S)     activeInputs.state.d = action;
	else if (key == GLFW_KEY_W)     activeInputs.state.u = action;
	else if (key == GLFW_KEY_SPACE) {
		activeInputs.state.jump = activeInputs.state.z = action;
		if (action) activeInputs.event.jump = 1;
	}
	else if (key == GLFW_KEY_C)     activeInputs.state.Z = action;
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
	quat o;

	// dome look stuff
	domeYaw   -= dx*look->sensitivity;
	domePitch -= dy*look->sensitivity;
	while (domeYaw >  M_PI) domeYaw -= 2*M_PI;
	while (domeYaw < -M_PI) domeYaw += 2*M_PI;
	if (domePitch > M_PI_2) domePitch = M_PI_2;
	else if (domePitch < -M_PI_2) domePitch = -M_PI_2;
	// Now construct and apply our quats
	double y = domeYaw/2;
	double p = domePitch/2;
	quat yawRot = {(float)cos(y), 0, 0, (float)sin(y)};
	quat pitchRot = {(float)cos(p), (float)sin(p), 0, 0};
	quat_mult(o, pitchRot, yawRot);

	/* Free-look stuff. I'm kinda proud of this, keeping it in!
	float dist = sqrt(dx*dx + dy*dy);
	float scale = -0.0008; // This negation was a later addition, need to factor it through
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
	quat_mult(o, r, quatCamRotation);
	*/

	// Common cleanup stuff
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

	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action) activeInputs.event.shoot = 1;
		activeInputs.state.shoot = action;
	} else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		look = action ? &look2 : &look1;
	}
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

static void copyEvent(char *a, char *b) {
	// If they're both 1, do I clear b?
	// For now I don't, which makes it
	// easier to hit an input every single frame
	// (since we effectively have a queue 1 frame long)
	if (!*a && *b) {
		*a = 1;
		*b = 0;
	}
}

void copyInputs() {
	// This matter a lot more if you're doing anything non-atomic writes,
	// or if serializing the inputs means writing something
	// (like for commands you want to be sent out exactly once).
	sharedInputs.state = activeInputs.state;
	copyEvent(&sharedInputs.event.jump, &activeInputs.event.jump);
	copyEvent(&sharedInputs.event.shoot, &activeInputs.event.shoot);

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

//// Player control stuff ////

static void serializeEvent(char *event, char state, char *sentState, char const *cmdUp, char const *cmdDown) {
	if (*event) {
		*event = 0;
		// Todo We know this is getting serialized out,
		//      whereas `outboundTextQueue` is checked
		//      to see if it's a local command first.
		//      Slight inefficiency.
		strcpy(outboundTextQueue.add().items, cmdUp);
		*sentState = 1;
	}
	if (*sentState && !state) {
		strcpy(outboundTextQueue.add().items, cmdDown);
		*sentState = 0;
	}
}

static void computeAimAtCamRotation(iquat output) {
	double y = domeYaw/2;

	double p = domePitch;
	float tan = aimAtCamTan.load(std::memory_order::relaxed);
	if (isfinite(tan)) {
		p += atan(tan);
		if (tan < 0) p += M_PI;
	} else {
		// Limit case, looking straight up
		p += M_PI_2;
	}
	p /= 2;

	quat yawRot = {(float)cos(y), 0, 0, (float)sin(y)};
	quat pitchRot = {(float)cos(p), (float)sin(p), 0, 0};
	quat o;
	quat_mult(o, pitchRot, yawRot);
	range(i, 4) {
		output[i] = round(FIXP*o[i]);
	}
}

// In theory I think this would let us have a dynamic size of input data per-frame.
// In practice we don't use that, partly because anything that *might* winds up
// being better implemented as a command, which ensures delivery (even if late,
// or stacked up with commands from other frames).
int getInputsSize() { return 7*sizeof(int32_t); }
void serializeInputs(char * dest) {
	// TODO I have all this nice stuff for serializing, and I'm going to ignore it
	int32_t *p = (int32_t*) dest;

	auto &inpState = sharedInputs.state;
	float moveKeyboard[3] = {
		(float)(inpState.r-inpState.l),
		(float)(inpState.u-inpState.d),
		(float)(inpState.z-inpState.Z),
	};
	float moveWorld[3];

	// free-look stuff
	//quat_apply(moveWorld, quatCamRotation, moveKeyboard);

	// dome-look stuff
	{
		double c = cos(domeYaw);
		double s = sin(domeYaw);
		moveWorld[0] = c*moveKeyboard[0] - s*moveKeyboard[1];
		moveWorld[1] = c*moveKeyboard[1] + s*moveKeyboard[0];
		moveWorld[2] = moveKeyboard[2];
	}

	range(i, 3) p[i] = FIXP*moveWorld[i];
	if (look->aimType == AIM_HIGH) {
		computeAimAtCamRotation(p+3);
	} else {
		range(i, 4) p[3+i] = round(quatCamRotation[i]*FIXP);
	}

	// jump stuff.
	serializeEvent(&sharedInputs.event.jump, sharedInputs.state.jump, &sentJumpState, "/_J", "/_j");
	serializeEvent(&sharedInputs.event.shoot, sharedInputs.state.shoot, &sentShootState, "/_S", "/_s");

	char x;
	if ((x = poll_game_flag.load(std::memory_order::acquire))) {
		if (x == 1) {
			// Candidate for DL loading
			snprintf(outboundTextQueue.add().items, TEXT_BUF_LEN, "/dl %s", poll_game_data);
		} else if (x == 2) {
			// Command from HTTP server
			snprintf(outboundTextQueue.add().items, TEXT_BUF_LEN, "%s", poll_game_data);
		}
		poll_game_flag.store(0, std::memory_order::release);
	}
}

void playerInputs(player *p, char const *data, int size) {
	// This always runs for all players (regardless of network state)
	// before the gamestate steps, so this guarantees `oldRot` is initialized.
	memcpy(p->m.oldRot, p->m.rot, sizeof(p->m.rot));

	// This can happen if:
	// - Malicious client
	// - No data yet seen from client
	// - Some other case I'm not sure about, maybe when client is late?
	if (size < 7*(int)sizeof(int32_t)) {
		// Set inputs to zero
		range(i, 3) p->inputs[i] = 0;
		// Reset facing - a visual indicator I guess?
		p->m.rot[0] = FIXP;
		range(i, 3) p->m.rot[i+1] = 0;
		return;
	}

	int32_t *ptr = (int32_t*)(data);
	range(i, 3) p->inputs[i] = ptr[i];
	range(i, 4) p->m.rot[i] = ptr[3+i];
}

//// Text command stuff ////

static void clearSkin(player *p) {
	if (p->skin) {
		p->skin->decr();
		p->skin = NULL;
	}
}

static void setSkin(player *p, char const *name) {
	dyntex_holder *dh = new dyntex_holder();
	p->skin = dh;
	dh->refs = 1;
	dh->descr.baseTex = getTeamShirt(p->team);
	snprintf(dh->descr.str, DYNTEX_BUF_LEN, "%s", name);
	addGgcMsg(GGC_DYNTEX_NEW, dh);
}

static void redoSkin(player *p) {
	dyntex_holder *skin = p->skin;
	if (!skin) return;
	setSkin(p, skin->descr.str);
	skin->decr();
}

static void camToPvar(int axis, dl_var *v, int *out_axis, int *out_sign) {
	quat composedRotation;
	quat_mult(composedRotation, quatCamRotation, v->value.position.rot);
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
	*out_axis = bestAxis;
	*out_sign = bestSign;
}

static int64_t* pvarUpdatePtr(dl_var *v) {
	if (v->value.position.pinned) {
		return v->value.position.transfDest;
	} else {
		return v->value.position.vec;
	}
}

char handleLocalCommand(char * buf, list<char> * outData) {
	if (isCmd(buf, "/ui")) {
		http_spawnClient();
		return 1;
	}
	if (isCmd(buf, "/dl") || isCmd(buf, "/selall")) {
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
			int bestAxis, bestSign;
			camToPvar(axis, &v, &bestAxis, &bestSign);
			pvarUpdatePtr(&v)[bestAxis] += bestSign * v.incr * amt;
		} else if (v.type == VAR_T_ROT) {
			// Input system maps up-down as Z (axis 2) and scroll as Y (axis 1),
			// but we want these two flipped for rotation inputs
			// (e.g. up-down is pitch (axis 1)).
			// Separately, axis 0 feels backwards so flip it.
			if (axis > 0) axis = 3-axis;
			else amt *= -1;
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
	if (isCmd(buf, "/rd")) {
		dl_selectedGroup->touched = 1;
		dl_var &v = dl_selectedGroup->vars[dl_selectedVar];
		v.touched = 1;
		if (v.type == VAR_T_INT) {
			v.value.integer = v.value.integer / v.incr * v.incr;
		} else if (v.type == VAR_T_POS) {
			int bestAxis, dummy;
			camToPvar(1, &v, &bestAxis, &dummy);
			int64_t &x = pvarUpdatePtr(&v)[bestAxis];
			x = x/v.incr*v.incr;
		} else if (v.type == VAR_T_ROT) {
			range(i, 3) {
				int32_t &x = v.value.rotation.angles[i];
				x = x/v.incr*v.incr;
				v.value.rotation.rotParams[i] = round(FIXP*sin((double)x/2/180*M_PI));
			}
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
	if (isCmd(buf, "/_cfgcam")) {
		readLookConfigs();
		return 1;
	}
	if (isCmd(buf, "/_cfgpred")) {
		readPredictionConfigs();
		return 1;
	}
	if (isCmd(buf, "/name")) {
		if (buf[5]) {
			cfg_name.set(buf+6);
		} else {
			cfg_name.unset();
		}
		// Command should still be sent out
		return 0;
	}
	return 0;
}

char customLoopbackCommand(gamestate *gs, char const * str) {
	if (isCmd(str, "/dl")) {
		if (!str[3]) {
			puts("/dl requires an arg!");
			return 1;
		}
		dl_processFile(str+4, gs, myPlayer);
		return 1;
	}
	if (isCmd(str, "/selall")) {
		gs->selection.num = 0;
		gs->selection.addAll(&gs->solids);
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
		// I have no idea if this works correctly lol
		mkSolidAtPlayer(gs, p);
	} else if (isCmd(str, "/team")) {
		char const *pos = str + 5;
		int team;
		if (getNum(&pos, &team)) {
			p->team = team;
			if (isReal) redoSkin(p);
		} else if (isMe && isReal) {
			printf("team = %hhd\n", p->team);
		}
	} else if (isCmd(str, "/name")) {
		// We could do this without waiting for
		// `isReal`, we'd just have to send and
		// process more `ggc_msg`s.
		if (isReal) {
			clearSkin(p);
			if (str[5]) {
				setSkin(p, str+6);
			}
		}
	} else if (isCmd(str, "/_J")) {
		p->jump |= 3; // Set 'jump this frame' and 'jump continuing' bits
	} else if (isCmd(str, "/_j")) {
		p->jump &= ~1; // Clear 'jump continuing' bit
	} else if (isCmd(str, "/_S")) {
		p->shoot = 3;
	} else if (isCmd(str, "/_s")) {
		p->shoot &= 2;
	} else if (isCmd(str, "/lv_tdm1")) {
		if (isReal) {
			prepareGamestateForLoad(gs, 0);
			lv_tdm1(gs);
		}
	} else if (isCmd(str, "/lv_playground")) {
		if (isReal) {
			prepareGamestateForLoad(gs, 0);
			lv_playground(gs);
		}
	} else if (isCmd(str, "/lv_swarm")) {
		if (isReal) {
			prepareGamestateForLoad(gs, 0);
			lv_swarm(gs);
		}
	} else if (isCmd(str, "/lv_peaks")) {
		if (isReal) {
			prepareGamestateForLoad(gs, 0);
			lv_peaks(gs);
		}
	} else if (isCmd(str, "/die")) {
		killPlayer(p);
	} else if (isCmd(str, "/respawn")) {
		softResetPlayer(p);
	} else {
		// If unprocessed, "main.cpp" puts this in a text chat buffer.
		// We don't render that though, so it's basically lost.
		if (isReal) printf("Unprocessed cmd: %s\n", str);
		return 0;
	}
	// We hit one of the earlier blocks, the command was processed.
	return 1;
}

void prefsToCmds() {
	if (cfg_name.present) {
		snprintf(outboundTextQueue.add().items, TEXT_BUF_LEN, "/name %s", cfg_name.get());
	}
}

//// graphics stuff! ////

void renderThreadSwitchOn() {
	sound_grab();
}

void renderThreadSwitchOff() {
	sound_ungrab();
}

static void checkGgc() {
	list<ggc_msg> &l = *msgs_gfx;
	rangeconst(i, l.num) {
		ggc_msg &m = l[i];
		if (m.type == GGC_DYNTEX_NEW) {
			newDyntexHolder(m.data.texHolder);
		} else if (m.type == GGC_DYNTEX_OLD) {
			oldDyntexHolder(m.data.texHolder);
		} else if (m.type == GGC_SND) {
			sound_add(&m.data.snd);
		}
		ggcDestroy(&m);
	}
	l.num = 0;
}

static void setSoundPosition(gamestate const *gs, player const *_p, int playerIx) {
	// Similar to the gfx math, but not quite.
	// - This is based on player pos, not cam pos
	// - Runs even if player in question is dead
	// Also the gfx code doesn't know if it's drawing a player
	// or something else, so doing it there would be a pain
	// anyway!
	player const &p = *_p;
	player const &p2 = gs->players[playerIx];
	offset &dest = sound_playerPositions[playerIx].o;
	range(i, 3) {
		int64_t d1 = p2.m.oldPos[i] - p.m.oldPos[i];
		dest[i] = d1 + gfx_interpRatio * (p2.m.pos[i] - p.m.pos[i] - d1);
	}
}

static int getTeamShirt(char team) {
	if (team >= 0 && team < 2) {
		return TEX_TEAM_SHIRT + team;
	} else {
		return 3;
	}
}

static void drawPlayer(player *p, float alpha) {
	if (!p->alive) return;

	float tint_alpha = 0.5;
	if (p->hits < 3) tint_alpha = 0.1*p->hits;
	tint(1, 0, 0, tint_alpha);

	int sprite;
	int mode = 3;

	if (p->skin) {
		sprite = p->skin->tex;
		// In this case `sprite` is a GL texture ID, not one of ours
		mode |= 32;
	} else {
		sprite = getTeamShirt(p->team);
	}

	drawCube(&p->m, PLAYER_SHAPE_RADIUS, sprite, mode, alpha);
}

static void castCam(gamestate *gs, player *self, offset p1, offset p2, fraction *best) {
	best->numer=PL_SHOOT_RANGE;
	best->denom=FIXP;

	unitvec dir;
	range(i, 3) dir[i] = gfx_lookDir[i] * FIXP;
	// Could use the stuff in vb_root, but the problem is not everything that's
	// present while shooting is still there. For example, player boxes are
	// cleaned up before the end of the step.
	// For now we just try to check the same things that shooting does.
	rangeconst(i, gs->players.num) {
		player *p = &gs->players[i];
		if (p == self) continue;
		raycast_interp(best, &p->m, p1, p2, dir, gfx_interpRatio);
	}

	// This is horribly inefficient, but we're only doing it once per frame,
	// and I just don't care enough. Can fix it later.
	// I think `bcast.cpp` has better code for this, but I'd need to account
	// for interpolation and also make sure the lists used are threadsafe.
	crosshairCandidates.num = 0;
	velbox_query_ts(gs->vb_root, &crosshairCandidates);

	rangeconst(i, crosshairCandidates.num) {
		mover *m = crosshairCandidates[i];
		raycast_interp(best, m, p1, p2, dir, gfx_interpRatio);
	}
}

static void drawCrosshair(gamestate *gs, player *self) {
	centeredGrid2d(256);
	float y;

	if (look->aimType == AIM_HIGH) {
		y = 0;
		// We just draw the crosshair in the middle in this case.
		// The other thread will use the distance calculated here
		// to determine the angle the player is looking, however.
		fraction best;
		offset p1, p2;
		memcpy(p1, gfx_camPos1, sizeof(offset));
		memcpy(p2, gfx_camPos2, sizeof(offset));
		// Start our raycast from above the player. This helps prevent weirdness
		// if another player passes right in front of the camera.
		float start = look->hovCos * gfx_camDist;
		range(i, 3) {
			p1[i] += start * gfx_lookDir[i];
			p2[i] += start * gfx_lookDir[i];
		}
		castCam(gs, self, p1, p2, &best);

		float dist = (float)best.numer*FIXP/best.denom - (gfx_camDist*look->hovCos - start);
		float vert = gfx_camDist * look->hovSin;
		aimAtCamTan.store(vert/dist, std::memory_order::relaxed);
	} else if (look->aimType == AIM_LOW) {
		fraction best;
		castCam(gs, self, self->m.oldPos, self->m.pos, &best);

		float dist = gfx_camDist * look->hovCos + (float)best.numer*FIXP/best.denom;
		float vert = gfx_camDist * look->hovSin;

		if (!dist) y = 0;
		else y = displayAreaBounds[1] * look->fovInv * vert / dist;
	} else { // AIM_NONE
		y = 0;
	}

	// We've got it on the "font" texture for now.
	// We double the resolution b/c we have to draw halves in some cases,
	// and need to split a pixel for that.
	selectTex2d(1, 128, 128);
	if (self->cooldown) {
		// Split crosshair
		float distance = (self->cooldown - gfx_interpRatio)/2;
		// src coords, size, dest coords
		sprite2d(0, 10, 5, 10, -5-distance, y-5);
		sprite2d(5, 10, 5, 10,    distance, y-5);
	} else {
		// Could draw it as 2 halves in this case as well,
		// I'm just not sure if it might look funny b/c of
		// pixel nonsense.
		// You never have the split cursor if you're dead,
		// so we only handle the "dead" case here.
		int x = self->alive ? 0 : 28;
		sprite2d(x, 10, 10, 10, -5, y-5);
	}
}

static void drawSolid(solid *s) {
	// `s->tex & 31` is validated in gamestate.cpp
	// Todo: This is weird and old, can just use (and validate) the whole int
	drawCube(&s->m, s->r, s->tex & 31, s->m.type, 1.0f);
}

// The supplied gamestate is not being changed by anyone else (owned by the graphics thread),
// but the game thread *can* be cloning it (`dup`) if there's no newer server data yet.
// Graphics thread must bear this in mind if it wants to do any writes to data in `gs`.
void draw(gamestate *gs, float interpRatio, long drawingNanos, long totalNanos) {
	updateTiming(&renderTiming, drawingNanos);
	gfx_interpRatio = interpRatio;

	checkGgc();
	player *p = &gs->players[myPlayer];
	box *boxForCamCasting = p->prox == gs->vb_root ? NULL : p->prox;
	setupFrame(p->m.oldPos, p->m.pos, boxForCamCasting, look);

	// Draw normal solids
	rangeconst(i, gs->solids.num) {
		drawSolid(gs->solids[i]);
	}
	// Draw solids in constels
	rangeconst(i, gs->constels.num) {
		constelInst *ci = gs->constels[i];
		// Currently all constels are always expanded, so this is pretty easy
		rangeconst(j, ci->solids.num) {
			drawSolid(&ci->solids[j]);
		}
	}
	// Todo Should maybe make a render fn part of the task
	//      so I can just call them here instead of having
	//      a dispatch table or whatever. But there's also
	//      3D vs 2D rendering to consider, idk yet.
	rangeconst(i, gs->tasks.num) {
		taskInstance &task = gs->tasks[i];
		if (task.defn->id == TSK_DYNAMICS) {
			// TODO I'm being lazy and goofy here
			drawSolid((solid*)task.data);
		}
	}

	sound_playerPositions.setMaxUp(gs->players.num);
	sound_playerPositions.num = gs->players.num;
	rangeconst(i, gs->players.num) {
		setSoundPosition(gs, p, i);
		if (i == myPlayer) continue;
		player *p2 = &gs->players[i];
		drawPlayer(p2, 1.0f);
	}
	tint(0, 0, 0, 0); // Clear tint.

	int32_t now = gs->clock;
	rangeconst(i, gs->trails.num) {
		trail &tr = gs->trails[i];
		drawTrail(tr.origin, tr.dir, tr.len, 1.0f - ((float)(tr.expiry-now)-interpRatio)/TRAIL_LIFETIME);
	}


	// This used to go to 0 right as the camera hits the player's edge,
	// but there are some minor inaccuracies:
	// - Ideally we'd consider the near Z plane, not the camera itself.
	// - Player is a cube, not round, so the angle the camera hovers at
	//   affects the distance to the player's surface.
	// GL will handle clamping if `alpha` goes negative, so no worries there.
	float alpha = (gfx_camDist-PLAYER_SHAPE_RADIUS) / (GFX_CAM_DIST_MAX-PLAYER_SHAPE_RADIUS) * 0.6;
	drawPlayer(p, alpha);


	setup2dDrawing();

	drawCrosshair(gs, p);

	// Draw hearts for player health
	if (p->alive) {
		centeredGrid2d(96);
		selectTex2d(1, 64, 64);
		rangeconst(i, 3 - p->hits) {
			sprite2d(6, 7, 7, 7, -11.5+8*i, displayAreaBounds[1]-7);
		}
	}

	setup2dTextDrawing();

	rangeconst(i, gs->tasks.num) {
		taskInstance &task = gs->tasks[i];
		if (task.defn->id == TSK_TDM) {
			taskTdm_draw(task.data, interpRatio);
			setup2dTextDrawing();
		}
	}

	if (main_typingLen >= 0) {
		drawText(main_textBuffer, 1, 1);
	}
	char msg[20];

	/*
	snprintf(msg, 20, "(%5ld,%5ld,%5ld)", p->prox->pos[0], p->prox->pos[1], p->prox->pos[2]);
	drawText(msg, 1, 8);
	snprintf(msg, 20, "r %6ld", p->prox->r);
	drawText(msg, 1, 15);
	*/

	// Can move all this up the screen some if I want to, the debug stuff above it is gone for now
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

	sound_frame(p->m.oldPos, p->m.pos, now, interpRatio, now - renderPhantomFrames);
}

static void updateTiming(timing *t, long nanos) {
	if (!t->counter) {
		t->counter = 15;
		t->minNanos = t->nextMin;
		t->maxNanos = t->nextMax;
		t->nextMin = t->nextMax = nanos;
	} else {
		if (nanos < t->nextMin) t->nextMin = nanos;
		if (nanos > t->nextMax) t->nextMax = nanos;
	}
	t->counter--;
}
