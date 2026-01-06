#!/bin/bash

if ! [ -p edit_events.fifo ]; then
	if ! mkfifo edit_events.fifo; then
		echo "Couldn't make named pipe (fifo)!";
		exit;
	fi;
fi;

if ! ( mount | grep dl_tmp ); then
	echo "dl_tmp isn't a mount, are you sure you want to do this?";
	exit;
fi;

inotifywait -m -e CLOSE_WRITE ./src/dl_tmp > edit_events.fifo &
# Probably a better way to do this?
inotify_pid="$(jobs -p)";
echo "inotifywait pid is '$inotify_pid'";

killed=;
ctrl_c() {
	if [ -n "$killed" ]; then
		echo "Exiting dl_watch.sh";
		exit;
	fi;
	# First time probably just killed python3, and
	# there's further cleanup we want to do.
	# Second ^C will actually exit the script.
	killed=x;
}

trap ctrl_c INT

python3 dl_watcher.py < edit_events.fifo;

echo "killing inotifywait process"
kill $inotify_pid;
