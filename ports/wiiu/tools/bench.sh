#!/usr/bin/env bash
# bench.sh <tag> [RAC1_X=v ...]: one automated console run (wiiload, nothing installed; the settings through the SD's run.cfg, Daino's put back after),
# RAC1_SIM_INPUT=explore in real time with the timing on, its log kept and summarised (the SD's run.cfg swapped for the run, put back).
#
#   ports/wiiu/tools/bench.sh veldin_base RAC1_LEVEL=0
#   ports/wiiu/tools/bench.sh novalis_base RAC1_LEVEL=1 RAC1_FRAMES=1500
#
# Environment: WIIU_IP (default 192.168.0.16), BENCH_DIR (logs and ELFs; default ../rac1-scratch/wiiu/bench), BUILD (build-wiiu),
# WIIU_TIMEOUT (default 400 s).
# The log: [timing] (CPU ms per stage a host frame), [gpu] (GPU ms per pass; RAC1_WIIU_GPU_TIME=2 by default here), [gx2] (GX2DrawDone /
# GX2Invalidate / draws / flushed bytes a host frame, the DrawDone sites), [game] (the game code's stages). The first two windows (the
# level's load) are left out of the summary.
set -u
tag=${1:?usage: bench.sh <tag> [RAC1_X=v ...]}
shift
W=$(cd "$(dirname "$0")/../../.." && pwd)
ip=${WIIU_IP:-192.168.0.16}
bdir=${BUILD:-$W/build-wiiu}
out=${BENCH_DIR:-$W/../rac1-scratch/wiiu/bench}
mkdir -p "$out"
if ! ping -c1 -W2 "$ip" > /dev/null 2>&1; then
    echo "bench: $ip not reachable (console off?)" >&2
    exit 2
fi
cp "$bdir/rac1_wiiu.elf" "$out/$tag.elf"  # (samples.py needs the very build that ran)
log=$out/$tag.udp.log
# (Aroma's wiiload passes no arguments - argc 1 -: the settings go through the SD's run.cfg; Daino's own is kept and always put back)
cfg_url=ftp://$ip/fs/vol/external01/rac1-native/run.cfg
backup=$out/run.cfg.daino
if [ ! -s "$backup" ]; then
    timeout 60 curl -s "$cfg_url" -o "$backup" || { echo "bench: run.cfg not read" >&2; exit 3; }
fi
{ printf '# bench.sh %s\n' "$tag"
  printf '%s\n' RAC1_WIIU_GFX=1 RAC1_BOOT=0 RAC1_MEMCARD=0 RAC1_REALTIME=1 RAC1_SIM_INPUT=explore RAC1_FRAMES=1200 RAC1_WIIU_TIMING=1 \
      RAC1_WIIU_GPU_TIME=2 RAC1_WIIU_WATCHDOG=10 RAC1_WIIU_MENU=1 "$@"; } > "$out/$tag.cfg"
restore() { timeout 60 curl -s -T "$backup" "$cfg_url" && echo "bench: Daino's run.cfg put back" || echo "bench: RESTORE FAILED, copy $backup to the SD" >&2; }
timeout 60 curl -s -T "$out/$tag.cfg" "$cfg_url" || { echo "bench: run.cfg not written" >&2; exit 3; }
WIIU_IP=$ip WIIU_TIMEOUT=${WIIU_TIMEOUT:-400} WIIU_LOG=$log WIIU_EXIT_RE="rac1-wiiu: (exit|back to the menu|operator new)|watchdog 50 s" \
    "$W/ports/wiiu/tools/wiiu-run.sh" "$bdir/rac1_wiiu.rpx" > /dev/null
rc=$?
sleep 20  # (no FTP while the app returns to the menu)
restore
echo "rc=$rc log=$log"
tr -d '\r' < "$log" | grep -aE "exception|abort|watchdog|returned|exit$" | cut -c1-200 | tail -4
avg() { awk '{s+=$1; n++} END {if (n) printf "%.1f (%d windows)", s/n, n; else print "-"}'; }
echo "-- host frame ms: $(tr -d '\r' < "$log" | grep -a '^\[timing\]' | tail -n +3 | sed -E 's/.*frames, ([0-9.]+) ms each.*/\1/' | avg)"
echo "-- GPU busy ms:   $(tr -d '\r' < "$log" | grep -a '^\[gpu\] [0-9]* TV frames' | tail -n +3 | sed -E 's/.*busy ([0-9.]+) ms.*/\1/' | avg)"
echo "-- game code ms:  $(tr -d '\r' < "$log" | grep -a '^\[game\]' | tail -n +3 | sed -E 's/.*code ([0-9.]+) ms.*/\1/' | avg)"
echo "-- last windows:"
for k in timing gpu gpu-world gx2 game; do tr -d '\r' < "$log" | grep -a "^\[$k\]" | tail -1 | cut -c1-900; done
