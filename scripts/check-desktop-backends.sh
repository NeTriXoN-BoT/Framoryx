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
retry() { for attempt in {1..50}; do if "$@"; then return 0; fi; sleep .1; done; return 1; }
openbox > "$XDG_RUNTIME_DIR/window-manager.log" 2>&1 & pids+=("$!")
xcompmgr > "$XDG_RUNTIME_DIR/compositor.log" 2>&1 & pids+=("$!")
pipewire > "$XDG_RUNTIME_DIR/pipewire.log" 2>&1 & pids+=("$!")
wireplumber > "$XDG_RUNTIME_DIR/wireplumber.log" 2>&1 & pids+=("$!")
pipewire-pulse > "$XDG_RUNTIME_DIR/pulse.log" 2>&1 & pids+=("$!")
retry pactl info >/dev/null
retry pactl load-module module-null-sink sink_name=framoryx_ci_input sink_properties=device.description=Framoryx_CI_Input >/dev/null
retry pactl load-module module-null-sink sink_name=framoryx_ci_output sink_properties=device.description=Framoryx_CI_Output >/dev/null
retry pactl load-module module-remap-source master=framoryx_ci_input.monitor source_name=framoryx_ci_mic source_properties=device.description=Framoryx_CI_Microphone >/dev/null
retry pactl set-default-sink framoryx_ci_output
retry pactl set-default-source framoryx_ci_mic
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
retry pactl set-source-volume framoryx_ci_mic 100%
retry pactl set-source-mute framoryx_ci_mic false
timeout 2 parec --device=framoryx_ci_mic --format=s16le --rate=48000 --channels=1 > "$XDG_RUNTIME_DIR/dry-input.raw" || [[ "$?" == 124 ]]
python3 - "$XDG_RUNTIME_DIR/dry-input.raw" <<'PY'
import array, pathlib, sys
samples=array.array('h');samples.frombytes(pathlib.Path(sys.argv[1]).read_bytes())
peak=max((abs(sample) for sample in samples),default=0)
assert peak>0, 'The private test microphone must contain the synthesized signal'
print('PASS: private microphone signal is nonzero; peak='+str(peak))
PY
result=0
./build/desktop/desktop/Framoryx --backend-test --backend-scope graphics --test-root "$RUNNER_TEMP/framoryx-backend-graphics" --snapshot-dir ./work/desktop-backends/graphics || result=1
./build/desktop/desktop/Framoryx --backend-test --backend-scope audio --test-root "$RUNNER_TEMP/framoryx-backend-audio" --snapshot-dir ./work/desktop-backends/audio || result=1
if [[ "$result" != 0 ]]; then
    pactl list sources short
    pw-dump | python3 -c 'import json,sys; nodes=json.load(sys.stdin); print(json.dumps([{"name":x.get("info",{}).get("props",{}).get("node.name"),"media":x.get("info",{}).get("props",{}).get("media.class"),"state":x.get("info",{}).get("state")} for x in nodes if x.get("type")=="PipeWire:Interface:Node"],indent=2))'
    if [[ -f "$RUNNER_TEMP/framoryx-backend-audio/output.conf" ]]; then
        pipewire -v -c "$RUNNER_TEMP/framoryx-backend-audio/output.conf" > work/desktop-backends/pipewire-probe.log 2>&1 & probe=$!;pids+=("$probe")
        sleep 1
        pw-dump | python3 -c 'import json,sys; print(json.dumps([{k:x.get("info",{}).get("props",{}).get(k) for k in ["node.name","media.class"]} for x in json.load(sys.stdin) if x.get("type")=="PipeWire:Interface:Node"],indent=2))'
        kill "$probe" 2>/dev/null || true
        tail -n 40 work/desktop-backends/pipewire-probe.log
    fi
fi
python3 scripts/check-graphics-apis.py --bin ./build/desktop/desktop --output ./work/desktop-backends || result=1
exit "$result"
