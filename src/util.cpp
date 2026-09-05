#include <stdlib.h>
#include <stdint.h>

char getNum(const char **c, int32_t *out) {
	char *d;
	int32_t result = strtol(*c, &d, 0);
	if (*c != d) {
		*c = d;
		*out = result;
		return 1;
	}
	return 0;
}
