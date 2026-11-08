#!/usr/bin/env bash
set -euo pipefail
root=$(pwd)
destination="$root/dist/desktop"
appdir="$destination/Framoryx.AppDir"
tools="$root/work/linux-packaging-tools"
[[ ! -e "$appdir" ]] || { echo 'Package destination must be fresh.' >&2; exit 1; }
mkdir -p "$appdir" "$tools"
fetch() {
    local name="$1" url="$2" sha="$3"
    curl --fail --location --retry 3 --proto '=https' --tlsv1.2 "$url" -o "$tools/$name"
    printf '%s  %s\n' "$sha" "$tools/$name" | sha256sum --check --status
    chmod 755 "$tools/$name"
}
fetch linuxdeploy-x86_64.AppImage https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage df908b19f1ed1cdafa79508d415aa12d6c56ce4d56d4149aa8c09286718706e5
fetch linuxdeploy-plugin-qt-x86_64.AppImage https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage cfc1055b2b9dbc08412b579f20990b7b41a17b61beaa5847dc9477c96c9e9617
fetch appimagetool-x86_64.AppImage https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage e8c1169c4af66c3184d7ee110713d4be04befe671fa358e2c1591ee840553b03
cmake --install build/desktop --prefix "$appdir/usr"
cp -a build/desktop/desktop/runtime "$appdir/usr/bin/runtime"
mkdir -p "$appdir/usr/share/doc/framoryx"
cp -a LICENSE THIRD-PARTY-NOTICES.txt VERSION docs licenses "$appdir/usr/share/doc/framoryx/"
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="$QT_ROOT_DIR/bin/qmake"
export QML_SOURCES_PATHS="$root/desktop/qml"
export EXTRA_PLATFORM_PLUGINS='libqwayland-egl.so;libqwayland-generic.so'
export PATH="$tools:$PATH"
# The Qt deployer finds Wayland's platform, shell and decoration plugins but
# omits its dynamically loaded EGL client integration. Keep these plugins in
# their lookup directory and deploy their dependencies/rpaths explicitly.
qt_plugins=$("$QMAKE" -query QT_INSTALL_PLUGINS)
wayland_client="$qt_plugins/wayland-graphics-integration-client"
[[ -f "$wayland_client/libqt-plugin-wayland-egl.so" ]] || { echo 'Qt Wayland EGL client integration is required.' >&2; exit 1; }
mkdir -p "$appdir/usr/plugins/wayland-graphics-integration-client"
for plugin in "$wayland_client/"*.so; do cp "$plugin" "$appdir/usr/plugins/wayland-graphics-integration-client/"; done
arguments=(--appdir "$appdir" --executable "$appdir/usr/bin/Framoryx" --desktop-file "$appdir/usr/share/applications/io.framoryx.Framoryx.desktop" --icon-file "$appdir/usr/share/icons/hicolor/256x256/apps/io.framoryx.Framoryx.png" --plugin qt)
arguments+=(--deploy-deps-only "$appdir/usr/plugins/wayland-graphics-integration-client")
for library in "$appdir/usr/bin/runtime/mangohud/lib/"*.so; do arguments+=(--library "$library"); done
"$tools/linuxdeploy-x86_64.AppImage" "${arguments[@]}"
[[ -f "$appdir/usr/plugins/wayland-graphics-integration-client/libqt-plugin-wayland-egl.so" ]] || exit 1
cat > "$appdir/AppRun.framoryx" <<'SH'
#!/bin/sh
set -eu
APPDIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
export APPDIR
export FRAMORYX_ORIGINAL_LD_LIBRARY_PATH="${LD_LIBRARY_PATH-}"
export LD_LIBRARY_PATH="$APPDIR/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$APPDIR/usr/bin/Framoryx" "$@"
SH
mv -fT -- "$appdir/AppRun.framoryx" "$appdir/AppRun"
chmod 755 "$appdir/AppRun"
if [[ ! -e "$appdir/io.framoryx.Framoryx.png" ]]; then cp "$appdir/usr/share/icons/hicolor/256x256/apps/io.framoryx.Framoryx.png" "$appdir/io.framoryx.Framoryx.png"; fi
(cd "$appdir" && find . -type f ! -name SHA256SUMS.txt -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS.txt)
ARCH=x86_64 "$tools/appimagetool-x86_64.AppImage" "$appdir" "$destination/Framoryx-Linux-x86_64.AppImage"
tar -C "$destination" -czf "$destination/Framoryx-Linux-x86_64.tar.gz" Framoryx.AppDir
(cd "$destination" && sha256sum Framoryx-Linux-x86_64.AppImage > Framoryx-Linux-x86_64.AppImage.sha256 && sha256sum Framoryx-Linux-x86_64.tar.gz > Framoryx-Linux-x86_64.tar.gz.sha256)
