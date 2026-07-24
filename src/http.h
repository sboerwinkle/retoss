extern int http_fd;
extern list<int> http_client_fds;

extern char http_accept();
extern void http_read(int client_fd);
extern void http_spawnClient();

extern void http_init();
extern void http_destroy();
