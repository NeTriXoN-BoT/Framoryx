# Runtime libraries and corresponding source

Framoryx uses shared runtime libraries. Their original licenses and notices are retained in `licenses` and, for the Linux game layer, `runtime/mangohud/licenses`. The application's MIT license does not replace those licenses. Compatible library builds can be substituted; Framoryx imposes no additional restriction on debugging modifications to the LGPL components.

## Qt 6.8.3

The desktop dynamically links Qt Core, Gui, Widgets, Network, Qml, Quick, Quick Controls, Concurrent and Multimedia. Qt is available under the LGPL-3.0/GPL terms described in its source and the included license texts. Unmodified corresponding source, third-party notices and build instructions are available directly from Qt:

- [Qt Base source](https://download.qt.io/archive/qt/6.8/6.8.3/submodules/qtbase-everywhere-src-6.8.3.tar.xz)
- [Qt Declarative source](https://download.qt.io/archive/qt/6.8/6.8.3/submodules/qtdeclarative-everywhere-src-6.8.3.tar.xz)
- [Qt Multimedia source](https://download.qt.io/archive/qt/6.8/6.8.3/submodules/qtmultimedia-everywhere-src-6.8.3.tar.xz)
- [All Qt 6.8.3 source modules](https://download.qt.io/archive/qt/6.8/6.8.3/submodules/)

The runtime comes from the official Qt 6.8.3 distribution. Framoryx does not modify these Qt libraries. Application rebuild instructions are in [BUILDING.md](BUILDING.md). Each Qt source archive includes its `LICENSES` and third-party attribution files.

## FFmpeg

Qt Multimedia's official 6.8.3 distribution uses FFmpeg n7.1 under LGPL-2.1-or-later and its included permissive component licenses. Qt's prebuilt distribution omits FFmpeg's optional GPL-only components. Framoryx does not modify FFmpeg.

- [Corresponding FFmpeg n7.1 source](https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n7.1.tar.gz)
- [Qt's FFmpeg component notices](https://github.com/qt/qtmultimedia/blob/v6.8.3/src/3rdparty/ffmpeg/qt_attribution.json)
- [Qt 6.8.3 build provisioning](https://code.qt.io/cgit/qt/qt5.git/tree/coin/provisioning?h=v6.8.3)

Original LGPL and component license texts are in `licenses/FFmpeg-*`.

## Linux game layer

MangoHud is MIT licensed. Its upstream revision, local patch, source retrieval and build procedure are in [runtime/mangohud](../runtime/mangohud/README.md). Dependency archives carry pinned hashes. The runtime includes the original licenses of MangoHud and its bundled dependencies.

## Optional microphone plugin

RNNoise's `noise-suppression-for-voice` plugin is downloaded separately from [the official project](https://github.com/werman/noise-suppression-for-voice). Its GPL-3.0 license and corresponding source are provided upstream with each release. Windows Equalizer APO or the Linux PipeWire process hosts that plugin. It is not embedded in Framoryx's executable.

## Windows runtime

Application-local Microsoft Visual C++ runtime DLLs come from Visual Studio's redistributable directory. System32 DLLs and compiler tools are not copied. These files retain Microsoft's terms; Windows system components remain supplied by the operating system.
