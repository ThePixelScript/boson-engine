#include <iostream>
#include <string_view>
#include <string>
#include <vector>
#include "validation/VerificationHarness.hpp"
#include "board/MoveGenerator.hpp"
#include "fen/FenParser.hpp"
#include "benchmark/BenchmarkRunner.hpp"
#include "config/ParameterRegistry.hpp"
#include "search/SearchController.hpp"
#include "eval/nnue/NNUEEvaluator.hpp"

int main(int argc, char* argv[]) {
    Boson::MoveGenerator::initializeTables();

    // First pass: extract and apply all --param flags, separating positional/other args
    std::vector<std::string_view> filteredArgs;
    filteredArgs.push_back(argv[0]);

    bool requireNNUE = false;
    std::string networkPath;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--param") {
            if (i + 1 >= argc) {
                std::cerr << "[ERROR] --param requires argument in format <Name>=<Value>\n";
                return 1;
            }
            std::string_view spec(argv[++i]);
            std::string name, val;
            if (!Boson::ParameterRegistry::parseCliParam(spec, name, val)) {
                std::cerr << "[ERROR] Invalid parameter specification: " << spec << "\n";
                return 1;
            }
            if (!Boson::ParameterRegistry::getInstance().setParamFromString(name, val)) {
                std::cerr << "[ERROR] Failed to set parameter '" << name << "' to '" << val << "'\n";
                return 1;
            }
        } else if (arg.starts_with("--param=")) {
            std::string name, val;
            if (!Boson::ParameterRegistry::parseCliParam(arg, name, val)) {
                std::cerr << "[ERROR] Invalid parameter specification: " << arg << "\n";
                return 1;
            }
            if (!Boson::ParameterRegistry::getInstance().setParamFromString(name, val)) {
                std::cerr << "[ERROR] Failed to set parameter '" << name << "' to '" << val << "'\n";
                return 1;
            }
        } else if (arg == "--require-nnue") {
            requireNNUE = true;
            Boson::eval::nnue::NNUEEvaluator::setRequireNNUE(true);
        } else if (arg == "--network" || arg == "--model" || arg == "--nnue") {
            if (i + 1 >= argc) {
                std::cerr << "[ERROR] " << arg << " requires a file path\n";
                return 1;
            }
            networkPath = argv[++i];
        } else if (arg.starts_with("--network=")) {
            networkPath = std::string(arg.substr(10));
        } else if (arg.starts_with("--model=")) {
            networkPath = std::string(arg.substr(8));
        } else if (arg.starts_with("--nnue=")) {
            networkPath = std::string(arg.substr(7));
        } else {
            filteredArgs.push_back(arg);
        }
    }

    if (requireNNUE) {
        if (networkPath.empty()) {
            std::cerr << "[FATAL] --require-nnue specified, but no NNUE network model path provided via --network / --model / --nnue\n";
            return 1;
        }
        if (!Boson::eval::nnue::NNUEEvaluator::loadModelStrict(networkPath)) {
            std::cerr << "[FATAL] --require-nnue failed to strictly load network model from '" << networkPath << "'\n";
            return 1;
        }
        Boson::ParameterRegistry::getInstance().setParam("Eval_Mode", int64_t{1});
    } else if (!networkPath.empty()) {
        if (!Boson::eval::nnue::NNUEEvaluator::loadModel(networkPath, false)) {
            std::cerr << "[WARN] Failed to load NNUE network model from '" << networkPath << "', falling back to Classical evaluation\n";
            Boson::ParameterRegistry::getInstance().setParam("Eval_Mode", int64_t{0});
        } else {
            Boson::ParameterRegistry::getInstance().setParam("Eval_Mode", int64_t{1});
        }
    }

    // CLI Dispatch Router: bench command
    if (filteredArgs.size() > 1 && (filteredArgs[1] == "bench" || filteredArgs[1] == "--bench")) {
        Boson::BenchmarkConfig config;
        bool explicitHash = false;
        for (size_t i = 2; i < filteredArgs.size(); ++i) {
            std::string_view arg = filteredArgs[i];
            if (arg == "--depth" && i + 1 < filteredArgs.size()) {
                config.overrideDepth = std::stoi(std::string(filteredArgs[++i]));
            } else if (arg == "--hash" && i + 1 < filteredArgs.size()) {
                config.hashSizeMb = static_cast<size_t>(std::stoul(std::string(filteredArgs[++i])));
                explicitHash = true;
            } else if (arg == "--mode" && i + 1 < filteredArgs.size()) {
                std::string_view modeStr = filteredArgs[++i];
                if (modeStr == "persistent") {
                    config.mode = Boson::BenchmarkStateMode::Persistent;
                } else {
                    config.mode = Boson::BenchmarkStateMode::Isolated;
                }
            } else if (arg == "--json" && i + 1 < filteredArgs.size()) {
                config.jsonOutputFile = std::string(filteredArgs[++i]);
            }
        }
        if (!explicitHash) {
            int64_t regHash = Boson::ParameterRegistry::getInstance().getInt("Hash");
            if (regHash > 0) {
                config.hashSizeMb = static_cast<size_t>(regHash);
            }
        }
        Boson::BenchmarkRunner::run(config);
        return 0;
    }

    // CLI Dispatch Router: uci command
    if (filteredArgs.size() > 1 && (filteredArgs[1] == "uci" || filteredArgs[1] == "--uci")) {
        std::string line;
        Boson::ParameterRegistry::getInstance().handleUciCommand("uci");
        while (std::getline(std::cin, line)) {
            if (line == "quit") break;
            Boson::ParameterRegistry::getInstance().handleUciCommand(line);
        }
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