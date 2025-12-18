#!/bin/bash

inotifywait -m -e CLOSE_WRITE ./src/dl_tmp | python3 dl_watcher.py
