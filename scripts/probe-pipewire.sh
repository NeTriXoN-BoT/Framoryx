#!/usr/bin/env bash
# Developer diagnostic: only an isolated CI session, never a user's audio server.
set -euo pipefail
[[ "${GITHUB_ACTIONS:-}" == true ]] || exit 2
export XDG_RUNTIME_DIR="$RUNNER_TEMP/framoryx-audio-probe"
export XDG_CONFIG_HOME="$RUNNER_TEMP/framoryx-audio-probe-config"
export PULSE_SERVER="unix:$XDG_RUNTIME_DIR/pulse/native"
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME" work/pipewire-diagnostics
chmod 700 "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME"
pids=()
trap 'for pid in "${pids[@]}"; do kill "$pid" 2>/dev/null || true; done' EXIT
command -v pipewire
pipewire --version
env | sort | sed -n '/^PIPEWIRE_/p'
PIPEWIRE_DEBUG=3 pipewire > work/pipewire-diagnostics/server.log 2>&1 & pids+=("$!")
wireplumber > work/pipewire-diagnostics/session.log 2>&1 & pids+=("$!")
pipewire-pulse > work/pipewire-diagnostics/pulse.log 2>&1 & pids+=("$!")
sleep 2
retry() { for attempt in {1..40}; do if "$@"; then return 0; fi; sleep .1; done; return 1; }
retry pactl info >/dev/null
retry pactl load-module module-null-sink sink_name=framoryx_ci_input sink_properties=device.description=Framoryx_CI_Input >/dev/null
retry pactl load-module module-null-sink sink_name=framoryx_ci_output sink_properties=device.description=Framoryx_CI_Output >/dev/null
retry pactl load-module module-remap-source master=framoryx_ci_input.monitor source_name=framoryx_ci_mic source_properties=device.description=Framoryx_CI_Microphone >/dev/null
python3 - "$XDG_RUNTIME_DIR/tone.wav" <<'PY'
import array, math, sys, wave
samples=array.array('h',(int(6500*math.sin(2*math.pi*180*i/48000)) for i in range(48000)))
with wave.open(sys.argv[1],'wb') as output:
    output.setparams((1,2,48000,0,'NONE','not compressed'))
    for _ in range(60): output.writeframes(samples.tobytes())
PY
paplay --device=framoryx_ci_input "$XDG_RUNTIME_DIR/tone.wav" > work/pipewire-diagnostics/tone.log 2>&1 & tone=$!;pids+=("$tone")
sleep 1
for device in framoryx_ci_input.monitor framoryx_ci_mic; do
    pactl set-source-mute "$device" false
    pactl set-source-volume "$device" 100%
    timeout 2 parec --device="$device" --format=s16le --rate=48000 --channels=1 > "$XDG_RUNTIME_DIR/$device.raw" || [[ "$?" == 124 ]]
    python3 - "$XDG_RUNTIME_DIR/$device.raw" <<'PY'
import array,pathlib,sys
samples=array.array('h');samples.frombytes(pathlib.Path(sys.argv[1]).read_bytes());print(pathlib.Path(sys.argv[1]).name,'samples',len(samples),'peak',max(map(abs,samples),default=0))
PY
done
ps -p "$tone" -o pid,stat,args || true
pactl list sink-inputs > work/pipewire-diagnostics/sink-inputs.log
pw-dump > work/pipewire-diagnostics/fixture-nodes.json
python3 - "$1" <<'PY'
import json, pathlib, sys
config=json.loads(pathlib.Path(sys.argv[1]).read_text())
root=pathlib.Path('work/pipewire-diagnostics')
for module in config['context.modules']:
    if module['name']=='libpipewire-module-filter-chain':
        graph=module['args']['filter.graph']
        if isinstance(graph,str): graph=json.loads(graph)
        module['args']['filter.graph']=json.dumps({key:graph[key] for key in ['nodes','links','inputs','outputs']})
for name, wrapped in [('ordered-json',True),('ordered-spa',False)]:
    text=json.dumps(config, indent=2)
    if not wrapped: text=text[text.index('{')+1:text.rindex('}')]
    (root/(name+'.conf')).write_text(text)
PY
for variant in ordered-json ordered-spa; do
    PIPEWIRE_DEBUG=4 pipewire -c "$PWD/work/pipewire-diagnostics/$variant.conf" > "work/pipewire-diagnostics/$variant.log" 2>&1 & probe=$!;pids+=("$probe")
    sleep 2
    ps -p "$probe" -o pid,stat,args || true
    timeout 5 pw-dump > "work/pipewire-diagnostics/$variant-nodes.json" || true
    kill "$probe" 2>/dev/null || true
    sleep .2
    kill -KILL "$probe" 2>/dev/null || true
    wait "$probe" || true
    tail -n 45 "work/pipewire-diagnostics/$variant.log"
    python3 - "work/pipewire-diagnostics/$variant-nodes.json" <<'PY'
import json, pathlib, sys
nodes=json.loads(pathlib.Path(sys.argv[1]).read_text())
names={node.get('info',{}).get('props',{}).get('node.name') for node in nodes}
assert {'framoryx_output','playback.framoryx_output'} <= names, 'The filter graph did not publish its output nodes'
print('PASS: ordered filter graph publishes both output nodes')
PY
done
