#!/bin/bash

# This is the super-naive version
# If you want a really robust way to ensure the working dir matches the script location, look it up!
cd "$(dirname "$0")"

# Converts some data files to hexdump snippets suitable for including in C source
src/http.d/compile.sh

echo 'Compiling...'

shopt -s nullglob

# On my local setup, GLFW3 seems to require -ldl, but doesn't list it in the pkg-config libs?
L_GLFW3="`pkg-config --libs glfw3 libpng openal` -ldl"
if [ 0 -ne $? ]; then exit; fi;

# `rdynamic` exports many symbols, which we need so that
# stuff in shared object files can use our symbols.
# `-ldl` is included again since I also use it myself, separate from GLFW3.
DL_STUFF="-rdynamic -ldl"

g++ -std=c++20 -fdiagnostics-color -Wall -Wshadow -Wno-switch -Wno-format-truncation -Wno-invalid-offsetof -O2 -g $DL_STUFF "$@" \
	src/{,lv/,comp/,tasks/}*.{c,cpp} \
	$L_GLFW3 -pthread -lm -lGL -o game
#src/*.cpp src/*.c src/lv/*.cpp src/lv/*.c \

echo 'Done'
