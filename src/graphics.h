extern int gfx_camDist;
extern float gfx_interpRatio;

extern int displayWidth;
extern int displayHeight;
extern float scaleX, scaleY;
extern float displayAreaBounds[2];

extern void initGraphics(); // should be `gfx_init` but this func is old
extern void gfx_destroy();

extern void setupFrame(int64_t const *p1, int64_t const *p2, box *prox);
extern void tint(float r, float g, float b, float a);
extern void drawCube(mover *m, int64_t scale, int tex, int mesh);
extern void setupStipple();

extern void setupTransparent();
extern void drawTrail(offset const start, unitvec const dir, int64_t len);

extern void setup2d();

extern void centeredGrid2d(float boundsY);
extern void selectTex2d(int tex, int texW, int texH);
extern void sprite2d(int spr_off_x, int spr_off_y, int spr_w, int spr_h, float x, float y);

extern void setup2dText();
extern void drawText(char const *str, int x, int y);
