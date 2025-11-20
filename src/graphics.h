extern int displayWidth;
extern int displayHeight;
extern float scaleX, scaleY;

extern void initGraphics();

extern void setDisplaySize(int width, int height);

extern void setupFrame(int64_t *_camPos);
extern void drawCube(int64_t pos[3], float scale, int tex, char sixFaced);

extern void setup2d();
extern void setup2dText();
extern void setup2dSprites();
extern void drawSprite(int cameraX, int cameraY, int sprX, int sprY);
extern void drawText(char const *str, int x, int y);
