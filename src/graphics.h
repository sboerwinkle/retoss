extern int displayWidth;
extern int displayHeight;
extern float scaleX, scaleY;

extern void initGraphics();

extern void setDisplaySize(int width, int height);

extern void setupFrame();
extern void drawCube(int64_t pos[3], int tex, char sixFaced);

extern void drawSprite(int cameraX, int cameraY, int sprX, int sprY);
