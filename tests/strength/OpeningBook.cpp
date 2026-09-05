#include "strength/OpeningBook.hpp"
#include <array>

namespace Boson {

namespace {

const std::array<OpeningEntry, 20> g_openings = {{
    {
        "open_01",
        "Italian Game",
        "Giuoco Piano",
        {"e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "f8c5"},
        "r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4"
    },
    {
        "open_02",
        "Italian Game",
        "Two Knights Defense",
        {"e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "g8f6"},
        "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4"
    },
    {
        "open_03",
        "Ruy Lopez",
        "Berlin Defense",
        {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "g8f6"},
        "r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4"
    },
    {
        "open_04",
        "Ruy Lopez",
        "Morphy Defense",
        {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "a7a6"},
        "r1bqkbnr/1ppp1ppp/p1n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4"
    },
    {
        "open_05",
        "Sicilian Defense",
        "Open Classical",
        {"e2e4", "c7c5", "g1f3", "d7d6", "d2d4", "c5d4", "f3d4", "g8f6", "b1c3", "b8c6"},
        "r1bqkb1r/pp2pppp/2np1n2/8/3NP3/2N5/PPP2PPP/R1BQKB1R w KQkq - 2 6"
    },
    {
        "open_06",
        "Sicilian Defense",
        "Najdorf Variation",
        {"e2e4", "c7c5", "g1f3", "d7d6", "d2d4", "c5d4", "f3d4", "g8f6", "b1c3", "a7a6"},
        "r1bqkb1r/1p2pppp/p2p1n2/8/3NP3/2N5/PPP2PPP/R1BQKB1R w KQkq - 0 6"
    },
    {
        "open_07",
        "French Defense",
        "Winawer Variation",
        {"e2e4", "e7e6", "d2d4", "d7d5", "b1c3", "f8b4"},
        "rnbqk1nr/ppp2ppp/4p3/3p4/1b1PP3/2N5/PPP2PPP/R1BQKBNR w KQkq - 2 4"
    },
    {
        "open_08",
        "French Defense",
        "Classical Variation",
        {"e2e4", "e7e6", "d2d4", "d7d5", "b1c3", "g8f6"},
        "rnbqkb1r/ppp2ppp/4pn2/3p4/3PP3/2N5/PPP2PPP/R1BQKBNR w KQkq - 2 4"
    },
    {
        "open_09",
        "Caro-Kann Defense",
        "Classical Variation",
        {"e2e4", "c7c6", "d2d4", "d7d5", "b1c3", "d5e4", "c3e4", "c8f5"},
        "rn1qkbnr/pp2pppp/2p5/5b2/4N3/8/PPPP1PPP/R1BQKBNR w KQkq - 1 5"
    },
    {
        "open_10",
        "Caro-Kann Defense",
        "Advance Variation",
        {"e2e4", "c7c6", "d2d4", "d7d5", "e4e5", "c8f5"},
        "rn1qkbnr/pp2pppp/2p5/4Pb2/3P4/8/PPP2PPP/RNBQKBNR w KQkq - 1 4"
    },
    {
        "open_11",
        "Queen's Gambit Declined",
        "Orthodox Defense",
        {"d2d4", "d7d5", "c2c4", "e7e6", "b1c3", "g8f6", "g1f3", "f8e7"},
        "rnbqk2r/ppp1bppp/4pn2/3p4/2PP4/2N2N2/PP2PPPP/R1BQKB1R w KQkq - 2 5"
    },
    {
        "open_12",
        "Queen's Gambit Declined",
        "Tartakower / Standard",
        {"d2d4", "d7d5", "c2c4", "e7e6", "b1c3", "g8f6"},
        "rnbqkb1r/ppp2ppp/4pn2/3p4/2PP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 2 4"
    },
    {
        "open_13",
        "Slav Defense",
        "Classical Variation",
        {"d2d4", "d7d5", "c2c4", "c7c6", "g1f3", "g8f6", "b1c3", "d5c4", "a2a4", "c8f5"},
        "rn1qkb1r/pp2pppp/2p2n2/5b2/P1pP4/2N2N2/1P2PPPP/R1BQKB1R w KQkq - 1 6"
    },
    {
        "open_14",
        "King's Indian Defense",
        "Fianchetto / Classical Setup",
        {"d2d4", "g8f6", "c2c4", "g7g6", "b1c3", "f8g7"},
        "rnbqk2r/ppppppbp/5np1/8/2PP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 2 4"
    },
    {
        "open_15",
        "Nimzo-Indian Defense",
        "Classical Setup",
        {"d2d4", "g8f6", "c2c4", "e7e6", "b1c3", "f8b4"},
        "rnbqk2r/pppp1ppp/4pn2/8/1bPP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 2 4"
    },
    {
        "open_16",
        "Queen's Indian Defense",
        "Main Line Setup",
        {"d2d4", "g8f6", "c2c4", "e7e6", "g1f3", "b7b6"},
        "rnbqkb1r/p1pp1ppp/1p2pn2/8/2PP4/5N2/PP2PPPP/RNBQKB1R w KQkq - 0 4"
    },
    {
        "open_17",
        "Grünfeld Defense",
        "Three Knights Setup",
        {"d2d4", "g8f6", "c2c4", "g7g6", "b1c3", "d7d5"},
        "rnbqkb1r/ppp1pp1p/5np1/3p4/2PP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 0 4"
    },
    {
        "open_18",
        "English Opening",
        "Symmetrical Variation",
        {"c2c4", "c7c5", "b1c3", "b8c6", "g2g3", "g7g6", "f1g2", "f8g7"},
        "r1bqk1nr/pp1pppbp/2n3p1/2p5/2P5/2N3P1/PP1PPPBP/R1BQK1NR w KQkq - 2 5"
    },
    {
        "open_19",
        "English Opening",
        "Four Knights Variation",
        {"c2c4", "e7e5", "b1c3", "g8f6", "g1f3", "b8c6"},
        "r1bqkb1r/pppp1ppp/2n2n2/4p3/2P5/2N2N2/PP1PPPPP/R1BQKB1R w KQkq - 4 4"
    },
    {
        "open_20",
        "Modern Defense",
        "Standard Setup",
        {"e2e4", "g7g6", "d2d4", "f8g7", "b1c3", "d7d6"},
        "rnbqk1nr/ppp1ppbp/3p2p1/8/3PP3/2N5/PPP2PPP/R1BQKBNR w KQkq - 0 4"
    }
}};

} // anonymous namespace

std::span<const OpeningEntry> OpeningBook::getOpenings() noexcept {
    return g_openings;
}

const OpeningEntry& OpeningBook::getOpening(size_t index) noexcept {
    return g_openings[index % g_openings.size()];
}

} // namespace Boson
