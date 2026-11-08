#include "scalar_formats.hpp"
#include "vendor/imgui/imgui.h"
#include "vendor/imgui/imgui_internal.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

int main() {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif
    int checks = 0;
    auto check = [&](bool ok, const char* why) {
        if (!ok)
            throw std::runtime_error(why);
        ++checks;
    };
    try {
        char buffer[256]{};
        int number = 42;
        check(ImGui::DataTypeFormatString(buffer, sizeof(buffer), ImGuiDataType_S32, &number,
                                          "%04d") == 4 &&
                  std::string(buffer) == "0042",
              "retain allowed integer padding");
        unsigned hex = 0x2afe;
        ImGui::DataTypeFormatString(buffer, sizeof(buffer), ImGuiDataType_U32, &hex, "%08X");
        check(std::string(buffer) == "00002AFE", "retain hexadecimal formatting");
        ImS64 signed64 = -9223372036854775807LL;
        ImGui::DataTypeFormatString(buffer, sizeof(buffer), ImGuiDataType_S64, &signed64, "%lld");
        check(std::string(buffer) == "-9223372036854775807", "retain signed 64-bit formatting");
        ImU64 unsigned64 = 18446744073709551615ULL;
        ImGui::DataTypeFormatString(buffer, sizeof(buffer), ImGuiDataType_U64, &unsigned64, "%llu");
        check(std::string(buffer) == "18446744073709551615", "retain full unsigned 64-bit range");
        float fraction = 12.375f;
        ImGui::DataTypeFormatString(buffer, sizeof(buffer), ImGuiDataType_Float, &fraction, "%.2f");
        check(std::string(buffer) == "12.38", "retain decimal precision");
        double precise = 1.2345678901234567;
        ImGui::DataTypeFormatString(buffer, sizeof(buffer), ImGuiDataType_Double, &precise,
                                    "%.17g");
        check(std::string(buffer) == "1.2345678901234567", "retain high precision doubles");
        const char* bad[]{"%n",   "%s",   "%p", "%*d",   "%999999999d", "%d %d",
                          "%1$n", "%lld", "%f", "%I64n", "%hhn",        "x%u"};
        for (const char* format : bad) {
            ImGui::DataTypeFormatString(buffer, sizeof(buffer), ImGuiDataType_S32, &number, format);
            check(std::string(buffer) == "42",
                  "reject unsafe/mismatched/unsupported integer formats");
        }
        for (const char* format : {"%n", "%lld", "%s", "%*f", "%f %f"}) {
            ImGui::DataTypeFormatString(buffer, sizeof(buffer), ImGuiDataType_Float, &fraction,
                                        format);
            check(std::string(buffer) == "12.375", "reject unsafe/mismatched floating formats");
        }
        char callerFormat[]{'%', '0', '4', 'd', 0};
        auto literal = fg::scalarFormatLiteral(callerFormat, fg::ScalarFormat::Signed32);
        check(literal != callerFormat && std::string(literal) == callerFormat,
              "return trusted literal rather than caller storage");
        char small[3]{'x', 'x', 'x'};
        ImGui::DataTypeFormatString(small, sizeof(small), ImGuiDataType_S32, &number, "%04d");
        check(small[2] == 0 && std::string(small) == "00",
              "truncate numeric output with termination");
        for (float t : {0.f, .1234567f, .5f, .75f, 1.f}) {
            ImS64 maximum = 10000000000123456LL;
            auto actual = ImGui::ScaleValueFromRatioT<ImS64, ImS64, double>(ImGuiDataType_S64, t, 0,
                                                                            maximum, false, 0, 0);
            auto expected = static_cast<ImS64>(static_cast<long double>(maximum) * t + .5L);
            check(std::abs(actual - expected) <= 2,
                  "perform slider range multiplication at double precision");
        }
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(800, 600);
        io.DeltaTime = 1.f / 60;
        for (float pixels : {13.f, 26.f, 39.f}) {
            io.Fonts->Clear();
            ImFontConfig config;
            config.SizePixels = pixels;
            io.Fonts->AddFontDefault(&config);
            check(io.Fonts->Build(), "build font atlas after allocation hardening");
            check(io.Fonts->TexPixelsAlpha8 && io.Fonts->TexWidth > 0 && io.Fonts->TexHeight > 0,
                  "retain valid atlas pixels at multiple HUD sizes");
        }
        ImGui::NewFrame();
        ImGui::Begin("Headless dependency regression");
        ImGui::Value("Value", fraction, "%n");
        ImGui::Value("Value", fraction, "%.2f");
        ImGui::End();
        auto draw = ImGui::GetForegroundDrawList();
        int before = draw->IdxBuffer.Size;
        draw->Flags &= ~ImDrawListFlags_AntiAliasedFill;
        const ImVec2 concave[]{ImVec2(20, 20), ImVec2(100, 20), ImVec2(100, 45),  ImVec2(60, 45),
                               ImVec2(60, 80), ImVec2(100, 80), ImVec2(100, 105), ImVec2(20, 105)};
        draw->AddConcavePolyFilled(concave, 8, IM_COL32_WHITE);
        check(draw->IdxBuffer.Size - before == 18,
              "triangulate concave geometry through explicit scratch partitions");
        before = draw->IdxBuffer.Size;
        draw->Flags |= ImDrawListFlags_AntiAliasedFill;
        draw->AddConcavePolyFilled(concave, 8, IM_COL32_WHITE);
        check(draw->IdxBuffer.Size - before == 66, "retain antialiased concave fill geometry");
        ImGui::Render();
        check(ImGui::GetDrawData()->TotalVtxCount > 0,
              "produce CPU draw data without opening a native window");
        ImGui::DestroyContext();
        std::cout << "PASS: " << checks << " headless dependency regression checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
