struct lookConfig {
	double sensitivity;
	double fovInv;
	double hovCos, hovSin;
	int64_t hovDist;
	u8 aimType; // AIM_ constants defined in game.cpp, graphics shouldn't care.
};

#define GFX_Z_NEAR 100
extern float gfx_camDist;
#define GFX_CAM_DIST_MAX 4000
extern float gfx_interpRatio;
extern float gfx_camHoverCos;
extern float gfx_camHoverSin;
extern float gfx_lookDir[3];

extern int displayWidth;
extern int displayHeight;
extern float scaleX, scaleY;
extern float displayAreaBounds[2];
extern offset gfx_camPos1;
extern offset gfx_camPos2;

extern void newDyntexHolder(dyntex_holder *h);
extern void oldDyntexHolder(dyntex_holder *h);

extern void initGraphics(); // should be `gfx_init` but this func is old
extern void gfx_destroy();

extern void reset3dTexScale();
extern void setupFrame(int64_t const *p1, int64_t const *p2, box *prox, lookConfig *lookCfg);
extern void tint(float r, float g, float b, float a);
extern void drawCube(mover *m, int64_t scale, int tex, int mesh, float alpha);

extern void drawBillboard(offset p1, offset p2, int tex, float x, float y, float w, int64_t r);
extern void drawTrail(offset const start, unitvec const dir, int64_t len, float age_interp);

extern void setup2dDrawing();
extern void spriteColorMult(float r, float g, float b, float a);
extern void spriteColorAdd(float r, float g, float b, float a);

extern void centeredGrid2d(float boundsY);
extern void selectTex2d(int tex, int texW, int texH);
extern void sprite2d(int spr_off_x, int spr_off_y, int spr_w, int spr_h, float x, float y);

extern void setup2dTextDrawing();
extern void drawTextCentered(char const *str, int y);
extern void drawText(char const *str, int x, int y);
