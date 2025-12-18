#!/bin/bash

# This is the super-naive version
# If you want a really robust way to ensure the working dir matches the script location, look it up!
cd `dirname "$0"`

shopt -s nullglob

# On my local setup, GLFW3 seems to require -ldl, but doesn't list it in the pkg-config libs?
L_GLFW3="`pkg-config --libs glfw3 libpng` -ldl"

# `rdynamic` exports many symbols, which we need so that
# stuff in shared object files can use our symbols.
# `-ldl` is included again since I also use it myself, separate from GLFW3.
DL_STUFF="-rdynamic -ldl"

g++ -std=c++20 -fdiagnostics-color -Wall -Wshadow -Wno-switch -Wno-format-truncation -O2 -g $DL_STUFF "$@" \
	src/*.cpp src/*.c \
	$L_GLFW3 -pthread -lm -lGL -o game
