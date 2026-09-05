#include <iostream>
#include <string_view>
#include <string>
#include "validation/VerificationHarness.hpp"
#include "board/MoveGenerator.hpp"
#include "fen/FenParser.hpp"
#include "benchmark/BenchmarkRunner.hpp"

int main(int argc, char* argv[]) {
    Boson::MoveGenerator::initializeTables();

    // CLI Dispatch Router: bench command
    if (argc > 1 && (std::string_view(argv[1]) == "bench" || std::string_view(argv[1]) == "--bench")) {
        Boson::BenchmarkConfig config;
        for (int i = 2; i < argc; ++i) {
            std::string_view arg(argv[i]);
            if (arg == "--depth" && i + 1 < argc) {
                config.overrideDepth = std::stoi(argv[++i]);
            } else if (arg == "--hash" && i + 1 < argc) {
                config.hashSizeMb = static_cast<size_t>(std::stoul(argv[++i]));
            } else if (arg == "--mode" && i + 1 < argc) {
                std::string_view modeStr(argv[++i]);
                if (modeStr == "persistent") {
                    config.mode = Boson::BenchmarkStateMode::Persistent;
                } else {
                    config.mode = Boson::BenchmarkStateMode::Isolated;
                }
            } else if (arg == "--json" && i + 1 < argc) {
                config.jsonOutputFile = argv[++i];
            }
        }
        Boson::BenchmarkRunner::run(config);
        return 0;
    }

    // Default Diagnostic Verification Mode
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