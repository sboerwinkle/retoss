#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdio.h>
#include <spawn.h>
#include <string.h>

extern char **environ;

#include "util.h"

#include "config.h"
#include "json.h"
#include "mypoll.h"
#include "file.h"

#include "http.h"

#define BUF_SIZE 4096
static char staticBuffer[BUF_SIZE];

int http_fd = -1;
static int serverPort = -1;

static char const *FAIL_MSG = "WARN: Failed to set up HTTP server, config UI will be unavailable. Issue is:\n\t";
static char const *OK_HEADERS = "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n";

// "Rs" = "Response"
list<char> onlyGetsRs, noContentRs, defaultHtml;

static cfg_item *httpConfigs[] = {
	&cfg_name, // Need "name" here b/c we read it, don't expect to write it though
	&cfg_fov_1,
	&cfg_fov_2,
	&cfg_sensitivity_1,
	&cfg_sensitivity_2,
	&cfg_aim_1,
	&cfg_aim_2,
	&cfg_cam_angle_1,
	&cfg_cam_angle_2,
	&cfg_cam_dist_1,
	&cfg_cam_dist_2,
	&cfg_pred_shot_self,
	&cfg_pred_shot_others,
	&cfg_no_ui,
	NULL
	// NOT cfg_browser, that's a program that will be executed. Not safe to configure via HTTP.
};

// Todo: Nearly duplicated in net.c
static void writeAll(int fd, char const *src, int len) {
	while (len) {
		int ret = write(fd, src, len);
		if (ret < 0) {
			printf("WARN: `write` to HTTP client failed with: %s (%s)\n", strerrorname_np(errno), strerror(errno));
			return;
		}
		src += ret;
		len -= ret;
	}
}

static void writeList(int fd, list<char> const *l) {
	writeAll(fd, l->items, l->num);
}

static void write200(int fd, char const *html, int len, char const *contentType) {
	char headers[100];
	int headerBytes = snprintf(headers, 100, OK_HEADERS, len, contentType);
	// Check size. We use `>=` because `headerBytes` is the number
	// of non-null bytes that `snprintf` *wanted* to write, and it
	// always saves one for the null byte at the end.
	if (headerBytes >= 100) {
		puts("WARN: HTTP ran out of buffer to write headers");
		return;
	}
	writeAll(fd, headers, headerBytes);
	writeAll(fd, html, len);
}

static void sendCommand(char const *cmd) {
	// We're on the "poll" thread, which is the only one that's
	// supposed to be writing to `poll_game_data`.
	if (!poll_game_flag.load(std::memory_order::acquire)) {
		snprintf(poll_game_data, POLL_BUF_LEN, "%s", cmd);
		poll_game_flag.store(2, std::memory_order::release);
	} else {
		printf("http couldn't send cmd to main thread: %s\n", cmd);
	}
}

static void writeConfigs(int fd) {
	jsonValue root;
	root.initObj();
	for (cfg_item **x = httpConfigs; *x; x++) {
		cfg_item &item = **x;
		if (item.present) {
			root.set(item.name)->initStr(strdup(item._data));
		}
	}

	list<char> buffer;
	buffer.init();
	jsonSerialize(&buffer, &root, -1);
	buffer.add('\n'); // Probly don't need this but oh well
	root.destroy();

	write200(fd, buffer.items, buffer.num, "application/json");
	buffer.destroy();
}

// Very similar to `cfg_lookup`, but only checks `httpConfigs`, and may return `NULL`.
static cfg_item* findConfig(char *str) {
	for (cfg_item **x = httpConfigs; *x; x++) {
		if (!strcmp(str, (*x)->name)) {
			return *x;
		}
	}
	return NULL;
}

static void readConfigs(char *str) {
	while (1) {
		char *equals = strchr(str, '=');
		if (!equals) return;
		*equals = '\0';
		cfg_item *cfg = findConfig(str);
		if (!cfg) {
			printf("WARN: http: No config by name '%s'\n", str);
			return;
		}
		str = equals+1;

		char *amp = strchr(str, '&');
		if (amp) *amp = '\0';

		// Can't set a config to the empty string with the HTML UI for now
		cfg->simpleSet(str);

		if (!amp) return;
		str = amp+1;
	}
}

static char readFirstLine(int fd) {
	char *buf = staticBuffer;
	int remaining = BUF_SIZE - 1;

	while (1) {
		ssize_t size = read(fd, buf, remaining);
		if (size == 0) {
			puts("WARN: HTTP `read` found EOF, that doesn't seem right");
			return 1;
		}
		if (size == -1) {
			printf("WARN: HTTP `read` failed with: %s (%s)\n", strerrorname_np(errno), strerror(errno));
			return 1;
		}
		buf[size] = '\0';
		char *end = strchr(buf, '\r');
		if (end) {
			*end = '\0';
			return 0;
		}
		buf += size;
		remaining -= size;
	}
}

static void read_inner(int fd) {
	if (readFirstLine(fd)) return;
	// This would be a problem if we were multi-threaded!
	char *buf = staticBuffer;

	if (strncmp("GET ", buf, 4)) {
		writeList(fd, &onlyGetsRs);
		return;
	}
	buf += 4;

	// Convert first space (if any) to a NULL byte.
	// `buf` now holds just the requested path.
	*strchrnul(buf, ' ') = '\0';

	if (!strcmp(buf, "/team0")) {
		sendCommand("/team 0");
		writeList(fd, &noContentRs);
	} else if (!strcmp(buf, "/team1")) {
		sendCommand("/team 1");
		writeList(fd, &noContentRs);
	} else if (!strcmp(buf, "/team2")) {
		sendCommand("/team 2");
		writeList(fd, &noContentRs);
	} else if (!strcmp(buf, "/")) {
		write200(fd, defaultHtml.items, defaultHtml.num, "text/html");
	} else if (!strcmp(buf, "/config")) {
		writeConfigs(fd);
	} else if (!strncmp(buf, "/name/", 6)) {
		// "/name/foo" -> "/name foo"
		// "/name/" -> "/name"
		// Will probably have to rework this when I add URL encoding haha
		buf[5] = buf[6] ? ' ' : '\0';
		sendCommand(buf);
		writeList(fd, &noContentRs);
	} else if (!strncmp(buf, "/camera?", 8)) {
		readConfigs(buf+8);
		sendCommand("/_cfgcam");
		writeList(fd, &noContentRs);
	} else if (!strncmp(buf, "/prediction?", 12)) {
		readConfigs(buf+12);
		sendCommand("/_cfgpred");
		writeList(fd, &noContentRs);
	} else if (!strncmp(buf, "/misc?", 6)) {
		readConfigs(buf+6);
		writeList(fd, &noContentRs);
	} else {
		char const *format = "Unhandled HTTP request to: %s\n";
		if (!strncmp(buf, "/favicon", 8)) {
			format = QUIET("Unhandled HTTP request to: %s\n");
		}
		printf(format, buf);
		// todo: Send 404 instead of 204?
		writeList(fd, &noContentRs);
	}
}

char http_read() {
	int fd;
	if (-1 == (fd = accept(http_fd, NULL, NULL))) {
		printf("WARN: HTTP `accept` failed with: %s (%s)\n", strerrorname_np(errno), strerror(errno));
		// Currently we always leave http_fd open,
		// but we would return `1` if we ever were
		// to close it.
		return 0;
	}

	read_inner(fd);
	close(fd);
	return 0;
}

extern void http_spawnClient() {
	if (http_fd == -1) {
		puts("HTTP server didn't start successfully, so not launching browser.");
		return;
	}

	char *cmd;
	char fallback[9];
	if (cfg_browser.present) {
		// For some reason `argv` is not a `char const *`,
		// but I can't imagine anyone would actually write
		// to it... surely? Even if they do, it'll be from
		// the forked process, so we won't see. The config
		// item's string is in fact writeable, you're just
		// not meant to do it directly.
		cmd = (char*) cfg_browser.get();
	} else {
		cmd = fallback;
		strcpy(fallback, "xdg-open");
	}

	char url[25];
	snprintf(url, 25, "http://localhost:%d", serverPort);
	char *const argv[] = {cmd, url, NULL};
	pid_t ignore;
	posix_spawnp(&ignore, cmd, NULL, NULL, argv, environ);
	// Todo: Could check for errors I guess,
	//       but it's kind of hard to get useful info
}

// This function runs right at the start of the polling thread - after
// the main application startup, but before we handle any http or game
// server communications.
// This might save a little time during startup (disk reads are slow),
// but it also means we're not getting data from the game server until
// these reads finish!
// I will need to revisit this if we start loading too much data here.
void http_preload() {
	char fail = 0;
	fail |= readSystemFile("assets/http/onlyGets.txt", &onlyGetsRs);
	fail |= readSystemFile("assets/http/noContent.txt", &noContentRs);
	fail |= readSystemFile("assets/http/default.html", &defaultHtml);
	if (fail) {
		puts("file(s) needed for UI didn't load correctly (above)");
		// We're not going to do anything else about it lol
	}
}

void http_init() {
	onlyGetsRs.init();
	noContentRs.init();
	defaultHtml.init();

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
	// Might move this eventually, but for now `http` is the only file that spawns processes,
	// so setting these flags is this file's concern.
	range(i, 3) {
		fcntl(i, F_SETFD, FD_CLOEXEC | fcntl(i, F_GETFD));
	}
}

void http_destroy() {
	onlyGetsRs.destroy();
	noContentRs.destroy();
	defaultHtml.destroy();

	if (http_fd == -1) return;
	if (-1 == close(http_fd)) {
		printf("WARN: HTTP `close` failed with: %s (%s)\n", strerrorname_np(errno), strerror(errno));
	}
}
