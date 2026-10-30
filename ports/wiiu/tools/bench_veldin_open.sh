#!/usr/bin/env bash
# bench_veldin_open.sh <tag> [RAC1_X=v ...]: Veldin's heavy view - after the arrival's cutscene (game frame ~780) the camera turned
# left toward the open area (the towers, the rocks, the vehicles) and Ratchet walking into it; the explore input turns right,
# toward the light side, and flattered the numbers (33 ms there against ~50 ms here, 2026-10-30). 2100 frames, shots at 1200 / 1800.
exec "$(dirname "$0")/bench.sh" "$1" RAC1_LEVEL=0 RAC1_FRAMES=2100 'RAC1_SIM_INPUT=seq:780-815:rx=-1;815-2100:ly=-0.5' \
    RAC1_WIIU_SHOT=1200,1800 "${@:2}"
