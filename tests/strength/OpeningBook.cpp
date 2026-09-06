#include "strength/OpeningBook.hpp"
#include <array>

namespace Boson {

namespace {

const std::array<OpeningEntry, 50> g_openings = {{
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
        "r1bqkb1r/pp2pppp/2np1n2/8/3NP3/2N5/PPP2PPP/R1BQKB1R w KQkq - 3 6"
    },
    {
        "open_06",
        "Sicilian Defense",
        "Najdorf Variation",
        {"e2e4", "c7c5", "g1f3", "d7d6", "d2d4", "c5d4", "f3d4", "g8f6", "b1c3", "a7a6"},
        "rnbqkb1r/1p2pppp/p2p1n2/8/3NP3/2N5/PPP2PPP/R1BQKB1R w KQkq - 0 6"
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
        "rn1qkbnr/pp2pppp/2p5/5b2/3PN3/8/PPP2PPP/R1BQKBNR w KQkq - 1 5"
    },
    {
        "open_10",
        "Caro-Kann Defense",
        "Advance Variation",
        {"e2e4", "c7c6", "d2d4", "d7d5", "e4e5", "c8f5"},
        "rn1qkbnr/pp2pppp/2p5/3pPb2/3P4/8/PPP2PPP/RNBQKBNR w KQkq - 1 4"
    },
    {
        "open_11",
        "Queen's Gambit Declined",
        "Orthodox Defense",
        {"d2d4", "d7d5", "c2c4", "e7e6", "b1c3", "g8f6", "g1f3", "f8e7"},
        "rnbqk2r/ppp1bppp/4pn2/3p4/2PP4/2N2N2/PP2PPPP/R1BQKB1R w KQkq - 4 5"
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
        "rnbqkb1r/ppp1pp1p/5np1/3p4/2PP4/2N5/PP2PPPP/R1BQKBNR w KQkq d6 0 4"
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
    },
    {
        "open_21",
        "King's Pawn Game",
        "Open Game",
        {"e2e4", "e7e5"},
        "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2"
    },
    {
        "open_22",
        "Sicilian Defense",
        "Standard Setup",
        {"e2e4", "c7c5"},
        "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2"
    },
    {
        "open_23",
        "French Defense",
        "Standard Setup",
        {"e2e4", "e7e6"},
        "rnbqkbnr/pppp1ppp/4p3/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2"
    },
    {
        "open_24",
        "Caro-Kann Defense",
        "Standard Setup",
        {"e2e4", "c7c6"},
        "rnbqkbnr/pp1ppppp/2p5/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2"
    },
    {
        "open_25",
        "Pirc Defense",
        "Standard Setup",
        {"e2e4", "d7d6"},
        "rnbqkbnr/ppp1pppp/3p4/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2"
    },
    {
        "open_26",
        "Scandinavian Defense",
        "Standard Setup",
        {"e2e4", "d7d5"},
        "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2"
    },
    {
        "open_27",
        "Alekhine Defense",
        "Standard Setup",
        {"e2e4", "g8f6"},
        "rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2"
    },
    {
        "open_28",
        "Queen's Pawn Game",
        "Closed Game",
        {"d2d4", "d7d5"},
        "rnbqkbnr/ppp1pppp/8/3p4/3P4/8/PPP1PPPP/RNBQKBNR w KQkq d6 0 2"
    },
    {
        "open_29",
        "Indian Defense",
        "Standard Setup",
        {"d2d4", "g8f6"},
        "rnbqkb1r/pppppppp/5n2/8/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 1 2"
    },
    {
        "open_30",
        "Dutch Defense",
        "Standard Setup",
        {"d2d4", "f7f5"},
        "rnbqkbnr/ppppp1pp/8/5p2/3P4/8/PPP1PPPP/RNBQKBNR w KQkq f6 0 2"
    },
    {
        "open_31",
        "English Opening",
        "King's English",
        {"c2c4", "e7e5"},
        "rnbqkbnr/pppp1ppp/8/4p3/2P5/8/PP1PPPPP/RNBQKBNR w KQkq e6 0 2"
    },
    {
        "open_32",
        "English Opening",
        "Symmetrical Setup",
        {"c2c4", "c7c5"},
        "rnbqkbnr/pp1ppppp/8/2p5/2P5/8/PP1PPPPP/RNBQKBNR w KQkq c6 0 2"
    },
    {
        "open_33",
        "Réti Opening",
        "Standard Setup",
        {"g1f3", "d7d5"},
        "rnbqkbnr/ppp1pppp/8/3p4/8/5N2/PPPPPPPP/RNBQKB1R w KQkq d6 0 2"
    },
    {
        "open_34",
        "King's Indian Attack",
        "Standard Setup",
        {"g1f3", "g8f6"},
        "rnbqkb1r/pppppppp/5n2/8/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 2 2"
    },
    {
        "open_35",
        "Scotch Opening",
        "Classical Setup",
        {"e2e4", "e7e5", "g1f3", "b8c6"},
        "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3"
    },
    {
        "open_36",
        "Petroff Defense",
        "Classical Setup",
        {"e2e4", "e7e5", "g1f3", "g8f6"},
        "rnbqkb1r/pppp1ppp/5n2/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3"
    },
    {
        "open_37",
        "Philidor Defense",
        "Exchange Variation Setup",
        {"e2e4", "e7e5", "g1f3", "d7d6"},
        "rnbqkbnr/ppp2ppp/3p4/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 3"
    },
    {
        "open_38",
        "Four Knights Game",
        "Spanish Variation Setup",
        {"e2e4", "e7e5", "b1c3", "g8f6"},
        "rnbqkb1r/pppp1ppp/5n2/4p3/4P3/2N5/PPPP1PPP/R1BQKBNR w KQkq - 2 3"
    },
    {
        "open_39",
        "Vienna Game",
        "Falkbeer Setup",
        {"e2e4", "e7e5", "b1c3", "b8c6"},
        "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/2N5/PPPP1PPP/R1BQKBNR w KQkq - 2 3"
    },
    {
        "open_40",
        "Bishop's Opening",
        "Berlin Defense Setup",
        {"e2e4", "e7e5", "f1c4", "g8f6"},
        "rnbqkb1r/pppp1ppp/5n2/4p3/2B1P3/8/PPPP1PPP/RNBQK1NR w KQkq - 2 3"
    },
    {
        "open_41",
        "Sicilian Defense",
        "Alapin Setup",
        {"e2e4", "c7c5", "c2c3", "d7d5"},
        "rnbqkbnr/pp2pppp/8/2pp4/4P3/2P5/PP1P1PPP/RNBQKBNR w KQkq d6 0 3"
    },
    {
        "open_42",
        "Sicilian Defense",
        "Closed Setup",
        {"e2e4", "c7c5", "b1c3", "b8c6"},
        "r1bqkbnr/pp1ppppp/2n5/2p5/4P3/2N5/PPPP1PPP/R1BQKBNR w KQkq - 2 3"
    },
    {
        "open_43",
        "French Defense",
        "Exchange Setup",
        {"e2e4", "e7e6", "d2d4", "d7d5"},
        "rnbqkbnr/ppp2ppp/4p3/3p4/3PP3/8/PPP2PPP/RNBQKBNR w KQkq d6 0 3"
    },
    {
        "open_44",
        "Caro-Kann Defense",
        "Exchange Setup",
        {"e2e4", "c7c6", "d2d4", "d7d5"},
        "rnbqkbnr/pp2pppp/2p5/3p4/3PP3/8/PPP2PPP/RNBQKBNR w KQkq d6 0 3"
    },
    {
        "open_45",
        "Queen's Gambit",
        "Declined Setup",
        {"d2d4", "d7d5", "c2c4", "e7e6"},
        "rnbqkbnr/ppp2ppp/4p3/3p4/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3"
    },
    {
        "open_46",
        "Queen's Gambit",
        "Slav Setup",
        {"d2d4", "d7d5", "c2c4", "c7c6"},
        "rnbqkbnr/pp2pppp/2p5/3p4/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3"
    },
    {
        "open_47",
        "King's Indian Defense",
        "Normal Setup",
        {"d2d4", "g8f6", "c2c4", "g7g6"},
        "rnbqkb1r/pppppp1p/5np1/8/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3"
    },
    {
        "open_48",
        "Nimzo-Indian Defense",
        "Normal Setup",
        {"d2d4", "g8f6", "c2c4", "e7e6"},
        "rnbqkb1r/pppp1ppp/4pn2/8/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3"
    },
    {
        "open_49",
        "Benoni Defense",
        "Modern Setup",
        {"d2d4", "g8f6", "c2c4", "c7c5"},
        "rnbqkb1r/pp1ppppp/5n2/2p5/2PP4/8/PP2PPPP/RNBQKBNR w KQkq c6 0 3"
    },
    {
        "open_50",
        "Catalan Opening",
        "Standard Setup",
        {"d2d4", "g8f6", "g1f3", "d7d5"},
        "rnbqkb1r/ppp1pppp/5n2/3p4/3P4/5N2/PPP1PPPP/RNBQKB1R w KQkq d6 0 3"
    }
}};

} // anonymous namespace

std::span<const OpeningEntry> OpeningBook::getOpenings() noexcept {
    return g_openings;
}

const OpeningEntry& OpeningBook::getOpening(size_t index) noexcept {
    return g_openings[index % g_openings.size()];
}

size_t OpeningBook::size() noexcept {
    return g_openings.size();
}

} // namespace Boson
