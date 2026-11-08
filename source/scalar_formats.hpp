#pragma once
#include <cstring>

namespace fg {
// The overlay's dependency profile accepts numeric presentation formats only.
// Always return a literal from a typed pool, never the caller's format pointer.
enum class ScalarFormat { Signed32, Unsigned32, Signed64, Unsigned64, Float, Double };
inline const char* scalarFormatLiteral(const char* requested, ScalarFormat kind) {
    static constexpr const char* integer32[]{
        "%d",   "%i",   "%u",   "%x",   "%X",   "%o",   "%+d",  "% d",  "%02d", "%03d",
        "%04d", "%06d", "%08d", "%10d", "%02u", "%03u", "%04u", "%06u", "%08u", "%10u",
        "%02x", "%04x", "%08x", "%02X", "%04X", "%08X", "%#x",  "%#X",  "%#o"};
    static constexpr const char* integer64[]{"%lld",
                                             "%lli",
                                             "%llu",
                                             "%llx",
                                             "%llX",
                                             "%llo",
                                             "%+lld",
                                             "% lld",
                                             "%02lld",
                                             "%03lld",
                                             "%04lld",
                                             "%08lld",
                                             "%016lld",
                                             "%02llu",
                                             "%04llu",
                                             "%08llu",
                                             "%016llu",
                                             "%016llx",
                                             "%016llX",
                                             "%#llx",
                                             "%#llX",
                                             "%#llo"
#ifdef _MSC_VER
                                             ,
                                             "%I64d",
                                             "%I64i",
                                             "%I64u",
                                             "%I64x",
                                             "%I64X",
                                             "%I64o"
#endif
    };
    static constexpr const char* real[]{
        "%f",   "%F",   "%e",    "%E",    "%g",    "%G",   "%a",   "%A",   "%+f",
        "% f",  "%.0f", "%.1f",  "%.2f",  "%.3f",  "%.4f", "%.5f", "%.6f", "%.7f",
        "%.8f", "%.9f", "%.10f", "%.11f", "%.12f", "%.0e", "%.1e", "%.2e", "%.3e",
        "%.6e", "%.9e", "%.1g",  "%.2g",  "%.3g",  "%.4g", "%.5g", "%.6g", "%.7g",
        "%.8g", "%.9g", "%.10g", "%.12g", "%.15g", "%.17g"};
    auto find = [requested](const auto& literals) -> const char* {
        if (requested)
            for (const char* literal : literals)
                if (std::strcmp(requested, literal) == 0)
                    return literal;
        return nullptr;
    };
    if (kind == ScalarFormat::Signed32 || kind == ScalarFormat::Unsigned32) {
        if (auto literal = find(integer32))
            return literal;
        return kind == ScalarFormat::Signed32 ? "%d" : "%u";
    }
    if (kind == ScalarFormat::Signed64 || kind == ScalarFormat::Unsigned64) {
        if (auto literal = find(integer64))
            return literal;
#ifdef _MSC_VER
        return kind == ScalarFormat::Signed64 ? "%I64d" : "%I64u";
#else
        return kind == ScalarFormat::Signed64 ? "%lld" : "%llu";
#endif
    }
    if (auto literal = find(real))
        return literal;
    return kind == ScalarFormat::Float ? "%.3f" : "%f";
}
} // namespace fg
