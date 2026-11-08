FRAMORYX — YOUR GAMING TOOLKIT
Version 0.2.2 | Native C++ | Windows 10/11 x64

GET STARTED

1. Extract the complete folder. Open Framoryx.exe.
2. Open a game. Auto follows it, or choose its window from the list.
3. Click Show overlay. Capture starts automatically if it is not running.

Keep both FramoryxHook DLLs, Framoryx.exe, assets and tools together.
The source folder is included for rebuilding and inspection.
Quick guide > Open test scene launches a managed local graphics demo and
starts capture for it. Closing Framoryx also closes its own demo scene.
Opening Framoryx again brings its existing window forward. The tray icon
provides capture, overlay, Overlay Studio, Hide window and Exit commands.
Double-click the tray icon to return to the app. Hide window keeps the
toolkit running; Exit and the window's close button shut it down.

OVERLAY STUDIO

Choose a game in the Studio's picker, then make the overlay your own.
Choose Glass for a spacious card, Compact for a smaller footprint or
Telemetry Bar for horizontal readouts. All layouts keep the same choices.
Select live FPS, average FPS, 1% low, frame time, frame graph, system CPU,
system RAM, FPS limit, renderer and the Framoryx label independently.
An empty selection hides the HUD. The preview uses clearly labelled
sample values when no real game data is available.

Size: Auto follows the game's render resolution for an injected HUD,
or the game's display resolution for the desktop HUD. Choose 50–300%
for manual control. Oversized layouts fit within the available display
or render surface so controls do not place the HUD outside the picture.
The in-game renderer rebuilds its font atlas at the effective size.

Choose background opacity, Mint / Ice blue / Violet / Amber accents,
and any of the four corners. Styles are saved separately per executable
basename. With no game selected, changes update the default style.
These preferences never authorize automatic injection.

Display method:
  Auto: use a recently drawing attached HUD; otherwise use the desktop HUD.
  Desktop: force the non-injecting desktop HUD.
  In-game: requires explicit hook opt-in; Enable in-game opens the same
           guarded confirmation as the limiter. A specific game selection
           is required. No fallback to injection happens automatically.

The desktop HUD appears while its target game is foreground. It hides
when you switch to another app, avoiding a counter over your desktop.

DESKTOP SIZE AND MOTION

Overlay Studio > Desktop app size offers Auto plus 80–175% overrides.
Auto uses Windows DPI and the available work area. Manual percentages
are also constrained to fit the current screen. The app updates when
the display/DPI changes; text is drawn at the effective pixel size.
Create shortcut adds a desktop link to this extracted copy. Opening that
shortcut again brings the existing Framoryx window forward.

Motion effects include eased navigation transitions, hover highlights,
switch movement, animated metric changes, a live graph pulse and the
glass-ribbon header. Animation pauses when the app is backgrounded or
minimized. The initial preference follows Windows' animation setting;
the Motion effects switch saves your explicit choice.
The process-local animation clock adapts to the current display up to
144 Hz. Header particles and light traces move continuously rather than
stepping between points. No global Windows timer setting is changed.

COMPATIBILITY

Select a game and open Compatibility for architecture, runtime module
hints, window state, current overlay path and other detected overlays.
Runtime modules can coexist; they are hints rather than proof of the
API currently drawing the game. Live frame data and actual HUD drawing
signals determine whether monitoring is working. Refresh compatibility
and Open borderless controls provide direct next steps.

OVERVIEW

Live FPS, average FPS, 1% low, frame time, a 30-second graph, CPU load
and system memory. Select a particular game to keep monitoring it while
you use another window. Refresh rescans visible application windows.

Measurements normally come from Intel PresentMon's Windows ETW events.
This mode does not inject a DLL or read/write game memory. Membership
in Performance Log Users or administrator privileges may be required.
If capture reports access denied, close Framoryx and use Windows'
Run as administrator, or use an account already configured for ETW.
Framoryx does not silently elevate or change your account's groups.

The graph identifies ETW or HOOK as its current source. If you explicitly
enable the optional hook and ETW becomes unavailable, hook timing can
supply the FPS statistics and graph automatically.

The counter measures intervals between presentation calls. Displayed
FPS and generated-frame FPS can differ. This build does not identify
DLSS/FSR frame generation or calculate end-to-end input latency.
The 1% low is 1000 divided by the mean duration of the slowest 1% of
retained frames. It is shown after at least 100 frames. Histories are
separate for each process and swapchain; their FPS values are not added.

FRAME LIMITER — OPTIONAL AND EXPERIMENTAL

Select a specific game, choose a limit, then Enable / apply limit.
The confirmation can add that executable to your allow list. Limits
are remembered by executable name. Choose 0 for uncapped presentation.

This loads the matching FramoryxHook DLL into the selected process and
hooks DXGI Present/Present1. Architecture selection is automatic: older
32-bit DX11 programs use the bundled fixed-purpose compatibility helper.
The limiter targets DX11/DX12 applications. DX11 also
supports an in-process HUD that can appear in exclusive fullscreen.
DX12 now supports an in-process HUD through an observed, unambiguous
direct command queue. When multiple direct queues on the same device
make that choice ambiguous, drawing falls back to the desktop path in
Auto mode while the limiter remains available. No queue is guessed.
These paths were validated against local 32/64-bit DX11 and DX12 scenes;
commercial games still need game-specific testing.
Vulkan, OpenGL and DX9 use normal ETW monitoring and the desktop
overlay; use their in-game or graphics-driver limiter.

Hooks can conflict with other overlays, crash games or trigger anti-cheat.
No utility can guarantee support for every game or guarantee no bans.
Only use hooks where the game's publisher permits them. The allow list
records your preference; it is not an anti-cheat approval list.

Framoryx checks known anti-cheat process and active service names before
hooking, checks again while a hook is active, and refuses hooking when
inspection fails. This is an incomplete, conservative check. An unknown
or undetected anti-cheat can still be present. There is no stealth mode,
anti-cheat bypass, automatic injection or automatic retry after blocking.

Disable restores hook entry points. The DLL and its inactive resources
remain mapped until the game exits, avoiding unsafe code unloading while
a render thread might still return through it. Restart the game before
reattaching or entering an anti-cheat-protected session. If Framoryx
crashes or loses contact, the hook disables after about 3.5 seconds.

The limiter cannot make a slow game render faster. It preserves the
game's V-Sync settings and does not promise a particular input latency.

BORDERLESS PLAY

Start the game in Windowed mode, select its window and a display, then
click Make borderless. Restore window brings back the original border,
position and size. Normal application exit also restores saved windows.
Games can ignore or reset Windows style and size changes; true exclusive
fullscreen is not universally convertible through window controls.

Auto borderless only acts on games in your allow list. Game rules lets
you add exact executable names or wildcard patterns. It does not hook
games automatically. A desktop overlay can still be covered by exclusive
fullscreen, protected presentation or an application's own topmost window.
For Red Dead Redemption 2, try its built-in borderless windowed setting;
the Vulkan renderer does not support this build's injected DX11 HUD.
Red Dead Redemption 2 and Sleeping Dogs have not been game-tested here.

GAME RULES

Allow and block lists persist in settings.ini. Block rules take priority.
System processes and known anti-cheat executables are always blocked.
Rules use executable basenames, one per line, e.g. game.exe or game*.exe.
Names are matched without ASCII case sensitivity; * and ? are supported.
Blocked targets are hidden from the window list and ignored for automatic
selection. Enabling an optional hook always requires a specific process
selection and a fresh confirmation, even for allow-listed executables.

AUDIO

Output and Microphone have separate views, with a built-in setup guide.
The live volume slider spans 0-200%. Values through 100% control Windows;
above 100% uses Equalizer APO gain (200% = +6.0206 dB, twice the amplitude).
Without an APO config, the slider stops at 100% and explains setup.
Positive gain can clip. This is not a dynamic compressor.

Bass enhancement uses a 90 Hz shelf with matching headroom reduction.
Microphone EQ provides low/mid/high bands and voice presets. RNNoise
suppression uses the official external x64 VST2 plugin at 48,000 Hz.
The latest stable package downloads automatically in the background when
you open Microphone, with compatibility checks and a manual update switch.
New versions are selected for the next Apply; active files are not replaced.
The app records no microphone audio. No virtual mic driver is bundled.

Extra effects require Equalizer APO enabled for the playback/capture
device in its configurator. Finding its config does not prove it is active.
Existing filters are preserved and root configs backed up before linking.
Effects persist after exit until disabled. Raw/exclusive/ASIO paths may
bypass system effects. See docs/AUDIO.md for setup and limitations.
Official project: https://sourceforge.net/projects/equalizerapo/

HOTKEYS

Ctrl+Alt+O     Show/hide desktop overlay
Ctrl+Alt+B     Make the manually selected window borderless
Ctrl+Alt+L     Disable the optional hook

If another app reserves a hotkey, the corresponding button still works.
Motion effects can be switched off. Background/minimized operation
pauses animation while monitoring, hotkeys and tray controls continue.

FILES AND PRIVACY

Settings are portable and saved alongside the program. CSV export is
manual and contains the current target's retained frame data. Frame
history stays in bounded memory; capture does not automatically write
CSV or ETL recordings. Normal operation has no accounts or telemetry.
Audio setup and Windows sound-settings buttons act only when clicked.
Automatic RNNoise downloads contact the official GitHub release service;
this option can be disabled. No voice audio is uploaded.

UNSIGNED WINDOWS BUILD: no Authenticode publisher certificate is included.
Windows may show an unknown-publisher or SmartScreen warning. Verify the
ZIP SHA-256. A matching hash checks integrity, not safety or game approval.
See docs/VALIDATION.md for completed checks and current limits.

BUILD AND VERIFY

See README.md and docs/BUILDING.md for source, validation and dependency notices.
