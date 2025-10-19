#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include "serialize.h"

char const *const seriz_versionString = "rTs0";
int const seriz_latestVersion = 0;

// Some global state to make stuff easier.
// Could put this in an object and pass it around if I really needed to,
// but seriz/deser will probably only ever be done on one thread.
char seriz_reading = 0;
int seriz_index = 0;
int seriz_version = 0;
char seriz_complain = 0; // TODO use this in a lot more places
list<char> *seriz_data;

void write8(char v) {
	seriz_data->add(v);
}

char read8() {
	int i = seriz_index;
	if (i >= seriz_data->num) return 0;
	seriz_index++;
	return (*seriz_data)[i];
}

void trans8(char *x) {
	if (seriz_reading) *x = read8();
	else write8(*x);
}

// We don't care about unsigned vs signed char.
// Small method here prevents manually casting each such case.
void trans8(unsigned char *x) {
	trans8((char*) x);
}

// This one can be used outside of serialization if we want
void write32Raw(list<char> *data, int offset, int32_t v) {
	*(int32_t*)(data->items + offset) = htonl(v);
}

void write32(int32_t v) {
	int n = seriz_data->num;
	seriz_data->setMaxUp(n + 4);
	write32Raw(seriz_data, n, v);
	seriz_data->num = n + 4;
}

int32_t read32() {
	int i = seriz_index;
	if (i + 4 > seriz_data->num) return 0;
	seriz_index += 4;
	return ntohl(*(int32_t*)(seriz_data->items + i));
}

void trans32(int32_t *x) {
	if (seriz_reading) *x = read32();
	else write32(*x);
}

// TODO I'm not doing byte-order-conversion here,
//      should make that consistent with other methods.
//      Presumably everything I'm targetting is little-endian anyways.
void write64(int64_t v) {
	int n = seriz_data->num;
	seriz_data->setMaxUp(n + 8);
	*(int64_t*)(seriz_data->items + n) = v;
	seriz_data->num = n + 8;
}

int64_t read64() {
	int i = seriz_index;
	if (i + 8 > seriz_data->num) return 0;
	seriz_index += 8;
	return *(int64_t*)(seriz_data->items + i);
}

void trans64(int64_t *x) {
	if (seriz_reading) *x = read64();
	else write64(*x);
}

void seriz_writeHeader() {
	seriz_data->setMaxUp(seriz_data->num + 4);
	memcpy(&(*seriz_data)[seriz_data->num], seriz_versionString, 4);
	seriz_data->num += 4;
	// Everything looks good with the header, we're starting a new serialization,
	// if we encounter any issue we should complain.
	seriz_complain = 1;
}

int seriz_verifyHeader() {
	if (seriz_data->num < 4) {
		fprintf(stderr, "Only got %d bytes of data, can't deserialize\n", seriz_data->num);
		return -1;
	}
	// Only compare 3 bytes, not 4, since the last one we use as a version
	if (strncmp(seriz_data->items, seriz_versionString, 3)) {
		fprintf(
			stderr,
			"Beginning of serialized data should read \"%c%c%c\", actually reads \"%c%c%c\"\n",
			seriz_versionString[0], seriz_versionString[1], seriz_versionString[2],
			(*seriz_data)[0], (*seriz_data)[1], (*seriz_data)[2]
		);
		return -1;
	}
	seriz_version = seriz_data->items[3] - '0';
	if (seriz_version < 0 || seriz_version > seriz_latestVersion) {
		fprintf(
			stderr,
			"Version number should be between 0 and %d, but found %d\n",
			seriz_latestVersion, seriz_version
		);
		return -1;
	}

	seriz_index = 4;
	// Everything looks good with the header, we're starting a new deserialization,
	// if we encounter any issue we should complain.
	seriz_complain = 1;
	return 0;
}
