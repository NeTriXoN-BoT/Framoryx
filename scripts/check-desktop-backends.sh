#!/usr/bin/env bash
# Isolated CI desktop/audio session. Never run against a user's session.
set -euo pipefail
[[ "${GITHUB_ACTIONS:-}" == true ]] || exit 2
export XDG_RUNTIME_DIR="$RUNNER_TEMP/framoryx-private-runtime"
export XDG_CONFIG_HOME="$RUNNER_TEMP/framoryx-private-config"
export XDG_SESSION_TYPE=x11
export PULSE_SERVER="unix:$XDG_RUNTIME_DIR/pulse/native"
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME" work/desktop-backends
chmod 700 "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME"
pids=()
cleanup() { for pid in "${pids[@]}"; do kill "$pid" 2>/dev/null || true; done; }
trap cleanup EXIT
openbox > "$XDG_RUNTIME_DIR/window-manager.log" 2>&1 & pids+=("$!")
xcompmgr > "$XDG_RUNTIME_DIR/compositor.log" 2>&1 & pids+=("$!")
pipewire > "$XDG_RUNTIME_DIR/pipewire.log" 2>&1 & pids+=("$!")
wireplumber > "$XDG_RUNTIME_DIR/wireplumber.log" 2>&1 & pids+=("$!")
pipewire-pulse > "$XDG_RUNTIME_DIR/pulse.log" 2>&1 & pids+=("$!")
for attempt in {1..50}; do if pactl info >/dev/null 2>&1; then break; fi; sleep .1; done
pactl load-module module-null-sink sink_name=framoryx_ci_input sink_properties=device.description=Framoryx_CI_Input >/dev/null
pactl load-module module-null-sink sink_name=framoryx_ci_output sink_properties=device.description=Framoryx_CI_Output >/dev/null
pactl load-module module-remap-source master=framoryx_ci_input.monitor source_name=framoryx_ci_mic source_properties=device.description=Framoryx_CI_Microphone >/dev/null
pactl set-default-sink framoryx_ci_output
pactl set-default-source framoryx_ci_mic
python3 - "$XDG_RUNTIME_DIR/tone.wav" <<'PY'
import array, math, sys, wave
samples = array.array('h', (int(6500 * math.sin(2*math.pi*180*i/48000)) for i in range(48000)))
if sys.byteorder != 'little': samples.byteswap()
with wave.open(sys.argv[1], 'wb') as output:
    output.setparams((1, 2, 48000, 0, 'NONE', 'not compressed'))
    for _ in range(230): output.writeframes(samples.tobytes())
PY
paplay --device=framoryx_ci_input "$XDG_RUNTIME_DIR/tone.wav" > "$XDG_RUNTIME_DIR/tone.log" 2>&1 & pids+=("$!")
sleep 1
result=0
./build/desktop/desktop/Framoryx --backend-test --test-root "$RUNNER_TEMP/framoryx-backend" --snapshot-dir ./work/desktop-backends || result=1
python3 scripts/check-graphics-apis.py --bin ./build/desktop/desktop --output ./work/desktop-backends || result=1
exit "$result"
