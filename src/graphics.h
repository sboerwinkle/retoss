extern int displayWidth;
extern int displayHeight;
extern float scaleX, scaleY;
extern float textAreaBounds[2];

extern void initGraphics();

extern void setupFrame(int64_t *_camPos);
extern void drawCube(solid *s, int tex, int mesh);

extern void setup2d();
extern void setup2dText();
extern void setup2dSprites();
extern void drawSprite(int cameraX, int cameraY, int sprX, int sprY);
extern void drawText(char const *str, int x, int y);
