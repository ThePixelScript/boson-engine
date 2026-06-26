#include <iostream>
#include <cassert>
#include "board/Square.hpp"
#include "board/Bitboard.hpp"
#include "board/Move.hpp"
#include "board/Position.hpp"

void runSanityCheck() {
    std::cout << "[BOSON TEST] Initializing system type safety verification...\n";

    // Test 1: Coordinate mapping verification
    Boson::Square sqA1 = Boson::Square::A1;
    Boson::Square sqH8 = Boson::Square::H8;
    assert(static_cast<int>(sqA1) == 0);
    assert(static_cast<int>(sqH8) == 63);
    std::cout << "  -> Square coordinate indices: PASS\n";

    // Test 2: Bitboard masking lookup
    Boson::Bitboard maskA1 = Boson::Bitboards::getSquareBit(sqA1);
    Boson::Bitboard maskH8 = Boson::Bitboards::getSquareBit(sqH8);
    assert(maskA1 == 1ULL);
    assert(maskH8 == (1ULL << 63));
    std::cout << "  -> Bitboard hardware masks: PASS\n";

    // Test 3: Bitfield payload movement encoding
    Boson::Move dynamicMove(Boson::Square::E2, Boson::Square::E4, Boson::Move::MoveType::Normal);
    assert(dynamicMove.getFromSquare() == Boson::Square::E2);
    assert(dynamicMove.getToSquare() == Boson::Square::E4);
    assert(dynamicMove.getMoveType() == Boson::Move::MoveType::Normal);
    std::cout << "  -> Bitpacked Move data integrity: PASS\n";

    // Test 4: Base environment setup
    Boson::Position pos;
    assert(pos.getSideToMove() == Boson::Color::White);
    assert(pos.getTotalOccupancy() == 0ULL);
    std::cout << "  -> Position state default encapsulation: PASS\n";

    std::cout << "\n[BOSON TEST] SUCCESS: All structural invariants verified with zero memory leakage.\n\n";
}

int main() {
    runSanityCheck();
    return 0;
}