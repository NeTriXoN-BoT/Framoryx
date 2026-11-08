# Compatibility and security

Framoryx provides a non-injecting monitoring path and a separate experimental in-game path. An allow-list entry is a user preference, not approval from a game publisher or anti-cheat vendor.

| Capability | Current implementation | Practical limits |
| --- | --- | --- |
| FPS capture | PresentMon 2.6.0 ETW events | Permissions and game/security policies can restrict capture |
| Desktop HUD | Transparent, click-through window | Intended for windowed/borderless games; exclusive fullscreen can cover it |
| DX11 in-game HUD/limiter | Explicit DXGI hook, x64/x86 | Requires opt-in; can conflict with other overlays |
| DX12 in-game HUD | Observed queue + D3D11-on-12 | Multiple direct queues on one device disable drawing rather than guess a queue |
| DX12 limiter | Explicit DXGI hook | Can remain available while the HUD uses desktop fallback |
| Vulkan / OpenGL / DX9 | Desktop monitoring | No injected renderer or limiter for these APIs in this release |
| Borderless | Saved Win32 styles, placement and identity | Games may reject/reset changes; true exclusive fullscreen is not universally convertible |
| Audio effects | Endpoint-specific Equalizer APO configuration; optional external RNNoise VST2 | Requires capture/playback APO setup; raw/exclusive paths can bypass effects. No virtual microphone driver is bundled. |

## Use at your own risk

Games with anti-cheat may restrict or penalize overlays and in-game hooks. You are responsible for checking and following each game's rules. Framoryx does not guarantee anti-cheat compatibility. Its maintainers accept no responsibility for bans, suspensions, lost access or other consequences arising from its use.

## Choosing a display method

Start with **Auto**. It uses a recently drawing attached HUD and otherwise uses the desktop HUD. **Desktop** forces the non-injecting counter. **In-game** requires explicit hook opt-in. Choosing it does not automatically inject anything.

If the desktop HUD is covered, try the game's own borderless option. For a Vulkan game, keep the desktop path; selecting an in-game layout does not add a Vulkan renderer.

## Unsigned distribution

The Windows app and optional hook modules are unsigned: no Authenticode publisher certificate is included. Windows may display an unknown-publisher or SmartScreen warning. Release ZIPs have a SHA-256 sidecar and a file manifest. These verify integrity against the published values; they do not certify safety or game approval. Framoryx never changes security settings or adds exclusions.

## Security-tool behavior

Microsoft Defender flagged and quarantined the optional injection stress-test helper during development. Its interrupted run was not a successful stability test. No exclusion, restoration, security-setting change or bypass was used. The separate non-injecting ETW/borderless soak completed.

Optional hook components can still be flagged, blocked or quarantined. The app does not automatically restore or retry blocked components. A clean name/process scan cannot establish that a game permits hooks, and no ban-free guarantee is possible.

## Lifecycle

Disable restores hook entry points. The inactive DLL and resources stay mapped until the game exits to avoid unloading code while a render thread might still return through it. Restart the game before reattaching or moving to a protected session. A missing controller heartbeat disables the hook.

Window restoration uses both process identity and a per-window marker. Normal app exit restores saved borderless windows and closes only its own collector/demo processes.

## Validation scope

Local x64 DX11, x64 DX12 and x86 DX11 scenes exercise real presentations, live caps, HUD customization, font rebuilding, shader-state preservation, resizing and shutdown. A dedicated multi-queue DX12 scene exercises conservative HUD fallback. These are not commercial-game certification tests. Red Dead Redemption 2 and Sleeping Dogs have not been game-tested in this environment.
