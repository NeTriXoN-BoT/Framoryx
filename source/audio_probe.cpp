// Isolated VST2 compatibility probe. No plugin or SDK code is linked.
#include "microphone.hpp"
#include <thread>
#include <iostream>
#include <array>
#include <random>
#include <wincrypt.h>
struct Effect;
using Host = intptr_t (*)(Effect*, int, int, intptr_t, void*, float);
using Dispatch = intptr_t (*)(Effect*, int, int, intptr_t, void*, float);
using Process = void (*)(Effect*, float**, float**, int);
struct Effect {
    int magic;
    Dispatch dispatch;
    void* process;
    void (*set)(Effect*, int, float);
    float (*get)(Effect*, int);
    int programs, params, inputs, outputs, flags;
    intptr_t reserved1, reserved2;
    int delay, quality1, quality2;
    float ratio;
    void* object;
    void* user;
    int id, version;
    Process replace;
    void* replaceDouble;
    char future[56];
};
intptr_t host(Effect*, int op, int, intptr_t, void*, float) {
    if (op == 1)
        return 2400;
    if (op == 16)
        return 48000;
    if (op == 17)
        return 480;
    if (op == 23)
        return 2;
    return 0;
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 2)
        return 2;
    auto library = LoadLibraryExW(
        argv[1], nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!library) {
        std::cerr << "Plugin load failed " << GetLastError() << '\n';
        return 1;
    }
    auto entry = reinterpret_cast<Effect* (*)(Host)>(GetProcAddress(library, "VSTPluginMain"));
    if (!entry)
        return 1;
    Effect* effect = entry(host);
    if (!effect || effect->magic != 0x56737450)
        return 1;
    if (effect->inputs < 1 || effect->inputs > 2 || effect->outputs < 1 || effect->outputs > 2)
        return 1;
    effect->dispatch(effect, 0, 0, 0, nullptr, 0);
    effect->dispatch(effect, 10, 0, 0, nullptr, 48000);
    effect->dispatch(effect, 11, 0, 480, nullptr, 0);
    for (int preset = 0; preset < 3; ++preset) {
        auto encoded = fg::noiseChunk(preset);
        DWORD count = 0;
        CryptStringToBinaryA(encoded.c_str(), DWORD(encoded.size()), CRYPT_STRING_BASE64, nullptr,
                             &count, nullptr, nullptr);
        std::vector<BYTE> bytes(count);
        CryptStringToBinaryA(encoded.c_str(), DWORD(encoded.size()), CRYPT_STRING_BASE64,
                             bytes.data(), &count, nullptr, nullptr);
        effect->dispatch(effect, 24, 1, count, bytes.data(), 0);
        float expected[] = {.5f, .85f, .95f};
        bool threshold = false;
        for (int i = 0; i < effect->params; ++i) {
            char name[256]{};
            effect->dispatch(effect, 8, i, 0, name, 0);
            if (i == 0) {
                std::cout << "preset " << preset << ": threshold=" << effect->get(effect, i)
                          << '\n';
                threshold = std::abs(effect->get(effect, i) - expected[preset]) < .0001f;
            }
        }
        if (!threshold)
            return 1;
    }
    effect->dispatch(effect, 12, 0, 1, nullptr, 0);
    std::mt19937 random(73);
    std::uniform_real_distribution<float> noise(-.01f, .01f);
    std::array<std::array<float, 480>, 2> input{}, output{};
    float* in[]{input[0].data(), input[1].data()};
    float* out[]{output[0].data(), output[1].data()};
    double inputPower = 0, outputPower = 0;
    for (int block = 0; block < 500; ++block) {
        for (int channel = 0; channel < effect->inputs; ++channel)
            for (auto& sample : input[channel])
                sample = noise(random);
        effect->replace(effect, in, out, 480);
        for (int channel = 0; channel < effect->outputs; ++channel)
            for (int i = 0; i < 480; ++i) {
                if (!std::isfinite(output[channel][i]) || std::abs(output[channel][i]) > 4.f)
                    return 1;
                if (block > 100) {
                    inputPower += double(input[channel][i]) * input[channel][i];
                    outputPower += double(output[channel][i]) * output[channel][i];
                }
            }
    }
    std::cout << "Finite processed samples: " << 500 * 480 * effect->outputs
              << "; stationary-noise RMS ratio " << std::sqrt(outputPower / inputPower) << '\n';
    effect->dispatch(effect, 12, 0, 0, nullptr, 0);
    effect->dispatch(effect, 1, 0, 0, nullptr, 0);
    FreeLibrary(library);
    return 0;
}
