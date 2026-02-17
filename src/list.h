#pragma once

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

template <typename T>
using Comparator = char (*)(const T &a, const T &b);

template <typename T> class list {
public:
	T *items;
	int num, max;
	void init(int size = 1);
	void init(const list<T> &other);
	void destroy();
	T& operator[](int ix);
	const T& operator[] (int ix) const;
	void display(FILE* f) const;
	T& add();
	void add(const T &itm);
	void s_add(const T &itm);
	T& ins(const T &itm, int ix);
	char has(const T &itm) const;
	char s_has(const T &itm) const;
	int find(const T &itm) const;
	int s_find(const T &itm) const;
	int s_find(const T &itm, int lo, int hi) const;
	int s_find(const T &itm, Comparator<T> comp) const;
	int s_find(const T &itm, int lo, int hi, Comparator<T> comp) const;
	void addAll(const list<T> *other);
	void setMax(int size);
	void setMaxUp(int size);
	void stableRm(const T &itm);
	void quickRm(const T &itm);
	void s_rm(const T &itm);
	void stableRmAt(int ix);
	void quickRmAt(int ix);
	void rmAll(const list<T> &other);
	void diff(const list<T> &other);

	void qsort();
	void qsort(Comparator<T> comp);
	char sorted() const;

	// min-heap stuff (AI generated, manually reviewed)
	// Assumes `<` is defined for `T`
	void heap_push(const T &itm);
	T heap_pop();
	void heapify();

private:
	void qsort(int low, int high);
	void qsort(Comparator<T> comp, int low, int high);

	void heap_siftUp(int ix);
	void heap_siftDown(int ix);
};

template <typename T>
void list<T>::init(int size) {
	items = (T*)malloc(sizeof(T)*size);
	num = 0;
	max = size;
}

template <typename T>
void list<T>::init(const list<T> &other) {
	max = other.num ? other.num : 1;
	num = other.num;
	items = (T*)malloc(sizeof(T)*max);
	memcpy(items, other.items, sizeof(T)*num);
}

template <typename T>
void list<T>::destroy() {
	free(items);
}

template <typename T>
T& list<T>::operator[](int ix) {
	return items[ix];
}

template <typename T>
const T& list<T>::operator[](int ix) const {
	return items[ix];
}
template <typename T>
T& list<T>::add() {
	if (num == max) setMax(max * 2 + 1);
	return items[num++];
}

template <typename T>
void list<T>::add(const T &itm) {
	add() = itm;
}

template <typename T>
void list<T>::s_add(const T &itm) {
	int where = s_find(itm);
	if (where < 0) where = ~where;
	ins(itm, where);
}

template <typename T>
T& list<T>::ins(const T &itm, int ix) {
	if (num == max) setMax(max * 2 + 1);
	for (int i = num; i > ix; i--) {
		items[i] = items[i-1];
	}
	items[ix] = itm;
	num++;
	return items[ix];
}

template <typename T>
char list<T>::has(const T &itm) const {
	for (int i = num-1; i >= 0; i--) if (items[i] == itm) return 1;
	return 0;
}

template <typename T>
char list<T>::s_has(const T &itm) const {
	return s_find(itm) >= 0;
}

template <typename T>
int list<T>::find(const T &itm) const {
	for (int i = 0; i < num; i++) if (items[i] == itm) return i;
	return -1;
}

template <typename T>
int list<T>::s_find(const T &itm) const {
	return s_find(itm, 0, num);
}

template <typename T>
int list<T>::s_find(const T &itm, Comparator<T> comp) const {
	return s_find(itm, 0, num, comp);
}

template <typename T>
int list<T>::s_find(const T &itm, int lo, int hi) const {
	while (lo < hi) {
		int testIx = (lo + hi) / 2;
		T test = items[testIx];
		if (!(itm <= test)) {
			lo = testIx + 1;
			continue;
		}
		if (!(test <= itm)) {
			hi = testIx;
			continue;
		}
		return testIx;
	}
	return ~lo;
}

template <typename T>
int list<T>::s_find(const T &itm, int lo, int hi, Comparator<T> comp) const {
	while (lo < hi) {
		int testIx = (lo + hi) / 2;
		T test = items[testIx];
		if (!(*comp)(itm, test)) {
			lo = testIx + 1;
			continue;
		}
		if (!(*comp)(test, itm)) {
			hi = testIx;
			continue;
		}
		return testIx;
	}
	return ~lo;
}

template <typename T>
void list<T>::addAll(const list<T> *other) {
	int n2 = num+other->num;
	if (n2 > max) setMax(n2);
	memcpy(items+num, other->items, sizeof(T)*other->num);
	num = n2;
}

template <typename T>
void list<T>::setMax(int size) {
	items = (T*)realloc(items, sizeof(T)*size);
	max = size;
}

template <typename T>
void list<T>::setMaxUp(int size) {
	if (size <= max) return;
	int newMax = max;
	while (newMax < size) newMax = 2 * newMax + 1;
	setMax(newMax);
}

template <typename T>
void list<T>::stableRm(const T &itm) {
	int i;
	// Don't use != since it may not be defined for typename T
	for (i = 0; !(items[i] == itm); i++);
	stableRmAt(i);
}

template <typename T>
void list<T>::quickRm(const T &itm) {
	int i;
	// Don't use != since it may not be defined for typename T
	for (i = 0; !(items[i] == itm); i++);
	quickRmAt(i);
}

template <typename T>
void list<T>::s_rm(const T &itm) {
	int ix = s_find(itm);
#ifdef DEBUG
	if (ix < 0) {
		fputs("s_rm on an absent object!\n", stderr);
		exit(1);
	}
#endif
	stableRmAt(ix);
}

template <typename T>
void list<T>::stableRmAt(int ix) {
	num--;
	memmove(&items[ix], &items[ix+1], sizeof(*items) * (num-ix));
}

template <typename T>
void list<T>::quickRmAt(int ix) {
	num--;
	items[ix] = items[num];
}

template <typename T>
void list<T>::rmAll(const list<T> &other) {
	if (!other.num) return;
	int oix = 0;
	while (1) {
		int dest = 0;
		for (int ix = 0; ix < num; ix++) {
			if (!(items[ix] == other[oix])) {
				//Not the item we're looking for, move along.
				items[dest++] = items[ix];
				continue;
			}
			//Found him, don't copy, target the next guy.
			oix++;
			if (oix == other.num) {
				//Out of things to remove, so just copy over the rest.
				for (ix++; ix < num; ix++) {
					items[dest++] = items[ix];
				}
				num = dest;
				return;
			}
		}
		//Looked through the entire list, start over with our smaller list.
		num = dest;
	}
}

template <typename T>
void list<T>::diff(const list<T> &other) {
	int dest = 0;
	for (int i = 0; i < num; i++) {
		if (!other.has(items[i])) {
			items[dest++] = items[i];
		}
	}
	num = dest;
}

template <typename T>
void list<T>::qsort() {
	qsort(0, num);
}

template <typename T>
void list<T>::qsort(Comparator<T> comp) {
	qsort(comp, 0, num);
}

template <typename T>
void list<T>::qsort(int low, int high) {
	int L = low + 1;
	int H = high;
	if (L >= H) return;
	int middle = (low + high) / 2;
	T pivot = items[middle];
	items[middle] = items[low];
	while (1) {
		while (L < high && items[L] <= pivot) L++;
		while (H-1 > low && pivot <= items[H-1]) H = H-1;
		if (L >= H) break;
		T tmp = items[H-1];
		items[H-1] = items[L];
		items[L] = tmp;
	}
	L = (L+H)/2 - 1;
	items[low] = items[L];
	items[L] = pivot;
	qsort(low, L);
	qsort(L+1, high);
}

// TODO This is basically the same as above, might replace most of the body w/ a macro?
template <typename T>
void list<T>::qsort(Comparator<T> comp, int low, int high) {
	int L = low + 1;
	int H = high;
	if (L >= H) return;
	int middle = (low + high) / 2;
	T pivot = items[middle];
	items[middle] = items[low];
	while (1) {
		while (L < high && (*comp)(items[L], pivot)) L++;
		while (H-1 > low && (*comp)(pivot, items[H-1])) H = H-1;
		if (L >= H) break;
		T tmp = items[H-1];
		items[H-1] = items[L];
		items[L] = tmp;
	}
	L = (L+H)/2 - 1;
	items[low] = items[L];
	items[L] = pivot;
	qsort(comp, low, L);
	qsort(comp, L+1, high);
}

template <typename T>
char list<T>::sorted() const {
	if (!num) return 1;
	T prev = items[0];
	for (int i = 1; i < num; i++) {
		T n = items[i];
		if (prev > n) return 0;
		prev = n;
	}
	return 1;
}

template <typename T>
void list<T>::heap_push(const T &itm) {
	add(itm);
	heap_siftUp(num - 1);
}

template <typename T>
T list<T>::heap_pop() {
	// caller is expected to ensure non-empty heap
	T result = items[0];
	items[0] = items[num - 1];
	num--;
	heap_siftDown(0);
	return result;
}

template <typename T>
void list<T>::heapify() {
	// build heap in-place
	for (int i = (num / 2) - 1; i >= 0; i--) heap_siftDown(i);
}

template <typename T>
void list<T>::heap_siftUp(int ix) {
	while (ix > 0) {
		int parent = (ix - 1) / 2;
		if (!(items[ix] < items[parent])) break;
		T tmp = items[parent];
		items[parent] = items[ix];
		items[ix] = tmp;
		ix = parent;
	}
}

template <typename T>
void list<T>::heap_siftDown(int ix) {
	while (1) {
		int left = 2 * ix + 1;
		if (left >= num) break;
		// Find smallest child
		int small = left;
		int right = left + 1;
		if (right < num && items[right] < items[left]) small = right;
		// Verify smallest child is smaller than us
		if (!(items[small] < items[ix])) break;
		// Switch places and repeat one layer down
		T tmp = items[ix];
		items[ix] = items[small];
		items[small] = tmp;
		ix = small;
	}
}
