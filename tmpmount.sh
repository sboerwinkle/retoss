#!/bin/bash

mkdir ./src/dl_tmp || exit
doas mount -t tmpfs -o size=10m,noswap tmpfs ./src/dl_tmp
