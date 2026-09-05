#ifndef BOSON_INTEGRITY_SUITE_HPP
#define BOSON_INTEGRITY_SUITE_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include "PositionFingerprint.hpp"

namespace Boson {

/// \brief Quantitative metric aggregation for the 3-layer state transition audit.
struct IntegrityReport {
    uint64_t positionsTested{0};
    uint64_t legalMovesTested{0};
    uint64_t randomWalkSequences{0};
    uint32_t maxSequenceDepth{0};
    uint64_t totalWalkPlies{0};

    // Special event stress counters
    uint64_t kingsideCastles{0};
    uint64_t queensideCastles{0};
    uint64_t rookCapturesRevokingCastling{0};
    uint64_t epPushes{0};
    uint64_t epCaptures{0};
    uint64_t promotionsQ{0};
    uint64_t promotionsR{0};
    uint64_t promotionsB{0};
    uint64_t promotionsN{0};
    uint64_t checkEvasions{0};
    uint64_t doubleCheckEvasions{0};

    // Charter invariant violation counters (strictly 0 required)
    uint64_t makeUndoMismatches{0};
    uint64_t searchStateLeaks{0};
    uint64_t hashMismatches{0};
    uint64_t castlingFailures{0};
    uint64_t epFailures{0};
    uint64_t promotionFailures{0};
    uint64_t checkStateFailures{0};
    uint64_t crashesOrUb{0};

    [[nodiscard]] bool isPass() const noexcept {
        return makeUndoMismatches == 0
            && searchStateLeaks == 0
            && hashMismatches == 0
            && castlingFailures == 0
            && epFailures == 0
            && promotionFailures == 0
            && checkStateFailures == 0
            && crashesOrUb == 0;
    }
};

class IntegrityRunner {
public:
    static IntegrityReport runSuite(bool verbose = false);
    static bool runMilestoneOmegaPhase3IntegrityTests();
};

} // namespace Boson

#endif // BOSON_INTEGRITY_SUITE_HPP
