extern int http_fd;
extern list<int> http_client_fds;
extern cond_t httpCond;

struct httpGameInfo_t {
	char ready;
	u8 team, kit;
};
extern httpGameInfo_t httpGameInfo;

extern char http_accept();
extern void http_read(int client_fd);
extern void http_spawnClient();

extern void http_init();
extern void http_destroy();
