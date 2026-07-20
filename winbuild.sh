#!/bin/bash

# This is the super-naive version
# If you want a really robust way to ensure the working dir matches the script location, look it up!
cd "$(dirname "$0")"

echo 'Compiling...'

shopt -s nullglob

# win_libs/libpng16.a \
# png_objs/*.o \
x86_64-w64-mingw32-g++ -std=c++20 \
	src/{,lv/,comp/,tasks/}*.{c,cpp} \
	-I./win_includes/ -I./includes/ -L./win_libs/ \
	-lws2_32 -lpng16 -lz -lglfw3 -lOpenAL32 -lsndfile -pthread \
	-o game \
\
&& echo 'Done'
