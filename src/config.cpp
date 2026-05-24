#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util.h"
#include "json.h"
#include "file.h"

#include "config.h"

#define CONFIG_NAME "retoss"

static char configDir[CFG_BUF_LEN] = {0};
static char configFile[CFG_BUF_LEN] = {0};
static char modified = 0;

char const* cfg_item::get() {
	// Returned as a `char const*` so the caller
	// doesn't accidentally change it somehow.
	return _data;
}

void cfg_item::set(char const *str) {
	if (!present || strcmp(_data, str)) {
		snprintf(_data, CFG_BUF_LEN, "%s", str);
		present = 1;
		modified = 1;
	}
}

void cfg_item::unset() {
	present = 0;
	modified = 1;
}

double cfg_item::getDouble() {
	return strtod(_data, NULL);
}

cfg_item cfg_host = {.name="host"};
cfg_item cfg_port = {.name="port"};
cfg_item cfg_fov_1 = {.name="fov_1"};
cfg_item cfg_fov_2 = {.name="fov_2"};
cfg_item cfg_sensitivity_1 = {.name="sensitivity_1"};
cfg_item cfg_sensitivity_2 = {.name="sensitivity_2"};
cfg_item cfg_aim_1 = {.name="aim_1"};
cfg_item cfg_aim_2 = {.name="aim_2"};
cfg_item cfg_cam_angle_1 = {.name="cam_angle_1"};
cfg_item cfg_cam_angle_2 = {.name="cam_angle_2"};
cfg_item cfg_cam_dist_1 = {.name="cam_dist_1"};
cfg_item cfg_cam_dist_2 = {.name="cam_dist_2"};
cfg_item cfg_pred_shot_self = {.name="pred_shot_self"};
cfg_item cfg_pred_shot_others = {.name="pred_shot_others"};
cfg_item cfg_no_ui = {.name="no_ui"};

static cfg_item *allItems[] = {
	&cfg_host,
	&cfg_port,
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
};

static cfg_item dummy = {.name=""};

// http.cpp has something similar (with some differences) called `findConfig`
cfg_item* cfg_lookup(char const *name) {
	cfg_item **x = allItems;
	for (; *x; x++) {
		if (!strcmp(name, (*x)->name)) {
			return *x;
		}
	}
	printf("No config item with name \"%s\"\n", name);
	return &dummy;
}

///// Logic for the on-disk config file /////

static void consumeString(jsonValue *root, cfg_item *item) {
	char const *name = item->name;
	char *dest = item->_data;
	jsonValue *v = root->get(name);
	if (!v) return;
	if (v->type != J_STR) {
		fprintf(stderr, "Config value for key '%s' should have type J_STR but is %s\n", name, typeStr(v->type));
		return;
	}
	item->present = 1;
	snprintf(dest, CFG_BUF_LEN, "%s", v->getString());
	root->rm(name);
}

void config_init() {
	// The polling thread also writes these configs (when serving HTTP requests).
	// Since the buffers are statically allocated it's not as big a deal, but the
	// final byte should still be zeroed so we're guaranteed to find a null byte.
	for (cfg_item **x = allItems; *x; x++) {
		(*x)->_data[CFG_BUF_LEN-1] = '\0';
	}

	char const *xdg_config_home = getenv("XDG_CONFIG_HOME");
	if (xdg_config_home && *xdg_config_home) {
		printf(QUIET("XDG_CONFIG_HOME='%s'\n"), xdg_config_home);
		snprintf(configDir, CFG_BUF_LEN, "%s/" CONFIG_NAME, xdg_config_home);
	} else {
		puts(QUIET("XDG_CONFIG_HOME not set, checking HOME."));
		char const *home = getenv("HOME");
		if (home && *home) {
			printf(QUIET("HOME='%s'\n"), home);
			snprintf(configDir, CFG_BUF_LEN, "%s/.config/" CONFIG_NAME, home);
		} else {
			puts("HOME not set either, config files will be stored in the working directory.");
			strcpy(configDir, ".");
		}
	}
	snprintf(configFile, 200, "%s/config.json", configDir);
	printf(QUIET("Using config file path: '%s'\n"), configFile);

	int configFd = open(configFile, O_RDONLY | O_CLOEXEC);
	if (configFd == -1) {
		if (errno == ENOENT) {
			puts("Config file doesn't exist, no configs loaded.");
		} else {
			printf("`open` failed for config file. No configs will be loaded. Error is %s (%s).\n", strerrorname_np(errno), strerror(errno));
		}
		return;
	}
	FILE* configStream = fdopen(configFd, "r");
	if (!configStream) {
		// This is... less expected.
		printf("`fdopen` failed for config file. No configs will be loaded. Error is %s (%s).\n", strerrorname_np(errno), strerror(errno));
		close(configFd);
		return;
	}
	jsonValue *configJson = jsonLoad(configStream);
	fclose(configStream);

	if (configJson->type != J_OBJ) {
		// The methods in json.cpp do all the type checking for you,
		// but we're going to get our hands dirty later with the
		// struct internals, so we need an explicit type check here.
		fprintf(stderr, "Config file top-level item should be J_OBJ but is %s. No configs will be loaded.\n", typeStr(configJson->type));
	} else {
		for (cfg_item **x = allItems; *x; x++) {
			consumeString(configJson, *x);
		}

		range(i, configJson->d.obj.num) {
			// This is phrased to be as informative and accurate as possible, but has to cover a few scenarios.
			// It could be an unknown key, or it could be a known key but the type was wrong (e.g. J_NUM not J_STR).
			// It will be erased if the file is written, but the file might not be written (if no config changes happen).
			fprintf(stderr, "Config key '%s' is unused\n", configJson->d.obj[i].key);
		}
		puts(QUIET("Config loaded."));
	}

	configJson->destroy();
	free(configJson);
}

void config_destroy() {
	// We have some strings, but they're all statically allocated so nothing to do here!
}

void config_write() {
	if (!modified) {
		puts(QUIET("No config options changed, not writing config file."));
		return;
	}
	modified = 0;

	if (!*configFile) {
		// Pretty sure this is unreachable currently.
		puts("Config file path somehow not set during startup, not writing config file.");
		return;
	}

	// Try to create the immediate parent dir,
	// since in a typical "~/.config/CONFIG_NAME/config.json" setup it likely won't exist the first time.
	if (mkdir(configDir, 0755) && errno != EEXIST) {
		fprintf(stderr, "Tried to create parent dir at '%s' for config file, but `mkdir` gave error %s (%s). Not writing config file.\n", configDir, strerrorname_np(errno), strerror(errno));
		return;
	}

	jsonValue root;
	root.initObj();
	for (cfg_item **x = allItems; *x; x++) {
		cfg_item &item = **x;
		if (item.present) {
			root.set(item.name)->initStr(strdup(item._data));
		}
	}

	list<char> buffer;
	buffer.init();
	jsonSerialize(&buffer, &root, 0);
	buffer.add('\n');
	root.destroy();

	writeSystemFile(configFile, &buffer);
	buffer.destroy();
}
