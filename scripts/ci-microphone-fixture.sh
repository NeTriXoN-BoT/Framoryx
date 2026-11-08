#!/usr/bin/env bash
# Source only from the private, synthetic CI audio session.
[[ "${GITHUB_ACTIONS:-}" == true && "$XDG_RUNTIME_DIR" == "$RUNNER_TEMP/"* ]] || exit 2
create_ci_microphone() {
    local config="$XDG_RUNTIME_DIR/microphone-fixture.conf"
    python3 - "$config" <<'PY'
import json, pathlib, sys
modules=[{'name':name} for name in ['libpipewire-module-protocol-native','libpipewire-module-client-node','libpipewire-module-adapter']]
modules.append({'name':'libpipewire-module-loopback','args':{
    'audio.channels':2,'audio.position':['FL','FR'],
    'capture.props':{'node.name':'capture.framoryx_ci_mic','node.target':'framoryx_ci_input','target.object':'framoryx_ci_input','stream.capture.sink':True,'node.passive':True},
    'playback.props':{'node.name':'framoryx_ci_mic','node.description':'Framoryx_CI_Microphone','media.class':'Audio/Source'}}})
pathlib.Path(sys.argv[1]).write_text(json.dumps({'context.spa-libs':{'audio.convert.*':'audioconvert/libspa-audioconvert','support.*':'support/libspa-support'},'context.modules':modules}))
PY
    pipewire -c "$config" > "$XDG_RUNTIME_DIR/microphone-fixture.log" 2>&1 & pids+=("$!")
}
