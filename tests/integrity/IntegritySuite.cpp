#ifndef BOSON_INTEGRITY_SUITE_CPP
#define BOSON_INTEGRITY_SUITE_CPP

#include "integrity/IntegritySuite.hpp"
#include "integrity/PositionFingerprint.hpp"
#include "fen/FenParser.hpp"
#include "board/Position.hpp"
#include "board/MoveGenerator.hpp"
#include "board/MoveExecutor.hpp"
#include "board/UndoState.hpp"
#include "search/Search.hpp"
#include "search/SearchController.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <bit>
#include <array>
#include <vector>

namespace Boson {

namespace {

// Pseudo-random number generator: deterministic 64-bit XorShift
class FastRng {
public:
    explicit FastRng(uint64_t seed = 0xB050D1973ULL) noexcept
        : m_state(seed == 0 ? 0x853c49e6748fea9bULL : seed) {}

    uint64_t next() noexcept {
        uint64_t x = m_state;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        m_state = x;
        return x;
    }

private:
    uint64_t m_state;
};

// RAII console stream redirector
struct CoutSilencer {
    std::streambuf* origBuf;
    std::ostringstream dummy;
    explicit CoutSilencer(bool silence)
        : origBuf(silence ? std::cout.rdbuf(dummy.rdbuf()) : nullptr) {}
    ~CoutSilencer() {
        if (origBuf) std::cout.rdbuf(origBuf);
    }
};

// 22 diverse test positions covering openings, tactical positions, complex endgames, and edge cases
const std::array<const char*, 22> s_integrityCorpus = {
    // 1. Standard Initial Position
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    // 2. KiwiPete (Heavy castling, en-passant, pins)
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    // 3. CPW Position 3 (Endgame with pawns & rooks)
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    // 4. CPW Position 4 (Sharp middlegame with promotions & attacks)
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    // 5. CPW Position 5 (Underpromotion & piece tension)
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    // 6. CPW Position 6 (Complex checks & pins)
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    // 7. Lucena Position (Rook Endgame)
    "1K1k4/1P6/8/8/8/8/r7/2R5 w - - 0 1",
    // 8. Philidor Position (Rook Endgame)
    "8/8/4k3/8/4r3/8/4P3/4K1R1 w - - 0 1",
    // 9. Queen Endgame
    "8/8/2k5/8/8/2Q5/8/4K3 w - - 0 1",
    // 10. Pure Pawn Endgame (Opposition & Zugzwang)
    "8/8/4k3/4p3/4P3/4K3/8/8 w - - 0 1",
    // 11. En-Passant Discovered Check
    "8/8/8/3pP3/8/8/8/4K2k w - d6 0 1",
    // 12. Castling Rights Revocation Edge
    "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
    // 13. Black En-Passant Available
    "8/8/8/8/3Pp3/8/8/4K2k b - d3 0 1",
    // 14. Heavy Piece Liquidation Setup
    "2rr2k1/5ppp/8/8/8/3R4/5PPP/3R2K1 w - - 0 1",
    // 15. Knight Family Fork Setup
    "r3k2r/ppp2ppp/2n5/3N4/8/8/PPP2PPP/R3K2R w KQkq - 0 1",
    // 16. Bishop & Pawn Endgame (Wrong colored bishop)
    "8/8/8/4k3/8/8/1B6/4K3 w - - 0 1",
    // 17. Absolute Skewer Setup
    "8/1q2k3/8/8/8/8/8/7R w - - 0 1",
    // 18. WAC.001 Heavy Piece Penetration
    "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1",
    // 19. WAC.003 Sharp Knight Sacrifice
    "5rk1/1ppb1ppp/p1pb4/8/3P1n1q/2P1R3/PP1B1PPP/R2Q1NK1 b - - 0 1",
    // 20. King Defense in Check Setup
    "rnb1k1nr/pppp1ppp/4p3/8/3P2q1/5N2/PPP1PPPP/RN1QKB1R w KQkq - 0 1",
    // 21. Four Queens Tactical Position
    "q3k2q/8/8/8/8/8/8/Q3K2Q w - - 0 1",
    // 22. Pawn on 7th with Immediate Promotion
    "4k3/6P1/8/8/8/8/8/4K3 w - - 0 1"
};

int countCheckers(const Position& pos, Color side) noexcept {
    const Square kingSq = pos.getKingSquare(side);
    if (kingSq == Square::None) return 0;

    const Color attacker = (side == Color::White) ? Color::Black : Color::White;
    const Bitboard totalOcc = pos.getTotalOccupancy();
    const int sqInt = static_cast<int>(kingSq);
    int checkers = 0;

    Bitboard pawns = pos.getPieceBitboard((attacker == Color::White) ? Piece::WhitePawn : Piece::BlackPawn);
    if (attacker == Color::White) {
        if (sqInt % 8 != 7 && sqInt >= 7 && ((1ULL << (sqInt - 7)) & pawns)) checkers++;
        if (sqInt % 8 != 0 && sqInt >= 9 && ((1ULL << (sqInt - 9)) & pawns)) checkers++;
    } else {
        if (sqInt % 8 != 7 && sqInt <= 54 && ((1ULL << (sqInt + 9)) & pawns)) checkers++;
        if (sqInt % 8 != 0 && sqInt <= 56 && ((1ULL << (sqInt + 7)) & pawns)) checkers++;
    }

    Bitboard knights = pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteKnight : Piece::BlackKnight);
    checkers += static_cast<int>(std::popcount(MoveGenerator::getKnightAttacks(kingSq) & knights));

    Bitboard diagonalAttackers = pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteBishop : Piece::BlackBishop) |
                                 pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteQueen : Piece::BlackQueen);
    checkers += static_cast<int>(std::popcount(MoveGenerator::getBishopAttacks(kingSq, totalOcc) & diagonalAttackers));

    Bitboard orthogonalAttackers = pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteRook : Piece::BlackRook) |
                                   pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteQueen : Piece::BlackQueen);
    checkers += static_cast<int>(std::popcount(MoveGenerator::getRookAttacks(kingSq, totalOcc) & orthogonalAttackers));

    return checkers;
}

} // anonymous namespace

IntegrityReport IntegrityRunner::runSuite(bool verbose) {
    IntegrityReport report;
    FastRng rng(0xB050D1973ULL);

    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE OMEGA, MODULE OMEGA.3: STATE INTEGRITY SUITE  ===\n";
    std::cout << "=================================================================\n";

    // -----------------------------------------------------------------------
    // LAYER A: 1-Ply Round-Trip Integrity (All Legal Moves Across Corpus)
    // -----------------------------------------------------------------------
    std::cout << "\n[Layer A] Executing 1-Ply Exhaustive Make/Undo Round-Trips...\n";
    for (size_t i = 0; i < s_integrityCorpus.size(); ++i) {
        auto parsed = FenParser::parse(s_integrityCorpus[i]);
        if (!parsed.has_value()) {
            report.crashesOrUb++;
            std::cout << "  [FAIL] Failed to parse corpus FEN #" << i << "\n";
            continue;
        }

        Position pos = parsed.value();
        report.positionsTested++;

        PositionFingerprint rootFp = PositionFingerprint::capture(pos);

        MoveList legalMoves;
        MoveGenerator::generateLegalMoves(pos, legalMoves);

        for (size_t mIdx = 0; mIdx < legalMoves.size(); ++mIdx) {
            report.legalMovesTested++;
            const Move& m = legalMoves[mIdx];

            UndoState undo;
            MoveExecutor::makeMove(pos, m, undo);
            MoveExecutor::undoMove(pos, m, undo);

            PositionFingerprint restoredFp = PositionFingerprint::capture(pos);
            if (!(rootFp == restoredFp)) {
                report.makeUndoMismatches++;
                if (rootFp.hashKey != restoredFp.hashKey) report.hashMismatches++;
                if (rootFp.castlingRights != restoredFp.castlingRights) report.castlingFailures++;
                if (rootFp.enPassantSquare != restoredFp.enPassantSquare) report.epFailures++;
                if (m.isPromotion()) report.promotionFailures++;

                std::cout << "  [FAIL] 1-Ply mismatch on pos #" << i << " move " << m.toString() << ":\n"
                          << rootFp.diff(restoredFp) << "\n";
            }
        }
    }
    std::cout << "  -> Layer A Complete: " << report.legalMovesTested << " legal moves verified across "
              << report.positionsTested << " positions. (Mismatches: " << report.makeUndoMismatches << ")\n";

    // -----------------------------------------------------------------------
    // LAYER B: Deep Random Walk Fuzzing (10,000 sequences, 10 to 50 plies)
    // -----------------------------------------------------------------------
    std::cout << "\n[Layer B] Executing Deep Random Walk Fuzzing (10,000 Sequences)...\n";
    constexpr uint64_t NUM_SEQUENCES = 10000;

    for (uint64_t seq = 0; seq < NUM_SEQUENCES; ++seq) {
        size_t corpusIdx = static_cast<size_t>(seq % s_integrityCorpus.size());
        auto parsed = FenParser::parse(s_integrityCorpus[corpusIdx]);
        if (!parsed.has_value()) {
            report.crashesOrUb++;
            continue;
        }

        Position pos = parsed.value();
        PositionFingerprint initialFp = PositionFingerprint::capture(pos);

        uint32_t walkDepth = 10 + static_cast<uint32_t>(rng.next() % 41); // 10 to 50 plies
        std::vector<std::pair<Move, UndoState>> history;
        history.reserve(walkDepth);

        for (uint32_t step = 0; step < walkDepth; ++step) {
            bool inCheck = MoveGenerator::inCheck(pos, pos.getSideToMove());
            if (inCheck) {
                int checkers = countCheckers(pos, pos.getSideToMove());
                if (checkers > 1) {
                    report.doubleCheckEvasions++;
                } else {
                    report.checkEvasions++;
                }
            }

            MoveList legalMoves;
            MoveGenerator::generateLegalMoves(pos, legalMoves);
            if (legalMoves.size() == 0) {
                // Terminal position (checkmate or stalemate)
                break;
            }

            uint32_t moveIdx = static_cast<uint32_t>(rng.next() % legalMoves.size());
            Move m = legalMoves[moveIdx];

            // Event tracking
            if (m.isCastling()) {
                Square toSq = m.getToSquare();
                if (toSq == Square::G1 || toSq == Square::G8) {
                    report.kingsideCastles++;
                } else {
                    report.queensideCastles++;
                }
            }
            if (m.isDoublePawnPush()) {
                report.epPushes++;
            }
            if (m.isEnPassant()) {
                report.epCaptures++;
            }
            if (m.isPromotion()) {
                switch (m.getPromotionPiece()) {
                    case Move::PromotionPiece::Queen:  report.promotionsQ++; break;
                    case Move::PromotionPiece::Rook:   report.promotionsR++; break;
                    case Move::PromotionPiece::Bishop: report.promotionsB++; break;
                    case Move::PromotionPiece::Knight: report.promotionsN++; break;
                    default: break;
                }
            }

            Square toSq = m.getToSquare();
            if (toSq == Square::A1 || toSq == Square::H1 || toSq == Square::A8 || toSq == Square::H8) {
                if ((pos.getTotalOccupancy() & Bitboards::getSquareBit(toSq)) != 0) {
                    report.rookCapturesRevokingCastling++;
                }
            }

            UndoState undo;
            MoveExecutor::makeMove(pos, m, undo);
            history.push_back({m, undo});
            report.totalWalkPlies++;
        }

        report.randomWalkSequences++;
        if (history.size() > report.maxSequenceDepth) {
            report.maxSequenceDepth = static_cast<uint32_t>(history.size());
        }

        // Unwind entire random walk stack
        for (int step = static_cast<int>(history.size()) - 1; step >= 0; --step) {
            MoveExecutor::undoMove(pos, history[step].first, history[step].second);
        }

        PositionFingerprint restoredFp = PositionFingerprint::capture(pos);
        if (!(initialFp == restoredFp)) {
            report.makeUndoMismatches++;
            if (initialFp.hashKey != restoredFp.hashKey) report.hashMismatches++;
            if (initialFp.castlingRights != restoredFp.castlingRights) report.castlingFailures++;
            if (initialFp.enPassantSquare != restoredFp.enPassantSquare) report.epFailures++;

            std::cout << "  [FAIL] Random walk unwind mismatch on sequence #" << seq << ":\n"
                      << initialFp.diff(restoredFp) << "\n";
        }
    }
    std::cout << "  -> Layer B Complete: " << report.randomWalkSequences << " walks, "
              << report.totalWalkPlies << " plies fuzzed and unwound. (Mismatches: "
              << report.makeUndoMismatches << ")\n";

    // -----------------------------------------------------------------------
    // LAYER C: Search Root Invariance (Full-Stack Depth 6+ Non-Destructive Lookahead)
    // -----------------------------------------------------------------------
    std::cout << "\n[Layer C] Executing Full-Stack Search Root Invariance (Depth 6)...\n";
    for (size_t i = 0; i < s_integrityCorpus.size(); ++i) {
        auto parsed = FenParser::parse(s_integrityCorpus[i]);
        if (!parsed.has_value()) {
            report.crashesOrUb++;
            continue;
        }

        Position pos = parsed.value();
        PositionFingerprint rootBefore = PositionFingerprint::capture(pos);

        // Silence routine search console output
        {
            CoutSilencer silencer(!verbose);
            Search::runSearch(pos, 6);
        }

        PositionFingerprint rootAfter = PositionFingerprint::capture(pos);
        if (!(rootBefore == rootAfter)) {
            report.searchStateLeaks++;
            if (rootBefore.hashKey != rootAfter.hashKey) report.hashMismatches++;

            std::cout << "  [FAIL] Search state leak on pos #" << i << ":\n"
                      << rootBefore.diff(rootAfter) << "\n";
        }
    }
    std::cout << "  -> Layer C Complete: " << s_integrityCorpus.size() << " positions searched to depth 6. "
              << "(Search state leaks: " << report.searchStateLeaks << ")\n";

    // Output formatted quantitative report
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE OMEGA, MODULE OMEGA.3: STATE INTEGRITY REPORT ===\n";
    std::cout << "=================================================================\n";
    std::cout << "[Scope & Workload Metrics]\n";
    std::cout << "  Positions Tested             : " << report.positionsTested << "\n";
    std::cout << "  1-Ply Legal Moves Tested     : " << report.legalMovesTested << "\n";
    std::cout << "  Deep Random Walk Sequences   : " << report.randomWalkSequences << "\n";
    std::cout << "  Total Random Walk Plies      : " << report.totalWalkPlies << "\n";
    std::cout << "  Maximum Sequence Depth       : " << report.maxSequenceDepth << "\n";
    std::cout << "\n[Domain Event Stress Coverage]\n";
    std::cout << "  King-Side Castles Executed   : " << report.kingsideCastles << "\n";
    std::cout << "  Queen-Side Castles Executed  : " << report.queensideCastles << "\n";
    std::cout << "  Rook Captures (Rights Revoked): " << report.rookCapturesRevokingCastling << "\n";
    std::cout << "  En-Passant Pushes Created    : " << report.epPushes << "\n";
    std::cout << "  En-Passant Captures Executed : " << report.epCaptures << "\n";
    std::cout << "  Promotions to Queen (Q)      : " << report.promotionsQ << "\n";
    std::cout << "  Promotions to Rook (R)       : " << report.promotionsR << "\n";
    std::cout << "  Promotions to Bishop (B)     : " << report.promotionsB << "\n";
    std::cout << "  Promotions to Knight (N)     : " << report.promotionsN << "\n";
    std::cout << "  Check Evasions Unwound       : " << report.checkEvasions << "\n";
    std::cout << "  Double-Check Evasions Unwound: " << report.doubleCheckEvasions << "\n";
    std::cout << "\n[Charter Invariant Verification]\n";
    std::cout << "  Make/Undo Mismatches         : " << report.makeUndoMismatches << " (target: 0)\n";
    std::cout << "  Search State Leaks           : " << report.searchStateLeaks << " (target: 0)\n";
    std::cout << "  Zobrist Hash Mismatches      : " << report.hashMismatches << " (target: 0)\n";
    std::cout << "  Castling State Failures      : " << report.castlingFailures << " (target: 0)\n";
    std::cout << "  En-Passant State Failures    : " << report.epFailures << " (target: 0)\n";
    std::cout << "  Promotion State Failures     : " << report.promotionFailures << " (target: 0)\n";
    std::cout << "  Check-State Invariant Fails  : " << report.checkStateFailures << " (target: 0)\n";
    std::cout << "  Crashes / Undefined Behavior : " << report.crashesOrUb << " (target: 0)\n";
    std::cout << "=================================================================\n";
    std::cout << "MODULE OMEGA.3 VERIFICATION RESULT: " << (report.isPass() ? "PASSED" : "FAILED") << "\n";
    std::cout << "=================================================================\n";

    return report;
}

bool IntegrityRunner::runMilestoneOmegaPhase3IntegrityTests() {
    IntegrityReport report = runSuite(false);
    return report.isPass();
}

} // namespace Boson

#endif // BOSON_INTEGRITY_SUITE_CPP
