#!/usr/bin/env bash
# Real native Wayland checks on a private compositor; never use a user's session.
set -euo pipefail
[[ "${GITHUB_ACTIONS:-}" == true ]] || exit 2
app=$(realpath "${1:-build/desktop/desktop/Framoryx}")
report=$(realpath -m "${2:-work/desktop-backends/wayland}")
export XDG_RUNTIME_DIR="$RUNNER_TEMP/framoryx-wayland-runtime"
export XDG_CONFIG_HOME="$RUNNER_TEMP/framoryx-wayland-config"
export XDG_SESSION_TYPE=wayland
export WAYLAND_DISPLAY=framoryx-wayland
export QT_QPA_PLATFORM=wayland
export QT_WAYLAND_DISABLE_WINDOWDECORATION=1
unset DISPLAY
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME" "$report"
chmod 700 "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME"
weston --backend=headless-backend.so --use-gl --width=1920 --height=1080 \
    --socket="$WAYLAND_DISPLAY" --idle-time=0 --no-config \
    --log="$report/compositor.log" > "$report/compositor-output.log" 2>&1 &
compositor=$!
cleanup() { kill "$compositor" 2>/dev/null || true; wait "$compositor" 2>/dev/null || true; }
trap cleanup EXIT
for attempt in {1..100}; do
    [[ ! -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ]] || break
    if ! kill -0 "$compositor" 2>/dev/null; then cat "$report/compositor.log"; exit 1; fi
    sleep .1
done
[[ -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ]] || { cat "$report/compositor.log"; exit 1; }
mode=--ui-smoke
[[ "$app" != *.AppImage ]] || mode=--bundle-test
timeout 45 "$app" "$mode" --test-root "$XDG_CONFIG_HOME/ui" --snapshot-dir "$report/ui"
timeout 120 "$app" --backend-test --backend-scope wayland \
    --test-root "$XDG_CONFIG_HOME/games" --snapshot-dir "$report/games"
