#ifndef BOSON_BENCHMARK_REPORTER_HPP
#define BOSON_BENCHMARK_REPORTER_HPP

#include "benchmark/BenchmarkTypes.hpp"
#include <iostream>
#include <string>

namespace Boson {

class BenchmarkReporter {
public:
    static void printConsoleReport(const BenchmarkRunRecord& record, std::ostream& out = std::cout);
    static std::string serializeJson(const BenchmarkRunRecord& record);
    static bool writeJsonFile(const std::string& filepath, const BenchmarkRunRecord& record);
    static bool parseJsonParity(const std::string& jsonStr, std::string& outSchemaVersion, std::string& outCorpusVersion, uint64_t& outTotalNodes, size_t& outPosCount);
};

} // namespace Boson

#endif // BOSON_BENCHMARK_REPORTER_HPP
