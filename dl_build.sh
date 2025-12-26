#!/bin/bash

g++ -std=c++20 -fdiagnostics-color -Wall -Wshadow -Wno-switch -Wno-format-truncation -O2 -g "$1" \
	-fPIC -shared -o src/dl_tmp/dl_obj.so
