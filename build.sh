#!/bin/bash

# This is the super-naive version
# If you want a really robust way to ensure the working dir matches the script location, look it up!
cd `dirname "$0"`

shopt -s nullglob

# On my local setup, GLFW3 seems to require -ldl, but doesn't list it in the pkg-config libs?
L_GLFW3="`pkg-config --libs glfw3` -ldl"

g++ -fdiagnostics-color -Wall -Wno-switch -Wno-format-truncation -O2 -g "$@" \
	src/*.cpp src/*.c \
	$L_GLFW3 -pthread -lm -lGL -o game
