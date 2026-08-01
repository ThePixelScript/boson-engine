#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include "fen/FenParser.hpp"
#include "board/Position.hpp"
#include "board/MoveGenerator.hpp"
#include "search/Search.hpp"
#include "search/SearchController.hpp"
#include "search/Zobrist.hpp"

namespace Boson {

struct TestCase {
    std::string fen;
    std::string expectedMove;
};

std::vector<TestCase> loadEPD(const std::string& filepath) {
    std::vector<TestCase> testCases;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open EPD file: " << filepath << std::endl;
        return testCases;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        size_t bmPos = line.find("bm ");
        if (bmPos == std::string::npos) continue;

        std::string fen = line.substr(0, bmPos);
        
        size_t moveStart = bmPos + 3;
        size_t moveEnd = line.find(';', moveStart);
        std::string expectedMove = line.substr(moveStart, moveEnd - moveStart);

        while (!fen.empty() && fen.back() == ' ') fen.pop_back();
        while (!expectedMove.empty() && expectedMove.back() == ' ') expectedMove.pop_back();

        testCases.push_back({fen, expectedMove});
    }
    return testCases;
}

int runTacticalSuite() {
    Zobrist::initialize();
    MoveGenerator::initializeTables();

    std::string epdPath = "../tests/tactical/wac.epd";
    auto testCases = loadEPD(epdPath);

    if (testCases.empty()) {
        std::cout << "No test cases loaded from " << epdPath << "\n";
        return 1;
    }

    std::cout << "\n=== Executing Tactical Suite: " << epdPath << " ===\n";
    int solvedCount = 0;

    for (size_t i = 0; i < testCases.size(); ++i) {
        std::cout << "Running Pos #" << (i + 1) << "...\n";
        auto parseResult = FenParser::parse(testCases[i].fen);
        if (!parseResult) {
            std::cout << "FAIL: Invalid FEN on position #" << (i + 1) << "\n";
            continue;
        }

        Position pos = parseResult.value();
        Search::runSearch(pos, 12);

        // ✅ Extract the best move (first token) from stats.pvString
        auto& stats = SearchController::getInstance().getStats();
        std::string foundMoveStr = "";
        
        if (!stats.pvString.empty()) {
            std::stringstream ss(stats.pvString);
            ss >> foundMoveStr; // Extracts the first move in the PV
        }

        if (foundMoveStr == testCases[i].expectedMove) {
            std::cout << "PASS: " << foundMoveStr << "\n";
            solvedCount++;
        } else {
            std::cout << "FAIL: Expected " << testCases[i].expectedMove << ", Found [" << foundMoveStr << "]\n";
        }
        std::cout << "==================================================\n";
    }

    std::cout << "\nTactical Accuracy: " << solvedCount << "/" << testCases.size() << " Solved.\n";
    return 0;
}

} // namespace Boson

int main() {
    return Boson::runTacticalSuite();
}