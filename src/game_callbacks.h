#pragma once

extern void game_init();
extern gamestate* game_init2();
extern void game_destroy2();
extern void game_destroy();

extern void handleKey(int key, int action);

extern void cursor_position_callback(GLFWwindow *window, double xpos, double ypos);
extern void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
extern void scroll_callback(GLFWwindow *window, double x, double y);
extern void window_focus_callback(GLFWwindow *window, int focused);
extern void copyInputs();
extern int getInputsSize();
extern void serializeInputs(char * dest);
extern int playerInputs(player *p, list<char> const * data);

//// Text command stuff ////

extern char handleLocalCommand(char * buf, list<char> * outData);
extern char customLoopbackCommand(gamestate *gs, char const * str);
extern char processBinCmd(gamestate *gs, player *p, char const *data, int chars, char isMe, char isReal);
extern char processTxtCmd(gamestate *gs, player *p, char *str, char isMe, char isReal);
extern void prefsToCmds(queue<strbuf> *cmds);

//// graphics stuff! ////

extern void draw(gamestate *gs, int myPlayer, float interpRatio, long drawingNanos, long totalNanos);
