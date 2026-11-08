#!/usr/bin/env bash
# Developer diagnostic: only an isolated CI session, never a user's audio server.
set -euo pipefail
[[ "${GITHUB_ACTIONS:-}" == true ]] || exit 2
export XDG_RUNTIME_DIR="$RUNNER_TEMP/framoryx-audio-probe"
export XDG_CONFIG_HOME="$RUNNER_TEMP/framoryx-audio-probe-config"
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME" work/pipewire-diagnostics
chmod 700 "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME"
pids=()
trap 'for pid in "${pids[@]}"; do kill "$pid" 2>/dev/null || true; done' EXIT
command -v pipewire
pipewire --version
env | sort | sed -n '/^PIPEWIRE_/p'
PIPEWIRE_DEBUG=3 pipewire > work/pipewire-diagnostics/server.log 2>&1 & pids+=("$!")
wireplumber > work/pipewire-diagnostics/session.log 2>&1 & pids+=("$!")
sleep 2
python3 - "$1" <<'PY'
import json, pathlib, sys
config=json.loads(pathlib.Path(sys.argv[1]).read_text())
root=pathlib.Path('work/pipewire-diagnostics')
for name, wrapped in [('json',True),('spa',False)]:
    text=json.dumps(config, indent=2)
    if not wrapped: text=text[text.index('{')+1:text.rindex('}')]
    (root/(name+'.conf')).write_text(text)
PY
for variant in json spa; do
    PIPEWIRE_DEBUG=4 pipewire -c "$PWD/work/pipewire-diagnostics/$variant.conf" > "work/pipewire-diagnostics/$variant.log" 2>&1 & probe=$!;pids+=("$probe")
    sleep 2
    ps -p "$probe" -o pid,stat,args || true
    pw-dump > "work/pipewire-diagnostics/$variant-nodes.json"
    kill "$probe" 2>/dev/null || true
    wait "$probe" || true
    tail -n 45 "work/pipewire-diagnostics/$variant.log"
done
