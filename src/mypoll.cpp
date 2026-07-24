// This is pretty simple, but I got the skeleton of it from ChatGPT
#include <stdio.h>
#include <unistd.h>

#include "main.h"
#include "net.h"
#include "net2.h"

#ifdef _WIN32

#include <winsock2.h>

#else

#include <poll.h>

#include "list.h"
#include "watch.h"

#define NUMFDS 3
static struct pollfd fds[NUMFDS];

#endif

#include "http.h"
#include "mypoll.h"

// Todo "poll_" prefix on these vars would be nice.
std::atomic<char> texReloadFlag = 0;
char texReloadPath[POLL_BUF_LEN];

std::atomic<char> poll_game_flag = 0;
char poll_game_data[POLL_BUF_LEN];

#ifdef _WIN32

void* mypoll_threadFunc(void *arg) {
	http_preload();
	timeval timeout = {.tv_usec = 200'000};
	fd_set readFds;
	char checkNet = 1, checkHttp = (http_fd != -1);
#define ANY_CHECK (checkNet || checkHttp)
	while (1) {
		if (!globalRunning) return NULL;
		FD_ZERO(&readFds);
		if (checkNet) FD_SET(net_fd, &readFds);
		if (checkHttp) FD_SET(http_fd, &readFds);

		int readable = select(0, &readFds, NULL, NULL, &timeout);
		if (readable == SOCKET_ERROR) {
			printf("mypoll.cpp `select` failed, WSA error = %d\n", WSAGetLastError());
			return NULL;
		}
		if (FD_ISSET(net_fd, &readFds)) {
			if (net2_read()) {
				closeSocket();
				checkNet = 0;
				if (!ANY_CHECK) return NULL;
			}
		}
		if (FD_ISSET(http_fd, &readFds)) {
			if (http_read()) {
				checkHttp = 0;
				if (!ANY_CHECK) return NULL;
			}
		}
	}
#undef ANY_CHECK
}

void mypoll_init() {
	// Shouldn't happen
	if (net_fd == -1) {
		puts("ERROR: net_fd == -1");
		exit(1);
	}
}

#else

void* mypoll_threadFunc(void *arg) {
	while (1) {
		// 200ms timeout.
		// I could probably also set an "ignore" signal handler
		// for SIGUSR and use it to wake up sleeping calls when
		// it's time to quit, but frames are faster than 200ms
		// anyway so it probably doesn't matter.
		int ret = poll(fds, NUMFDS, 200); // 200ms timeout
		if (!globalRunning) return NULL;

		if (ret == -1) {
			perror("mypoll");
			return NULL;
		}

		if (fds[0].revents & POLLIN) {
			if (net2_read()) {
				closeSocket();
				// This makes `poll` ignore this entry going forwards
				fds[0].fd = -1;
			}
		}

		if (fds[1].revents & POLLIN) {
			watch_read();
		}

		if (fds[2].revents & POLLIN) {
			if (http_read()) {
				fds[2].fd = -1;
			}
		}
	}
}

void mypoll_init() {
	// If components are init'd in the right order, this shouldn't happen.
	//
	// The init order is hardcoded, so the only thing I can think of is
	// if one of these other components fails without calling `exit()`.
	if (net_fd == -1) {
		puts("ERROR: net_fd == -1");
		exit(1);
	}
	if (watch_fd == -1) {
		puts("ERROR: watch_fd == -1");
		exit(1);
	}
	// We don't check http_fd, it's not crucial for operation.

	// That out of the way, it's just boring setup stuff.
	fds[0].fd = net_fd;
	fds[0].events = POLLIN;

	fds[1].fd = watch_fd;
	fds[1].events = POLLIN;

	fds[2].fd = http_fd;
	fds[2].events = POLLIN;
}

// Most of this file is #ifdef'd or #else'd around _WIN32
#endif

void mypoll_destroy() {
	// no-op
}
