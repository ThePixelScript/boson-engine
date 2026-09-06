#ifndef BOSON_ENGINE_INFO_HPP
#define BOSON_ENGINE_INFO_HPP

#include <string>
#include <sstream>
#include <string_view>
#include "config/EngineParameters.hpp"

namespace Boson {

class EngineInfo {
public:
    static constexpr std::string_view NAME = "Boson";
    static constexpr std::string_view VERSION = "0.8.0-dev";
    static constexpr std::string_view AUTHOR = "ThePixelScript";

    [[nodiscard]] static constexpr std::string_view getName() noexcept {
        return NAME;
    }

    [[nodiscard]] static constexpr std::string_view getVersion() noexcept {
        return VERSION;
    }

    [[nodiscard]] static constexpr std::string_view getAuthor() noexcept {
        return AUTHOR;
    }

    [[nodiscard]] static constexpr std::string_view getBuildType() noexcept {
#ifdef NDEBUG
        return "Release";
#else
        return "Debug";
#endif
    }

    [[nodiscard]] static std::string getCompiler() {
#if defined(_MSC_VER)
        return "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
        return "Clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__) + "." + std::to_string(__clang_patchlevel__);
#elif defined(__GNUC__)
        return "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__) + "." + std::to_string(__GNUC_PATCHLEVEL__);
#else
        return "Unknown Compiler";
#endif
    }

    [[nodiscard]] static constexpr std::string_view getTargetArch() noexcept {
#if defined(_M_X64) || defined(__x86_64__)
        return "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
        return "arm64";
#elif defined(_M_IX86) || defined(__i386__)
        return "x86_32";
#else
        return "unknown";
#endif
    }

    [[nodiscard]] static std::string getInstructionSets() {
        std::string isets;
#if defined(__AVX512F__)
        isets += "AVX-512 ";
#endif
#if defined(__AVX2__)
        isets += "AVX2 ";
#endif
#if defined(__BMI2__) || (defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86)))
        isets += "BMI2 ";
#endif
#if defined(__POPCNT__) || (defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86)))
        isets += "POPCNT ";
#endif
        if (isets.empty()) {
            isets = "x86_64 Base";
        } else if (isets.back() == ' ') {
            isets.pop_back();
        }
        return isets;
    }

    [[nodiscard]] static std::string serializeConfigSnapshot(const EngineParameters& params) {
        std::ostringstream oss;
        oss << "=== Boson Configuration Snapshot ===\n";
        oss << "Engine: " << NAME << " v" << VERSION << " (" << getBuildType() << ")\n";
        oss << "Compiler: " << getCompiler() << " | Arch: " << getTargetArch() << " | ISA: " << getInstructionSets() << "\n";
        oss << "\n[Search Parameters]\n";
        oss << "  lmrBase: " << params.search.lmrBase << "\n";
        oss << "  lmrDivisor: " << params.search.lmrDivisor << "\n";
        oss << "  lmrMinDepth: " << params.search.lmrMinDepth << "\n";
        oss << "  lmrMinMoveCount: " << params.search.lmrMinMoveCount << "\n";
        oss << "  nmpMinDepth: " << params.search.nmpMinDepth << "\n";
        oss << "  nmpReduction: " << params.search.nmpReduction << "\n";
        oss << "  aspirationInitialDelta: " << params.search.aspirationInitialDelta << "\n";
        oss << "  aspirationMaxDelta: " << params.search.aspirationMaxDelta << "\n";
        oss << "  killerSlotCount: " << params.search.killerSlotCount << "\n";
        oss << "  rfpMarginBase: " << params.search.rfpMarginBase << "\n";
        oss << "  lmrImprovingBonus: " << params.search.lmrImprovingBonus << "\n";
        oss << "\n[Evaluation Parameters]\n";
        oss << "  pawnValue: " << params.eval.pawnValue << "\n";
        oss << "  knightValue: " << params.eval.knightValue << "\n";
        oss << "  bishopValue: " << params.eval.bishopValue << "\n";
        oss << "  rookValue: " << params.eval.rookValue << "\n";
        oss << "  queenValue: " << params.eval.queenValue << "\n";
        oss << "  maxCorrection: " << params.eval.maxCorrection << "\n";
        oss << "  corrScaleFactor: " << params.eval.corrScaleFactor << "\n";
        oss << "\n[Time Parameters]\n";
        oss << "  nodeCheckPeriod: " << params.time.nodeCheckPeriod << "\n";
        oss << "  allocDivisor: " << params.time.allocDivisor << "\n";
        oss << "  incDivisor: " << params.time.incDivisor << "\n";
        oss << "  hardLimitMultiplier: " << params.time.hardLimitMultiplier << "\n";
        oss << "\n[Debug Parameters]\n";
        oss << "  enableNMP: " << (params.debug.enableNMP ? "true" : "false") << "\n";
        oss << "  enableLMR: " << (params.debug.enableLMR ? "true" : "false") << "\n";
        oss << "  enableAspiration: " << (params.debug.enableAspiration ? "true" : "false") << "\n";
        oss << "  enableCMH: " << (params.debug.enableCMH ? "true" : "false") << "\n";
        oss << "  enableContHist: " << (params.debug.enableContHist ? "true" : "false") << "\n";
        oss << "  enableCorrHist: " << (params.debug.enableCorrHist ? "true" : "false") << "\n";
        oss << "====================================\n";
        return oss.str();
    }
};

} // namespace Boson

#endif // BOSON_ENGINE_INFO_HPP
