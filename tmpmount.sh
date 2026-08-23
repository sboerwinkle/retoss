#!/bin/bash

if findmnt ./src/dl_tmp > /dev/null; then
	echo "Looks like there's an existing mount, aborting";
	exit;
fi;

if ! [ -d ./src/dl_tmp ]; then
	echo '`src/dl_tmp` is not a directory, create it first!';
	# We could do this ourselves, but this script is meant to
	# be run as root, so the permissions would be all wacky.
	exit;
fi;

if mount -t tmpfs -o size=10m,noswap tmpfs ./src/dl_tmp; then
	echo "tmpfs filesystem mounted."
fi;
