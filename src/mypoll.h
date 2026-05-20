#include <atomic>

#define POLL_BUF_LEN 200

extern std::atomic<char> texReloadFlag;
extern char texReloadPath[POLL_BUF_LEN];

extern std::atomic<char> poll_game_flag;
extern char poll_game_data[POLL_BUF_LEN];

extern void* mypoll_threadFunc(void *arg);

extern void mypoll_init();

extern void mypoll_destroy();
