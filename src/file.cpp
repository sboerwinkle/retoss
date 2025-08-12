#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include "list.h"
#include "file.h"

static int data_fd = -1;

void file_init() {
	data_fd = open("data", O_PATH | O_DIRECTORY);

	if (data_fd == -1) {
		fprintf(stderr, "Failed to get file descriptor for ./data - `open` gave error %s (%s)\n", strerrorname_np(errno), strerror(errno));
		exit(1);
	}
}

void file_destroy() {
	close(data_fd);
}

static const char* resolvePath(const char* path) {
	if (*path == '/') {
		fprintf(stderr, "ERROR - path '%s' is absolute, but must be relative.\n", path);
		return NULL;
	}
	if (strstr(path, "..")) {
		fprintf(stderr, "ERROR - path '%s' may not contain the sequence '..'\n", path);
		return NULL;
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
	return path;
}

static char writeFileInternal(int fd, const char *name, const list<char> *data) {
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
	name = resolvePath(name);
	if (!name) return 1;

	int fd = openat(data_fd, name, O_WRONLY | O_CREAT | O_TRUNC, 0664); // perms: rw-rw-r--
	if (fd == -1) {
		fprintf(stderr, "ERROR writing file '%s': `openat` gave error %s (%s)\n", name, strerrorname_np(errno), strerror(errno));
		return 1;
	}

	return writeFileInternal(fd, name, data);
}

// Really the only thing that should be using this is probably `config.cpp`.
// Most things you want to write, you want in the "data/" directory, which
// you should use `writeFile` for.
char writeSystemFile(const char *name, const list<char> *data) {
	int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0664); // perms: rw-rw-r--
	if (fd == -1) {
		fprintf(stderr, "ERROR writing file '%s': `open` gave error %s (%s)\n", name, strerrorname_np(errno), strerror(errno));
		return 1;
	}

	return writeFileInternal(fd, name, data);
}

static char readFileInternal(int fd, char const *name, list<char> *out) {
	off_t sz = lseek(fd, 0, SEEK_END);
	if (sz == -1) {
		fprintf(stderr, "ERROR from `lseek`: %s (%s)\n", strerrorname_np(errno), strerror(errno));
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
	name = resolvePath(name);
	if (!name) return 1;

	int fd = openat(data_fd, name, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "ERROR reading file '%s': `openat` gave error %s (%s)\n", name, strerrorname_np(errno), strerror(errno));
		return 1;
	}

	return readFileInternal(fd, name, out);
}

// Like `readFile`, but not restricted to the "data/" directory.
// For shaders and stuff, not game-logic I/O.
char readSystemFile(const char *name, list<char> *out) {
	int fd = open(name, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "ERROR reading file '%s': `open` gave error %s (%s)\n", name, strerrorname_np(errno), strerror(errno));
		return 1;
	}

	return readFileInternal(fd, name, out);
}
