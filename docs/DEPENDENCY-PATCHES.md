# Dependency hardening in 0.2.2

These are small, maintained changes to the pinned dependencies. Their original licenses and attribution are retained. CodeQL continues to analyze the real Windows application, x64/x86 hooks, helpers, tests and dependency code with the security-extended suite. No alert exclusions, query removals or bulk dismissals were added.

| Previous alerts | Component | Source change |
| --- | --- | --- |
| 2 | MinHook block allocator | Reserve the first slot using an explicit byte offset, with a compile-time check that the block header fits. Allocation addresses and slot size are unchanged. |
| 1 | ImGui triangulator | Partition raw scratch storage at a byte offset, then advance through the correctly typed pointer array. |
| 3 | ImGui/STB font allocation | Widen operands before multiplication; check dimensions against downstream integer indexing limits; handle failed allocations and free partial allocations. |
| 1 | ImGui slider math | Convert the operands to the template's floating-point precision before multiplication. |
| 10 | ImGui formatting helpers | Choose numeric formats from typed literal pools. Render localization and prefix text as data, with fixed format strings. Match promoted numeric argument types to the selected conversion. |
| 2 | Clang CET intrinsics | Initialize the fallback register operand to zero, matching `_get_ssp()`. The old uninitialized-warning pragmas are removed from these two helpers. |
| 1 | libc++ stream synchronization | Widen the encoding width to the stream offset type before multiplying by the buffered character count. This also covers 32-bit builds. |

## Numeric format profile

Framoryx uses ImGui for a read-only in-game HUD. Its internal scalar-widget dependency profile accepts integer decimal/hex/octal formats, common padding/sign forms and floating-point precision forms from `source/scalar_formats.hpp`. The actual format pointer passed to the formatting engine always comes from those literals. `%n`, string/pointer conversions, argument-width mismatches, positional/dynamic arguments and formats outside the profile fall back to the type's default numeric format.

This deliberately narrows the general-purpose ImGui scalar formatting API in this fork. Arbitrary custom widget format strings, including custom suffixes, are not supported. Framoryx's overlay customization controls do not expose that API, and the HUD's existing readouts/styles do not depend on it. When adding new scalar widgets, extend the typed literal pool and tests rather than forwarding raw format strings.

Localized URL tooltips replace the first `%s` placeholder as text; other percent sequences are displayed as data. The URL and localization cannot add printf instructions. `ImGui::Value` formats its number separately and uses a fixed literal to combine it with the prefix.

## Portable toolchain headers

`source/toolchain-fixes.ps1` runs before compilation for the pinned LLVM-MinGW 20261104 toolchain. It verifies complete input and output SHA-256 hashes for `cetintrin.h` and libc++ `fstream`, validates both files before making changes, applies only the documented edits and safely accepts already-patched headers. An unfamiliar header is rejected rather than modified. The downloaded compiler archive's original SHA-256 remains pinned independently.

The patches affect the explicitly selected portable toolchain, not the Windows SDK or operating-system settings. Compiler/runtime version changes require a reviewed update to these hashes and contexts.

## Regression coverage

`source/dependency_tests.cpp` exercises the actual ImGui implementation without creating native windows: numeric formatting and rejection cases, integer edge cases, truncation, large slider ranges, three font sizes, concave triangulation and CPU draw-data generation. It runs in x64 and x86 native builds, and with the portable Linux core tests. The same suite can run with address and undefined-behavior sanitizers.

Graphics/limiter and window-layout regressions run on GitHub's isolated Windows runner. No graphics scenes, test windows or application instances need to open on the development desktop.
