#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include "fen/FenParser.hpp"
#include "board/Position.hpp"
#include "search/Search.hpp"
#include "search/SearchController.hpp"
#include "search/Zobrist.hpp"

// We temporarily define s_tt as public or bypass access controls for the test harness.
// To bypass the 'private' check cleanly without changing your main header, 
// we can use a standard macro trick or remember to move s_tt to public in Search.hpp.
#define private public 
#include "search/Search.hpp"
#undef private

namespace Boson {

void runTacticalSuite(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open test suite: " << filePath << "\n";
        return;
    }

    std::string line;
    int testCount = 0;
    int passedTactics = 0;

    std::cout << "\n=== Executing Tactical Suite: " << filePath << " ===\n";

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        size_t bmPos = line.find("bm ");
        if (bmPos == std::string::npos) continue;

        testCount++; // Increments cleanly for every valid position

        std::string fen = line.substr(0, bmPos - 1);
        std::string bestMoveExpected = line.substr(bmPos + 3);
        if (!bestMoveExpected.empty() && bestMoveExpected.back() == ';') {
            bestMoveExpected.pop_back();
        }

        auto parseResult = FenParser::parse(fen);
        if (!parseResult) {
            std::cerr << "FAIL: Test #" << testCount << " malformed FEN string.\n";
            continue;
        }

        Position pos = parseResult.value();
        std::cout << "Running Pos #" << testCount << "...\n";

        Search::runSearch(pos, 10);

        auto& stats = SearchController::getInstance().getStats();

        std::string bosonMoveStr = "none";
        if (!stats.pvString.empty()) {
            std::vector<std::string> pvMoves;
            std::string tmp;
            std::stringstream ss(stats.pvString);
            while (ss >> tmp) pvMoves.push_back(tmp);
            if (!pvMoves.empty()) {
                bosonMoveStr = pvMoves[0]; 
            }
        }

        if (bosonMoveStr == bestMoveExpected) {
            passedTactics++;
            std::cout << "PASS: Found " << bosonMoveStr << "\n";
        } else {
            std::cout << "FAIL: Expected " << bestMoveExpected << ", Found [" << bosonMoveStr << "]\n";
        }
    }

    std::cout << "\nTactical Accuracy: " << passedTactics << "/" << testCount << " Solved.\n";
}

} // namespace Boson

int main() {
    Boson::Zobrist::initialize();
    Boson::runTacticalSuite("../tests/tactical/wac.epd");
    return 0;
}