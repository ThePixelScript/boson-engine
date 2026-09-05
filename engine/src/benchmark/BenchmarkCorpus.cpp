#include "benchmark/BenchmarkCorpus.hpp"
#include <array>

namespace Boson {

namespace {

constexpr std::array<BenchmarkPosition, 6> s_canonicalPositions = {{
    {
        "startpos",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        PositionCategory::Standard,
        10
    },
    {
        "kiwipete",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        PositionCategory::Tactical,
        9
    },
    {
        "tactical_wac001",
        "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1",
        PositionCategory::Tactical,
        8
    },
    {
        "positional_closed",
        "r2q1rk1/pp1b1ppp/2n1pn2/2pp4/2PP4/2NBPN2/PP3PPP/R1BQ1RK1 w - - 0 8",
        PositionCategory::Positional,
        8
    },
    {
        "endgame_kpk",
        "8/8/4k3/4p3/4P3/4K3/8/8 w - - 0 1",
        PositionCategory::Endgame,
        10
    },
    {
        "search_stress_evasions",
        "rnb1k1nr/pppp1ppp/4p3/8/3P2q1/5N2/PPP1PPPP/RN1QKB1R w KQkq - 0 1",
        PositionCategory::SearchStress,
        8
    }
}};

} // anonymous namespace

std::span<const BenchmarkPosition> BenchmarkCorpus::getPositions() noexcept {
    return s_canonicalPositions;
}

std::string_view BenchmarkCorpus::getCategoryName(PositionCategory category) noexcept {
    switch (category) {
        case PositionCategory::Standard:     return "Standard";
        case PositionCategory::Tactical:     return "Tactical";
        case PositionCategory::Positional:   return "Positional";
        case PositionCategory::Endgame:      return "Endgame";
        case PositionCategory::SearchStress: return "SearchStress";
        default:                             return "Unknown";
    }
}

} // namespace Boson
