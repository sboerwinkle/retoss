#!/bin/bash

if ( mount | grep dl_tmp ); then
	echo "Looks like there's an existing mount, aborting";
	exit;
fi;

if [ -n "$(ls -A ./src/dl_tmp)" ]; then
	echo "dl_tmp not empty, aborting";
	exit;
fi;

doas mount -t tmpfs -o size=10m,noswap tmpfs ./src/dl_tmp
echo "done"
