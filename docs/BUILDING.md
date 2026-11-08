# Building and testing

## Windows

Run `source/build.ps1` from PowerShell. Supply `-ToolchainBin` to reuse LLVM-MinGW, `-BuildDirectory` for intermediate files, and `-OutputDirectory` to write binaries to a separate package folder.

The toolchain archive is pinned and SHA-256 verified. PresentMon is fetched separately by `scripts/bootstrap-tools.ps1`; its binary is also pinned and verified. No global toolchain installation or account/group change is made.

```powershell
./scripts/bootstrap-tools.ps1
./source/build.ps1 -BuildDirectory ./work/build
./scripts/package.ps1 -ToolchainBin C:\llvm-mingw\bin
```

The build runs 32 core, 20 gain and 38 microphone/bass configuration checks. It also builds an isolated VST2 audio-compatibility helper for runtime RNNoise downloads. Audio tests use scratch configuration files and do not change system audio.

## Runtime checks

```powershell
# ETW and borderless only; no injection
./source/validate.ps1 -ToolchainBin C:\llvm-mingw\bin -SoakSeconds 120

# Explicit injection into bundled test scenes only
./source/validate.ps1 -ToolchainBin C:\llvm-mingw\bin -IncludeGraphicsTests

# UI/profile/scale/motion checks with isolated preferences
./source/validate.ps1 -ToolchainBin C:\llvm-mingw\bin -UiSmokeSeconds 28
```

The bounded `--smoke-audio-controls` app flag can be combined with `--smoke-seconds 12 --smoke-page 1 --report <scratch.json> --settings-file <scratch.ini>` to exercise the audio/rules controls. It suppresses Windows volume writes, disables automatic downloads and directs gain changes into a scratch APO config. Both report and isolated settings arguments are required. `--snapshot <image.png>`, `--audio-mic` and `--audio-setup` export the app's own client rendering for visual review.

The old injection soak is a separate `-IncludeHookSoak` opt-in and was blocked by Defender on the development host. A blocked or incomplete run is not counted as a pass. See [compatibility](COMPATIBILITY.md).

## Shared-core tests on Linux

```sh
cmake -S . -B build/core -DCMAKE_BUILD_TYPE=Release
cmake --build build/core --parallel
ctest --test-dir build/core --output-on-failure
```

This verifies the portable core. It does not build a Linux desktop app.

## Distribution

`scripts/package.ps1` stages only project source, required runtime files, assets, notices and documentation. Personal settings, captures, local logs, toolchain files and development caches are excluded. ZIP contents receive a SHA-256 manifest. Hosted release creation remains a draft so the files can be reviewed before publication.
