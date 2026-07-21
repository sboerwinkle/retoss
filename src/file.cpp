#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include "util.h"
#include "list.h"
#include "file.h"

#ifdef _WIN32
#define AT_FDCWD (-2)
#else
static int data_fd = -1;
#endif

void file_init() {
#ifndef _WIN32
	data_fd = open("data", O_PATH | O_DIRECTORY | O_CLOEXEC);

	if (data_fd == -1) {
		fprintf(stderr, "Failed to get file descriptor for ./data - `open` gave error %s (%s)\n", strerrorname_np(errno), strerror(errno));
		exit(1);
	}
#endif
}

void file_destroy() {
#ifndef _WIN32
	close(data_fd);
#endif
}

static char verifyPath(const char* path) {
	if (*path == '/') {
		fprintf(stderr, "ERROR - path '%s' is absolute, but must be relative.\n", path);
		return 0;
	}
	if (strstr(path, "..")) {
		fprintf(stderr, "ERROR - path '%s' may not contain the sequence '..'\n", path);
		return 0;
	}
	// File contents are sometimes sent over the network (for /load), which is a
	// potential security issue. This was addressed by making commands that accept
	// path arguments be processed entirely locally - paths are never read from network data.
	//
	// Now we are considering certain game events triggering filesystem interactions.
	// While we are still not accepting arbitrary file names over the network in any capacity,
	// it seems prudent to add another level of protection.
	//
	// The checks above are simple, but simple is good. So long as someone hasn't blindly
	// ported this code to a system that doesn't use Unix paths, and so long as you haven't
	// put anything weird in `data/`, this should be watertight.
	// ("weird" in this context means symlinks or hard links to places you don't want
	//  arbirary reads/writes to.)
	// See also `man 7 path_resolution`.
	return 1;
}

static char writeFileInternal(int relativeTo, const char *name, const list<char> *data) {
#ifdef _WIN32
	// TODO I think `openat` prevents "accidental" absolute paths,
	//      I'll need to make sure something like that gets in here.
	//      In general I need to figure out how paths work in mingw land,
	//      part of the guardrails I want is not being able to read/write
	//      arbitrary system files.
	int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC);
#else
	int fd = openat(relativeTo, name, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0664); // perms: rw-rw-r--
#endif
	if (fd == -1) {
		fprintf(
			stderr,
			"ERROR writing %s file '%s': `openat` gave error %s (%s)\n",
			relativeTo == AT_FDCWD ? "core" : "user",
			name,
			strerrorname_np(errno),
			strerror(errno)
		);
		return 1;
	}

	int ret = write(fd, data->items, data->num);
	if (ret != data->num) {
		fprintf(stderr, "ERROR writing file '%s': `write` returned %d when %d was expected.\n", name, ret, data->num);
		// We could always retry a partial write but that's more effort that I'm not sure is necessary
		if (ret == -1) {
			fprintf(stderr, "Error is %s (%s)\n", strerrorname_np(errno), strerror(errno));
		}
		return 1;
	}
	close(fd);
	return 0;
}

char writeFile(const char *name, const list<char> *data) {
	if (!verifyPath(name)) return 1;

#ifdef _WIN32
	char path[200];
	snprintf(path, 200, "data/%s", name);
	return writeFileInternal(1, path, data);
#else
	return writeFileInternal(data_fd, name, data);
#endif
}

// Really the only thing that should be using this is probably `config.cpp`.
// Most things you want to write, you want in the "data/" directory, which
// you should use `writeFile` for.
char writeSystemFile(const char *name, const list<char> *data) {
	return writeFileInternal(AT_FDCWD, name, data);
}

static char readFileInternal(int relativeTo, char const *name, list<char> *out) {
#ifdef _WIN32
	int fd = open(name, O_RDONLY);
#else
	int fd = openat(relativeTo, name, O_RDONLY | O_CLOEXEC);
#endif
	if (fd == -1) {
		printf(
			"ERROR reading %s file '%s': `openat` gave error %s (%s)\n",
			relativeTo == AT_FDCWD ? "core" : "user",
			name,
			strerrorname_np(errno),
			strerror(errno)
		);
		return 1;
	}

	off_t sz = lseek(fd, 0, SEEK_END);
	if (sz == -1) {
		printf("ERROR reading file '%s': `lseek` gave error %s (%s)\n", name, strerrorname_np(errno), strerror(errno));
		close(fd);
		return 1;
	}
	lseek(fd, 0, SEEK_SET);
	out->setMaxUp(out->num + sz);
	ssize_t actual = read(fd, out->items + out->num, sz);
	if (actual != sz) {
		if (actual == -1) {
			fprintf(stderr, "ERROR reading file '%s': `read` gave error %s (%s)\n", name, strerrorname_np(errno), strerror(errno));
		} else {
			fprintf(stderr, "ERROR reading file '%s': allegedly has size %ld, but only read %ld bytes\n", name, sz, actual);
		}
	} else {
		out->num += actual;
	}
	close(fd);
	return actual != sz;
}

char readFile(const char *name, list<char> *out) {
	if (!verifyPath(name)) return 1;

#ifdef _WIN32
	char path[200];
	snprintf(path, 200, "data/%s", name);
	return readFileInternal(1, path, out);
#else
	return readFileInternal(data_fd, name, out);
#endif
}

// Like `readFile`, but not restricted to the "data/" directory.
// For shaders and stuff, not game-logic I/O.
char readSystemFile(const char *name, list<char> *out) {
	return readFileInternal(AT_FDCWD, name, out);
}
