#!/usr/bin/env bash
set -euo pipefail
project=$(pwd)
source="$project/work/mangohud-source"
build="$project/work/mangohud-build"
destination=$(realpath -m "${1:-build/desktop/desktop/runtime/mangohud}")
revision=992103e4fb744897826de04ea00a2f71e7018214
if [[ ! -d "$source/.git" ]]; then git clone --no-checkout https://github.com/flightlessmango/MangoHud.git "$source"; fi
git -C "$source" fetch --depth 1 origin "$revision"
git -C "$source" checkout --detach "$revision"
[[ "$(git -C "$source" rev-parse HEAD)" == "$revision" ]] || exit 1
if ! git -C "$source" apply --reverse --check "$project/runtime/mangohud/framoryx.patch" 2>/dev/null; then
    git -C "$source" apply --check "$project/runtime/mangohud/framoryx.patch"
    git -C "$source" apply "$project/runtime/mangohud/framoryx.patch"
fi
cp runtime/mangohud/framoryx_stream.hpp "$source/src/framoryx_stream.hpp"
meson setup "$build" "$source" --buildtype release --prefix /usr --libdir lib -Dwith_xnvctrl=disabled -Dwith_dbus=disabled -Dwith_wayland=enabled -Dmangoplot=disabled -Dtests=disabled -Dinclude_doc=false
meson compile -C "$build" -j 3
mkdir -p "$destination/lib" "$destination/bin" "$destination/layers" "$destination/licenses"
find "$build/src" -maxdepth 1 -name 'libMangoHud*.so' -type f -exec cp {} "$destination/lib/" \;
cp runtime/mangohud/mangohud "$destination/bin/mangohud"
chmod 755 "$destination/bin/mangohud"
python3 - "$build/src/MangoHud.x86_64.json" "$destination/layers/MangoHud.json" <<'PY'
import json, pathlib, sys
data=json.loads(pathlib.Path(sys.argv[1]).read_text())
data['layer']['library_path']='../lib/libMangoHud.so'
pathlib.Path(sys.argv[2]).write_text(json.dumps(data, indent=2)+'\n')
PY
find "$source" -maxdepth 1 -iname '*license*' -type f -exec cp {} "$destination/licenses/" \;
while IFS= read -r -d '' notice; do
    relative="${notice#"$source/"}"
    mkdir -p "$destination/licenses/$(dirname "$relative")"
    cp "$notice" "$destination/licenses/$relative"
done < <(find "$source/subprojects" -iname '*license*' -type f -print0)
printf '%s\n' "$revision" > "$destination/SOURCE-REVISION"
