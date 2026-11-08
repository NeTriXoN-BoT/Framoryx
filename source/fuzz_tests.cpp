#include "core.hpp"
#include <random>
#include <iostream>
int main() {
    std::mt19937 random(20261108);
    fg::Parser parser;
    parser.parse("Application,ProcessID,SwapChainAddress,Runtime,TimeInSeconds,msBetweenPresents");
    for (int trial = 0; trial < 20000; ++trial) {
        std::string line;
        line.resize(random() % 4097);
        for (char& c : line)
            c = char(random() % 256);
        if (trial % 5 == 0)
            line = "game.exe,42,0x12,DXGI," + line + ",16.67";
        auto cells = fg::csv(line);
        if (cells.size() > line.size() + 1)
            return 1;
        auto number = fg::number(line);
        if (number && !std::isfinite(*number))
            return 1;
        auto frame = parser.parse(line);
        if (frame && (!std::isfinite(frame->time) || !std::isfinite(frame->ms) || frame->ms <= 0 ||
                      frame->ms > 60000 || frame->pid == 0))
            return 1;
        fg::validRule(line);
        fg::wildcard(line.substr(0, 128), line.substr(line.size() / 2, 128));
    }
    std::cout << "PASS: 20000 deterministic malformed-input cases\n";
    return 0;
}
