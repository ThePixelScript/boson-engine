#include "search/Zobrist.hpp"

namespace Boson {

uint64_t Zobrist::s_pieces[12][64]{};
uint64_t Zobrist::s_sideToMove = 0ULL;
uint64_t Zobrist::s_castling[16]{};
uint64_t Zobrist::s_enPassant[8]{};
bool Zobrist::s_initialized = false;

uint64_t Zobrist::pseudoRandom64() noexcept {
    // Deterministic LCG state to ensure perfect cross-platform reproducibility
    static uint64_t state = 1804289383ULL;
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return state * 2685821657736338717ULL;
}

void Zobrist::initialize() noexcept {
    if (s_initialized) return;

    for (int p = 0; p < 12; ++p) {
        for (int sq = 0; sq < 64; ++sq) {
            s_pieces[p][sq] = pseudoRandom64();
        }
    }

    s_sideToMove = pseudoRandom64();

    for (int i = 0; i < 16; ++i) {
        s_castling[i] = pseudoRandom64();
    }

    for (int f = 0; f < 8; ++f) {
        s_enPassant[f] = pseudoRandom64();
    }

    s_initialized = true;
}

} // namespace Boson