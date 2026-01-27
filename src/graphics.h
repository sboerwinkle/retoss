extern int displayWidth;
extern int displayHeight;
extern float scaleX, scaleY;
extern float displayAreaBounds[2];

extern void initGraphics();

extern void setupFrame(int64_t *_camPos);
extern void drawCube(mover *m, int64_t scale, int tex, int mesh, float interpRatio);

extern void setup2d();

extern void centeredGrid2d(float boundsY);
extern void selectTex2d(int tex, int texW, int texH);
extern void sprite2d(int spr_off_x, int spr_off_y, int spr_w, int spr_h, float x, float y);

extern void setup2dText();
extern void drawText(char const *str, int x, int y);
