#pragma once

#include "list.h"

extern char const *const seriz_versionString;
extern int const seriz_latestVersion;

extern char seriz_reading;
extern int seriz_index;
extern int seriz_version;
extern list<char> *seriz_data;

extern void write8(char v);
extern char read8();
extern void trans8(char *x);
extern void trans8(unsigned char *x);

// This one can be used outside of serialization if we want
extern void write32Raw(list<char> *data, int offset, int32_t v);

extern void write32(int32_t v);
extern int32_t read32();
extern void trans32(int32_t *x);

extern void write64(int64_t v);
extern int64_t read64();
extern void trans64(int64_t *x);

extern void seriz_writeHeader();
extern int seriz_verifyHeader();

extern char seriz_error();

// Assumes the list is valid, but maybe not the right size
template <typename T>
void transItemCount(list<T> *l) {
	trans32(&l->num);
	if (seriz_reading) {
		if (l->num > 10'000) {
			if (seriz_error()) {
				printf("I don't trust that list size, got %d (0x%X)\n", l->num, l->num);
				l->num = 1;
			}
		}
		l->setMaxUp(l->num);
	}
}

template <typename T>
void transWeakRef(T** itm, list<T*> *l) {
	if (seriz_reading) {
		int32_t idx = read32();
		if (idx >= 0 && idx < l->num) {
			*itm = (*l)[idx];
		} else {
			// -1 is the correct way to encode NULL,
			// anything else is an error.
			if (idx != -1 && seriz_error()) {
				printf(
					"Deserialized weak ref '%d' (=0x%X) invalid, only %d items known\n",
					idx,
					idx,
					l->num
				);
			}
			*itm = NULL;
		}
	} else {
		// Writing.
		if (*itm) {
			write32((*itm)->clone.idx);
		} else {
			write32(-1);
		}
	}
}

template <typename T>
void transStrongRef(T* itm, list<T*> *l) {
	if (seriz_reading) {
		l->add(itm);
	} else {
		// We never actually use that list during writing,
		// but we do need to keep track of how many we've serialized.
		// We just abuse `num` for this purpose.
		itm->clone.idx = l->num;
		l->num++;
	}
}

template <typename T>
void transRefList(list<T*> *l) {
	// We assume the list is already initialized,
	// but we may need to reset the contents
	l->num = 0;
}
