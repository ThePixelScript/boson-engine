#include <iostream>
#include "validation/VerificationHarness.hpp"
#include "board/MoveGenerator.hpp"
#include "fen/FenParser.hpp"

int main() {
    Boson::MoveGenerator::initializeTables();

    const std::string kiwipete = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";

    if (!Boson::VerificationHarness::verifyMakeUndoIntegrity(kiwipete, 2)) {
        std::cout << "Board integrity check FAILED\n";
        return 1;
    }

    auto pos = Boson::FenParser::parse(kiwipete);
    Boson::MoveList moves;
    Boson::MoveGenerator::generateLegalMoves(*pos, moves);
    std::cout << "Kiwipete legal moves: " << moves.size() << " (expected 48)\n";

    Boson::VerificationHarness::debugPerft(kiwipete, 4);

    return 0;
}