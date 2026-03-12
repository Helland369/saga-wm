#!/bin/bash
set -e

disp=":80"

Xephyr "$disp" -ac -screen 1280x720 -host-cursor &
XEPHYR_PID=$!

sleep 1
env -u WAYLAND_DISPLAY DISPLAY=$disp ./src/sagawm

kill "$XEPHYR_PID"
