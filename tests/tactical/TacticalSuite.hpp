#ifndef BOSON_TACTICAL_SUITE_HPP
#define BOSON_TACTICAL_SUITE_HPP

#include <string_view>
#include <vector>
#include <string>
#include <span>

namespace Boson {

/// \brief Specification for a single tactical regression test case.
struct TacticalTestCase {
    std::string_view name{};
    std::string_view fen{};
    std::vector<std::string_view> expectedBestMoves{};
    int minDepth{6};
    int expectedScoreMin{-32000};
    int expectedScoreMax{32000};
    bool isMate{false};
    int mateInPly{0};
};

/// \brief Diagnostic record for an executed tactical test case.
struct TacticalTestResult {
    bool passed{false};
    std::string testName{};
    std::string fen{};
    std::vector<std::string> expectedMoves{};
    std::string actualMove{};
    int depthSearched{0};
    int score{0};
    std::string pvString{};
    std::string failureReason{};
};

/// \brief Summary report for an entire tactical test category.
struct TacticalCategoryReport {
    std::string categoryName{};
    int totalTests{0};
    int passedTests{0};
    int failedTests{0};
    std::vector<TacticalTestResult> results{};
};

/// \brief High-level runner orchestrating tactical regression execution.
class TacticalRegressionRunner {
public:
    static TacticalTestResult runSingleTest(const TacticalTestCase& testCase, bool verbose = false);
    static TacticalCategoryReport runCategory(std::string_view categoryName, std::span<const TacticalTestCase> tests, bool verbose = false);
    static bool runMilestoneOmegaPhase2TacticalTests();
};

std::span<const TacticalTestCase> getMateTestCases() noexcept;
std::span<const TacticalTestCase> getCoreTacticsTestCases() noexcept;
std::span<const TacticalTestCase> getSearchEdgeCaseTestCases() noexcept;

} // namespace Boson

#endif // BOSON_TACTICAL_SUITE_HPP
