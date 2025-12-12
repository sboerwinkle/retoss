#include <atomic>

#define WATCH_PATH_LEN 200

extern std::atomic<char> texReloadFlag;
extern char texReloadPath[WATCH_PATH_LEN];

extern std::atomic<char> watch_dlFlag;
extern char watch_dlPath[WATCH_PATH_LEN];
