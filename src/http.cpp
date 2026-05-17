#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdio.h>
#include <spawn.h>
#include <string.h>

extern char **environ;

#include "util.h"

#include "http.h"

#define BUF_SIZE 4096

int http_fd = -1;
static int serverPort = -1;

//static char buf[BUF_SIZE];

static char const *FAIL_MSG = "WARN: Failed to set up HTTP server, config UI will be unavailable. Issue is:\n\t";
static char const *payload = "HTTP/1.1 200 OK\r\nContent-Length: 38\r\nContent-Type: text/html\r\n\r\n"
"<html><body>Lookit that</body></html>\n";
// TODO: <!DOCTYPE html>

char http_read() {
	int fd;
	if (-1 == (fd = accept(http_fd, NULL, NULL))) {
		printf("WARN: HTTP `accept` failed with: %s (%s)\n", strerrorname_np(errno), strerror(errno));
		// Currently we always leave http_fd open,
		// but if we ever opt to close it we could
		// return `1`.
		return 0;
	}

	// Right now we are the world's dumbest "http server".
	// TODO: Don't ignore result of `write`! Maybe fixup `sendData` from net.c and use that?
	write(fd, payload, strlen(payload));
	close(fd);
	return 0;
}

extern void http_spawnClient() {
	if (http_fd == -1) {
		puts("HTTP server didn't start successfully, so not launching browser.");
		return;
	}
	char url[25];
	snprintf(url, 25, "http://localhost:%d", serverPort);
	// TODO: Maybe close stdout / stdin?
	//       Could either mark them as CLOEXEC before we get here,
	//       or manually request them to be closed with one of the fancy args.
	// TODO: Selectable browser, but `xdg-open` is a good default
	char name[10];
	// TODO bounds check
	strcpy(name, "xdg-open");
	char *const argv[] = {name, url, NULL};
	pid_t ignore;
	posix_spawnp(&ignore, name, NULL, NULL, argv, environ);
	// Todo: Could check for errors I guess,
	//       but it's kind of hard to get useful info
}

void http_init() {
	http_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (http_fd == -1) {
		printf("%s`socket` failed with: %s (%s)\n", FAIL_MSG, strerrorname_np(errno), strerror(errno));
		return;
	}

	struct sockaddr_in address = {
		.sin_family = AF_INET,
		.sin_port = 0,
		.sin_addr = {.s_addr = htonl(INADDR_LOOPBACK)},
	};

	if (-1 == bind(http_fd, (struct sockaddr*)&address, sizeof(address))) {
		printf("%s`bind` failed with: %s (%s)\n", FAIL_MSG, strerrorname_np(errno), strerror(errno));
		close(http_fd);
		http_fd = -1;
		return;
	}

	// The backlog here (5) is sort of arbitrary. I figure browsers may send a bunch of sh*t,
	// but we'll only have one client, so 5 seems reasonable???
	if (-1 == listen(http_fd, 5)) {
		printf("%s`listen` failed with: %s (%s)\n", FAIL_MSG, strerrorname_np(errno), strerror(errno));
		close(http_fd);
		http_fd = -1;
		return;
	}

	socklen_t len = sizeof(address);
	if (-1 == getsockname(http_fd, (struct sockaddr*)&address, &len)) {
		printf("%s`getsockname` failed with: %s (%s)\n", FAIL_MSG, strerrorname_np(errno), strerror(errno));
		close(http_fd);
		http_fd = -1;
		return;
	}

	serverPort = ntohs(address.sin_port);

	// Set `FD_CLOEXEC` on stdin/stdout/stderr.
	// Might move this later, but for now `http` is the only file that spawns processes,
	// so setting these flags is this file's concern.
	range(i, 3) {
		fcntl(i, F_SETFD, FD_CLOEXEC | fcntl(i, F_GETFD));
	}
}

void http_destroy() {
	if (http_fd == -1) return;
	if (-1 == close(http_fd)) {
		printf("WARN: HTTP `close` failed with: %s (%s)\n", strerrorname_np(errno), strerror(errno));
	}
}
