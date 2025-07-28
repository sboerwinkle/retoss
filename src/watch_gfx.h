#include <atomic>

#define WATCH_PATH_LEN 200
extern std::atomic<char> texReloadFlag;
extern char texReloadPath[WATCH_PATH_LEN];
