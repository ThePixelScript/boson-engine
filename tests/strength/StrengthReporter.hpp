#ifndef BOSON_STRENGTH_REPORTER_HPP
#define BOSON_STRENGTH_REPORTER_HPP

#include "strength/StrengthTypes.hpp"
#include <iostream>
#include <string>

namespace Boson {

class StrengthReporter {
public:
    static void printConsoleReport(const MatchRecord& record, std::ostream& out = std::cout);
    static std::string serializeJson(const MatchRecord& record);
    static bool writeJsonFile(const std::string& filepath, const MatchRecord& record);
    static bool parseJsonParity(const std::string& jsonStr, std::string& outSchemaVersion, uint32_t& outTotalGames, double& outScore, double& outDeltaElo);
};

} // namespace Boson

#endif // BOSON_STRENGTH_REPORTER_HPP
