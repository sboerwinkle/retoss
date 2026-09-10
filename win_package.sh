#!/bin/bash

# You probably don't need this unless you're packaging the Windows build for distribution.
# Even then you might have to make sure some of the paths (like `win_bins/`) make sense.

cd "$(dirname "$0")"

rm retoss_windows.zip
cd ..
zip -r retoss/retoss_windows.zip retoss/assets retoss/shaders retoss/game.exe -x '*.xcf' '*.aup3' 'assets/old/' '*_orig.mp3'
cd retoss/win_bins/
zip -r ../retoss_windows.zip retoss/
