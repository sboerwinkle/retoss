#include <stdio.h>

#include "../gamestate.h"
#include "../bctx.h"
#include "../dl_helpers.h"

extern int32_t rocketDist, rocketSpeed, rocketAccel;
extern int32_t smokeR, smokeCount;

extern "C" void lvlUpd(gamestate *gs) {
	gs_gravity = var("gravity", 30);
	pl_tractMult = var("t_mult", 1000);
	pl_tractBonus = var("t_bonus", 1000);
	pl_speed = var("speed", 350);
	pl_walkForce = var("f_walk", 60);
	pl_jumpForce = var("f_jump", 180);
	pl_jump = var("jump", 250);
	pl_gummy = var("gummy", 30);
	gfx_camDist = var("cam", 4000);

	gp("rocket");
	rocketDist = var("d", 1200);
	rocketSpeed = var("spd", 600);
	rocketAccel = var("acc", 300);
	smokeR = var("s_r", 2000);
	smokeCount = var("s_#", 2);
}
