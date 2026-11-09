#include "core.hpp"
#include "presentation.hpp"
#include "audio_presets.hpp"
#include <iostream>
#include <stdexcept>
int main() {
    int checks = 0;
    auto check = [&](bool ok, const char* why) {
        ++checks;
        if (!ok)
            throw std::runtime_error(why);
    };
    try {
        check(fg::wildcard("Game*.EXE", "game_client.exe"), "case-insensitive wildcard");
        check(!fg::wildcard("Game?.exe", "game_long.exe"), "single character wildcard");
        check(fg::validRule("game*.exe") && !fg::validRule("C:\\game.exe") && !fg::validRule("*"),
              "rule validation");
        fg::Rules rules;
        rules.allow = {"*.exe"};
        rules.deny = {"protected*.exe"};
        check(!rules.hookAllowed("protected_game.exe"), "deny wins over allow");
        check(!rules.hookAllowed("dwm.exe") && !rules.hookAllowed("EasyAntiCheat_EOS.exe"),
              "immutable system denies");
        check(rules.hookAllowed("singleplayer.exe"), "explicit allowed game");
        auto cells = fg::csv("\"a,b\",\"a\"\"b\",x\r");
        check(cells.size() == 3 && cells[0] == "a,b" && cells[1] == "a\"b", "CSV quoting");
        check(fg::csv("\"unfinished").empty(), "malformed CSV quote");
        fg::Parser parser;
        check(!parser.parse("garbage"), "diagnostic before header");
        parser.parse("Application,SwapChainAddress,MsBetweenPresents,ProcessID,TimeInSeconds");
        auto frame = parser.parse("\"my,game.exe\",0x12,16.666667,42,1.0");
        check(frame && frame->pid == 42 && frame->chain == "0x12" && frame->app == "my,game.exe",
              "header-based reordered CSV");
        check(!parser.parse("game.exe,0x1,NA,42,1.0"), "NA ignored");
        check(!parser.parse("game.exe,0x1,nan,42,1.0"), "NaN ignored");
        check(!parser.parse("game.exe,0x1,-1,42,1.0"), "negative interval ignored");
        check(!parser.parse("game.exe,0x1,16,42.5,1.0"), "fractional PID ignored");
        check(!parser.parse("game.exe,0x1,16,4294967296,1.0"), "overflow PID ignored");
        parser.parse(
            "Application,ProcessID,SwapChainAddress,Runtime,TimeInSeconds,msBetweenPresents");
        check(parser.parse("real.exe,8,0x12,DXGI,1.0,16.67").has_value(),
              "actual PresentMon 2.6 v1 header capitalization");
        fg::Store store;
        const auto legacy = parser.parse("legacy.exe,9,0x10,D3D9,1.0,16.67");
        check(legacy && legacy->runtime == fg::PresentRuntime::D3D9, "recognize Direct3D 9 presents");
        store.add(*legacy, 1.0);
        check(store.snapshot(9, 1.0).runtime == fg::PresentRuntime::D3D9,
              "legacy runtime stays attached to its own frame stream");
        for (int i = 0; i < 1800; ++i)
            store.add({"game.exe", "primary", 42, i / 60.0, 1000.0 / 60.0}, i / 60.0);
        for (int i = 0; i < 30; ++i)
            store.add({"game.exe", "secondary", 42, 28.0 + i / 15.0, 1000.0 / 15.0}, 30.0);
        auto s = store.snapshot(42, 30.0);
        check(s.live && std::abs(s.fps - 60) < 0.001 && std::abs(s.low - 60) < 0.001,
              "FPS and low calculation");
        check(s.chain == "primary", "swapchains never mixed");
        check(store.hasLiveFrames(42, 31.999) && !store.hasLiveFrames(42, 32.0) &&
                  !store.hasLiveFrames(43, 30.0),
              "liveness query uses the same two-second window and target PID");
        check(!store.snapshot(42, 33.0).live && store.snapshot(42, 33.0).fps == 0,
              "stale data is not live");
        store.add({"game.exe", "primary", 42, 31.0, 100}, 31.0);
        auto spike = store.snapshot(42, 31.0);
        check(spike.low < 60 && spike.low > 0, "stutter affects low");
        store.add({"other.exe", "primary", 42, 0.1, 10}, 32.0);
        check(store.snapshot(42, 32.0).app == "other.exe", "PID reuse resets history");
        fg::Store bounded;
        for (int i = 0; i < 100000; ++i)
            bounded.add({"fast.exe", "1", 1, i / 1000.0, 1.0}, i / 1000.0);
        check(bounded.snapshot(1, 100.0).frames.size() <= 30001, "bounded rolling history");
        fg::Store budget;
        for (int pid = 1; pid <= 20; ++pid)
            for (int i = 0; i < 30000; ++i)
                budget.add({"game.exe", "1", uint32_t(pid), i / 1000.0, 1.0}, double(pid));
        check(budget.storedFrameCount() <= fg::Store::frameBudget,
              "global history memory budget across processes");
        budget.clear();
        check(budget.storedFrameCount() == 0, "clearing capture releases all frame history");
        check(fg::overlayScale(0, 1080) == 1.f && fg::overlayScale(0, 2160) == 2.f,
              "overlay automatic size follows actual render resolution");
        check(fg::overlayScale(200, 720) == 2.f && fg::overlayScale(200, 2160) == 2.f,
              "manual overlay percentage is independent of game resolution");
        check(fg::overlayScale(0, 240) >= .8f && fg::overlayScale(900, 1080) <= 3.f,
              "overlay scaling stays within valid rendering limits");
        check(fg::desktopScale(0, 192, 3840, 2160) >= 2.f &&
                  fg::desktopScale(0, 96, 3840, 2160) > fg::desktopScale(0, 96, 1920, 1080),
              "desktop automatic size follows high monitor DPI");
        float fitted = fg::desktopScale(175, 144, 1366, 768);
        check(fitted * 1180 <= 1366 && fitted * 820 <= 768 - 70,
              "desktop percentages fit a smaller work area");
        fg::HudOptions invalid{0xffffffffu, 900, -10, 20};
        invalid.validate();
        check(invalid.fields == fg::HudAll && invalid.scale == 300 && invalid.opacity == 0 &&
                  invalid.accent == fg::hudRainbowAccent,
              "invalid saved overlay styles are constrained before crossing IPC");
        check(fg::hudRainbowPhase(fg::hudRainbowCycleMs) == 0.f &&
                  fg::hudRainbowColor(0.25f) == fg::hudRainbowColor(1.25f) &&
                  fg::hudRainbowColor(fg::hudRainbowPhase(fg::hudRainbowCycleMs / 4)) != fg::hudRainbowColor(0.f),
              "rainbow animation changes hue and wraps continuously");
        check(fg::hudTemperatureColor(-1) == 0 && fg::hudTemperatureColor(64.9) == 0x7df2c6 &&
                  fg::hudTemperatureColor(65) == 0xf7be6f && fg::hudTemperatureColor(80) == 0xff8b96,
              "temperature bands leave missing readings neutral");
        const auto tint = [](uint32_t field, double number, double capacity = 0) {
            return fg::hudValueColor(field, number, true, false, capacity);
        };
        check(tint(fg::HudFps, 60) == 0x7df2c6 && tint(fg::HudAverage, 30) == 0xf7be6f &&
                  tint(fg::HudLow, 29) == 0xff8b96 && tint(fg::HudCap, 0) == 0,
              "frame rates and numeric caps use frame rate bands");
        check(tint(fg::HudTime, 1000. / 60) == 0x7df2c6 && tint(fg::HudTime, 1000. / 30) == 0xf7be6f &&
                  tint(fg::HudTime, 34) == 0xff8b96 && tint(fg::HudTime, 0) == 0,
              "higher frame time moves toward the warmer bands");
        check(tint(fg::HudCpu, 64.9) == 0x7df2c6 && tint(fg::HudGpu, 65) == 0xf7be6f &&
                  tint(fg::HudVram, 90) == 0xff8b96 && tint(fg::HudRam, 29, 32) == 0xff8b96 &&
                  tint(fg::HudRam, 29) == 0,
              "usage colors use percentage, including RAM when capacity is known");
        check(tint(fg::HudGpuTemp, 80) == 0xff8b96 && tint(fg::HudCpuTemp, -1) == 0 &&
                  tint(fg::HudPower, 200) == 0 && tint(fg::HudClock, 2500) == 0 &&
                  fg::hudValueColor(fg::HudCpuTemp, 75, false, true) == 0xf7be6f &&
                  fg::hudValueColor(fg::HudCpu, 75, false, true) == 0,
              "temperature-only preference and unscaled metrics keep their original behavior");
        fg::HudOptions custom;custom.order=0;custom.x=20000;custom.y=-8;custom.fontScale=500;custom.validate();
        check(custom.order==fg::HudOptions::defaultOrder&&custom.x==10000&&custom.y==-1&&custom.fontScale==140,"repair malformed order, coordinates and text size");
        custom.order=0xfedcba9876543210ull;custom.opacity=0;custom.validate();
        check(fg::orderedFields(custom)[0]==fg::HudFps&&fg::orderedFields(custom)[9]==fg::HudBrand&&custom.opacity==0,"readout order and fully transparent background are retained");
        for(int style=0;style<5;++style)for(uint32_t fields=0;fields<=fg::HudAll;++fields){custom.style=style;custom.fields=fields;const auto layout=fg::hudLayout(custom);size_t count=0;for(int bit=0;bit<16;++bit)if(fields&(1u<<bit))++count;bool fits=layout.count==count;for(size_t i=0;i<layout.count;++i){const auto& cell=layout.cells[i];fits&=cell.x>=0&&cell.y>=0&&cell.x+cell.width<=layout.width+.1f&&cell.y+cell.height<=layout.height+.1f;}check(fits,"every readout combination fits all five layouts");}
        double fast = 0, slow = 0;
        for (int i = 0; i < 20; ++i)
            fast = fg::approach(fast, 100, .016);
        for (int i = 0; i < 10; ++i)
            slow = fg::approach(slow, 100, .032);
        check(std::abs(fast - slow) < .0001, "motion easing is independent of frame cadence");
        check(fg::easeOut(-1) == 0 && fg::easeOut(2) == 1,
              "navigation transitions stop at their endpoints");
        for (auto size : {std::pair{860, 630}, std::pair{1180, 820}, std::pair{1920, 1080},
                          std::pair{3440, 1440}, std::pair{3840, 2160}})
            for (int dpi : {96, 144, 192})
                for (int percent : {0, 80, 100, 125, 175}) {
                    auto view = fg::desktopViewport(percent, float(dpi), 3840, 2160, size.first,
                                                    size.second);
                    check(view.scale > 0 && view.x >= 0 && view.y >= 0 &&
                              view.x + 1180 * view.scale <= size.first + .1f &&
                              view.y + 820 * view.scale <= size.second + .1f,
                          "scaled canvas fits normal, maximized and ultrawide client areas without "
                          "stretching");
                }
        auto normal = fg::desktopViewport(80, 96, 1920, 1080, 1180, 820);
        auto maximized = fg::desktopViewport(80, 96, 1920, 1080, 1920, 1080);
        check(normal.scale == maximized.scale && maximized.x > normal.x && maximized.y > normal.y,
              "maximizing preserves requested percentage and centers the design");
        check(fg::voicePresets[4].low == 6 && fg::voicePresets[4].mid == -3 &&
                  fg::voicePresets[4].high == 0,
              "bassy voice adds body without a treble boost");
        check(fg::voicePresetIndex(6, -3, 0) == 4 &&
                  fg::voicePresetIndex(6, 6, 0) == int(fg::voicePresets.size()),
              "saved and custom voice bands map to the correct preset labels");
        std::cout << "PASS: " << checks << " core checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
