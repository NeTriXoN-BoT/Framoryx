#pragma once
#include <array>
namespace fg {
struct VoicePreset {
    const wchar_t* name;
    int low, mid, high;
};
inline constexpr std::array<VoicePreset, 5> voicePresets{{{L"Flat", 0, 0, 0},
                                                          {L"Clarity", -3, 0, 3},
                                                          {L"Warmth", 3, 0, 0},
                                                          {L"Presence", -3, 3, 0},
                                                          {L"Bassy voice", 6, -3, 0}}};
inline int voicePresetIndex(int low, int mid, int high) {
    for (size_t i = 0; i < voicePresets.size(); ++i) {
        const auto& preset = voicePresets[i];
        if (preset.low == low && preset.mid == mid && preset.high == high)
            return int(i);
    }
    return int(voicePresets.size()); // Custom bands
}
} // namespace fg
