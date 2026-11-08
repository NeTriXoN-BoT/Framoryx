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
if ! git -C "$source" apply --unidiff-zero --reverse --check "$project/runtime/mangohud/framoryx.patch" 2>/dev/null; then
    git -C "$source" apply --unidiff-zero --check "$project/runtime/mangohud/framoryx.patch"
    git -C "$source" apply --unidiff-zero "$project/runtime/mangohud/framoryx.patch"
fi
cmp -s runtime/mangohud/framoryx_stream.hpp "$source/src/framoryx_stream.hpp" || cp runtime/mangohud/framoryx_stream.hpp "$source/src/framoryx_stream.hpp"
cmp -s runtime/mangohud/framoryx_hud.hpp "$source/src/framoryx_hud.hpp" || cp runtime/mangohud/framoryx_hud.hpp "$source/src/framoryx_hud.hpp"
cmp -s source/presentation.hpp "$source/src/presentation.hpp" || cp source/presentation.hpp "$source/src/presentation.hpp"
cp runtime/mangohud/imgui.wrap "$source/subprojects/imgui.wrap"
meson subprojects download --sourcedir "$source" imgui
for file in imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp imgui.h imgui_internal.h imconfig.h imstb_rectpack.h imstb_textedit.h imstb_truetype.h; do
    cmp -s "source/vendor/imgui/$file" "$source/subprojects/imgui-1.91.9b/$file" || cp "source/vendor/imgui/$file" "$source/subprojects/imgui-1.91.9b/$file"
done
cp source/scalar_formats.hpp "$source/scalar_formats.hpp"
sed -i "s/version: '1.91.6'/version: '1.91.9b'/" "$source/subprojects/imgui-1.91.9b/meson.build"
reconfigure=(); if [[ -f "$build/build.ninja" ]]; then reconfigure=(--reconfigure); fi
meson setup "${reconfigure[@]}" "$build" "$source" --buildtype release --prefix /usr --libdir lib -Dwith_xnvctrl=disabled -Dwith_dbus=disabled -Dwith_wayland=enabled -Dmangoplot=disabled -Dtests=disabled -Dinclude_doc=false
python3 runtime/mangohud/harden-dependencies.py "$source"
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
