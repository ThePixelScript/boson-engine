#include <iostream>
#include "fen/FenParser.hpp"
#include "debug/BoardPrinter.hpp"

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "[BOSON MAIN] Initializing Module 1.2 Verification Pipeline\n";

    // Scenario A: Standard Initial State
    constexpr std::string_view startFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    auto startResult = Boson::FenParser::parse(startFen);
    if (startResult) {
        std::cout << "\n>>> VALID FEN SUCCESS (Initial Position):";
        Boson::BoardPrinter::print(*startResult, Boson::BoardPrinter::Mode::Debug);
    }

    // Scenario B: Deep Middlegame / En Passant Verification
    constexpr std::string_view complexFen = "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2";
    auto complexResult = Boson::FenParser::parse(complexFen);
    if (complexResult) {
        std::cout << "\n>>> VALID FEN SUCCESS (Middlegame Position):";
        Boson::BoardPrinter::print(*complexResult, Boson::BoardPrinter::Mode::Debug);
    }

    // Scenario C: Malformed Input Interception
    constexpr std::string_view malformedFen = "rnbqkbnr/pppppppp/9/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"; // Has a '9' file!
    auto malformedResult = Boson::FenParser::parse(malformedFen);
    if (!malformedResult) {
        std::cout << ">>> INVALID FEN INTERCEPTED: " << Boson::to_string(malformedResult.error()) << " (Expected Pass)\n";
    }

    return 0;
}