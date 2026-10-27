#!/usr/bin/env bash
# Sends an .rpx/.wuhb to the Wii U (Aroma's wiiload plugin), captures the app's
# UDP log (port 4405) and waits for the app to say it is going back to the menu.
#
#   WIIU_IP=192.168.x.y ports/wiiu/tools/wiiu-run.sh app.rpx [app args...]
#
# Environment:
#   WIIU_IP        the console's address (required)
#   WIIU_TIMEOUT   seconds to wait for the exit line (default 60)
#   WIIU_UPLOAD_TIMEOUT  seconds the upload may take (default 300: about 1 MB/s)
#   WIIU_EXIT_RE   extended regex marking the app's exit (default "auto-exit|: exit")
#   WIIU_LOG       where to write the UDP log (default: next to the app, .udp.log)
#
# Exit codes: 0 app ran and exited, 2 console not reachable, 3 upload failed,
# 4 no exit line within the timeout (the console is probably hung: power-cycle it).
# It never sends twice, and never opens a connection to wiiload's port other than the upload's.
set -u

app=${1:?usage: wiiu-run.sh app.rpx [args...]}
shift
ip=${WIIU_IP:?set WIIU_IP to the console address}
timeout_s=${WIIU_TIMEOUT:-60}
upload_s=${WIIU_UPLOAD_TIMEOUT:-300}
exit_re=${WIIU_EXIT_RE:-auto-exit|: exit}
log=${WIIU_LOG:-${app%.*}.udp.log}
wiiload=${DEVKITPRO:-/opt/devkitpro}/tools/bin/wiiload

# (no probe of port 4299: the plugin serves one connection at a time and would wait on the probe's for a file
# that never comes, with the real upload queued behind it; FTP's port says whether the console is up)
if ! timeout 5 bash -c "exec 3<>/dev/tcp/$ip/21" 2>/dev/null; then
    echo "wiiu-run: $ip not reachable (console off?)" >&2
    exit 2
fi

: > "$log"
socat -u UDP-RECV:4405,reuseaddr - >> "$log" 2>/dev/null &
listener=$!
trap 'kill $listener 2>/dev/null' EXIT
sleep 0.5

if ! WIILOAD=tcp:$ip timeout "$upload_s" "$wiiload" "$app" "$@" > /dev/null 2>&1; then
    echo "wiiu-run: upload failed or timed out" >&2
    exit 3
fi
echo "wiiu-run: sent $(basename "$app"), waiting up to ${timeout_s}s for the exit line"

deadline=$((SECONDS + timeout_s))
until grep -aqE "$exit_re" "$log"; do
    if (( SECONDS >= deadline )); then
        echo "wiiu-run: no exit line after ${timeout_s}s; log so far:" >&2
        tr -d '\r' < "$log" >&2
        exit 4
    fi
    sleep 1
done
sleep 2  # the last lines after the exit marker
tr -d '\r' < "$log"
echo "wiiu-run: ok (log in $log)"
