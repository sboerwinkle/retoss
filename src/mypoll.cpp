#ifdef _WIN32
// TODO I just want to get this off the ground, but I'll need to actually get this to work properly in windows-land.

#include <unistd.h>

#include "main.h"
#include "net.h"
#include "net2.h"

#include "mypoll.h"

std::atomic<char> texReloadFlag = 0;
char texReloadPath[POLL_BUF_LEN];

std::atomic<char> poll_game_flag = 0;
char poll_game_data[POLL_BUF_LEN];

void* mypoll_threadFunc(void *arg) {
	while (1) {
		if (!globalRunning) return NULL;
		if (net2_read()) {
			closeSocket();
			return NULL;
		}
	}
}

void mypoll_init() {}
void mypoll_destroy() {}

#else



// This is pretty simple, but I got the skeleton of it from ChatGPT
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

#include "list.h"
#include "net.h"
#include "net2.h"
#include "watch.h"
#include "http.h"
#include "main.h"

#include "mypoll.h"

#define NUMFDS 3
static struct pollfd fds[NUMFDS];

// Todo "poll_" prefix on these vars would be nice.
std::atomic<char> texReloadFlag = 0;
char texReloadPath[POLL_BUF_LEN];

std::atomic<char> poll_game_flag = 0;
char poll_game_data[POLL_BUF_LEN];

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

void mypoll_destroy() {
	// no-op
}

// End of `#ifdef _WIN32`...`#else` from top of file
#endif
