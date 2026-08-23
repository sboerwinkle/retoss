#!/bin/bash

if ! [ -p edit_events.fifo ]; then
	if ! mkfifo edit_events.fifo; then
		echo "Couldn't make named pipe (fifo)!";
		exit;
	fi;
fi;

if ! [ -d src/dl_tmp ]; then
	mkdir src/dl_tmp;
	echo '`src/dl_tmp/` created.'
fi;

# If directory is empty but not currently mounted,
# advise user to maybe mount it.
if [ -z "$(ls -A ./src/dl_tmp)" ] && ! findmnt ./src/dl_tmp >/dev/null; then
	echo '
During editing, roughly 50KB will be written to disk per
item added or removed. Optionally, you can mount a `tmpfs`
filesystem so everything in `src/dl_tmp/` is in RAM (instead
of on disk), avoiding this cost.

If you would like to do this, exit this script (Ctrl+C), run
`tmpmount.sh` as root, and re-run this script. The resulting
filesystem will have a 10MB max size and will persist until
the system reboots or you manually `umount` it.

Alternatively, press Enter to skip this.';
	read
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
