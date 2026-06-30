#include "validation/VerificationHarness.hpp"
#include "fen/FenParser.hpp"
#include "search/Search.hpp"
#include "board/MoveGenerator.hpp"
#include "board/MoveExecutor.hpp"
#include "board/UndoState.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <string>
#include <atomic>
#include <algorithm>
#include <execution>
#include <mutex>
#include <intrin.h>

namespace Boson {

namespace {

constexpr const char* kDebugLogPath = "debug_log.txt";

const char* pieceLabel(size_t index) noexcept {
    static const char* labels[12] = {
        "WhitePawn", "WhiteKnight", "WhiteBishop", "WhiteRook", "WhiteQueen", "WhiteKing",
        "BlackPawn", "BlackKnight", "BlackBishop", "BlackRook", "BlackQueen", "BlackKing"
    };
    return labels[index];
}

const char* castlingRightsLabel(CastlingRights rights) noexcept {
    switch (static_cast<uint8_t>(rights)) {
        case 0: return "None(0)";
        case 1: return "WhiteOO(1)";
        case 2: return "WhiteOOO(2)";
        case 3: return "WhiteSide(3)";
        case 4: return "BlackOO(4)";
        case 5: return "WhiteOO|BlackOO(5)";
        case 6: return "WhiteOOO|BlackOO(6)";
        case 7: return "WhiteSide|BlackOO(7)";
        case 8: return "BlackOOO(8)";
        case 9: return "WhiteOO|BlackOOO(9)";
        case 10: return "WhiteOOO|BlackOOO(10)";
        case 11: return "WhiteSide|BlackOOO(11)";
        case 12: return "BlackSide(12)";
        case 13: return "WhiteOO|BlackSide(13)";
        case 14: return "WhiteOOO|BlackSide(14)";
        case 15: return "All(15)";
        default: return "Unknown";
    }
}

void appendPositionDump(std::ofstream& log, const char* heading, const Position& pos) {
    log << heading << "\n";
    for (size_t p = 0; p < 12; ++p) {
        log << "  bb[" << p << "][" << pieceLabel(p) << "]=0x"
            << std::hex << std::setw(16) << std::setfill('0')
            << pos.getPieceBitboard(static_cast<Piece>(p)) << std::dec << "\n";
    }
    for (size_t o = 0; o < 3; ++o) {
        log << "  occupancy[" << o << "]=0x"
            << std::hex << std::setw(16) << std::setfill('0')
            << pos.getColorOccupancy(static_cast<Color>(o)) << std::dec << "\n";
    }
    log << "  CastlingRights=" << castlingRightsLabel(pos.getCastlingRights())
        << " raw=" << static_cast<int>(pos.getCastlingRights()) << "\n";
    log << "  EnPassant=" << static_cast<int>(pos.getEnPassantSquare()) << "\n";
    log << "  WhiteKingSquare=" << static_cast<int>(pos.getKingSquare(Color::White)) << "\n";
    log << "  BlackKingSquare=" << static_cast<int>(pos.getKingSquare(Color::Black)) << "\n";
    log << "  SideToMove=" << (pos.getSideToMove() == Color::White ? "White" : "Black") << "\n";
    log << "  HalfmoveClock=" << pos.getHalfmoveClock() << "\n";
    log << "  FullmoveNumber=" << pos.getFullmoveNumber() << "\n";
}

void appendSnapshotDump(std::ofstream& log, const char* heading, const BoardSnapshot& snapshot) {
    log << heading << "\n";
    for (size_t p = 0; p < 12; ++p) {
        log << "  bb[" << p << "][" << pieceLabel(p) << "]=0x"
            << std::hex << std::setw(16) << std::setfill('0')
            << snapshot.pieces[p] << std::dec << "\n";
    }
    for (size_t o = 0; o < 3; ++o) {
        log << "  occupancy[" << o << "]=0x"
            << std::hex << std::setw(16) << std::setfill('0')
            << snapshot.occupancy[o] << std::dec << "\n";
    }
    log << "  CastlingRights=" << castlingRightsLabel(snapshot.castlingRights)
        << " raw=" << static_cast<int>(snapshot.castlingRights) << "\n";
    log << "  EnPassant=" << static_cast<int>(snapshot.enPassantSquare) << "\n";
    log << "  WhiteKingSquare=" << static_cast<int>(snapshot.whiteKingSquare) << "\n";
    log << "  BlackKingSquare=" << static_cast<int>(snapshot.blackKingSquare) << "\n";
    log << "  SideToMove=" << (snapshot.sideToMove == Color::White ? "White" : "Black") << "\n";
    log << "  HalfmoveClock=" << snapshot.halfmoveClock << "\n";
    log << "  FullmoveNumber=" << snapshot.fullmoveNumber << "\n";
}

void logIntegrityMismatch(std::ofstream& log,
                          const Position& pos,
                          const BoardSnapshot& expected,
                          const Move& move,
                          const char* phase) {
    log << "\n=== INTEGRITY FAILURE [" << phase << "] move=" << move.toString()
        << " flags=" << static_cast<int>(move.getFlags()) << " ===\n";

    appendSnapshotDump(log, "--- Expected snapshot ---", expected);
    appendPositionDump(log, "--- Actual Position ---", pos);

    log << "--- Field deltas ---\n";
    for (size_t p = 0; p < 12; ++p) {
        const Bitboard actual = pos.getPieceBitboard(static_cast<Piece>(p));
        if (actual != expected.pieces[p]) {
            log << "  MISMATCH piece bb[" << p << "][" << pieceLabel(p) << "]"
                << " actual=0x" << std::hex << actual
                << " expected=0x" << expected.pieces[p] << std::dec << "\n";
        }
    }
    for (size_t o = 0; o < 3; ++o) {
        const Bitboard actual = (o == 2) ? pos.getTotalOccupancy() : pos.getColorOccupancy(static_cast<Color>(o));
        if (actual != expected.occupancy[o]) {
            log << "  MISMATCH occupancy[" << o << "]"
                << " actual=0x" << std::hex << actual
                << " expected=0x" << expected.occupancy[o] << std::dec << "\n";
        }
    }
    if (pos.getSideToMove() != expected.sideToMove) {
        log << "  MISMATCH SideToMove actual="
            << (pos.getSideToMove() == Color::White ? "White" : "Black")
            << " expected="
            << (expected.sideToMove == Color::White ? "White" : "Black") << "\n";
    }
    if (pos.getEnPassantSquare() != expected.enPassantSquare) {
        log << "  MISMATCH EnPassant actual=" << static_cast<int>(pos.getEnPassantSquare())
            << " expected=" << static_cast<int>(expected.enPassantSquare) << "\n";
    }
    if (pos.getCastlingRights() != expected.castlingRights) {
        log << "  MISMATCH CastlingRights actual=" << static_cast<int>(pos.getCastlingRights())
            << " expected=" << static_cast<int>(expected.castlingRights) << "\n";
    }
    if (pos.getHalfmoveClock() != expected.halfmoveClock) {
        log << "  MISMATCH HalfmoveClock actual=" << pos.getHalfmoveClock()
            << " expected=" << expected.halfmoveClock << "\n";
    }
    if (pos.getFullmoveNumber() != expected.fullmoveNumber) {
        log << "  MISMATCH FullmoveNumber actual=" << pos.getFullmoveNumber()
            << " expected=" << expected.fullmoveNumber << "\n";
    }
    if (pos.getKingSquare(Color::White) != expected.whiteKingSquare) {
        log << "  MISMATCH WhiteKingSquare actual=" << static_cast<int>(pos.getKingSquare(Color::White))
            << " expected=" << static_cast<int>(expected.whiteKingSquare) << "\n";
    }
    if (pos.getKingSquare(Color::Black) != expected.blackKingSquare) {
        log << "  MISMATCH BlackKingSquare actual=" << static_cast<int>(pos.getKingSquare(Color::Black))
            << " expected=" << static_cast<int>(expected.blackKingSquare) << "\n";
    }
    log << "=== END FAILURE ===\n";
}

} // namespace

bool verifyBoardIntegrity(const Position& pos, const BoardSnapshot& expected) noexcept {
    if (!pos.matchesSnapshot(expected)) {
        return false;
    }

    Position scratch = pos;
    scratch.updateOccupancy();

    if (scratch.getKingSquare(Color::White) != expected.whiteKingSquare) return false;
    if (scratch.getKingSquare(Color::Black) != expected.blackKingSquare) return false;
    if (scratch.getTotalOccupancy() != pos.getTotalOccupancy()) return false;
    if (scratch.getColorOccupancy(Color::White) != pos.getColorOccupancy(Color::White)) return false;
    if (scratch.getColorOccupancy(Color::Black) != pos.getColorOccupancy(Color::Black)) return false;

    return true;
}

bool verifyMakeUndoCycle(Position& pos, const Move& move) noexcept {
    const BoardSnapshot before = pos.captureSnapshot();

    UndoState undo;
    MoveExecutor::makeMove(pos, move, undo);
    MoveExecutor::undoMove(pos, move, undo);

    if (verifyBoardIntegrity(pos, before)) {
        return true;
    }

    std::ofstream log(kDebugLogPath, std::ios::app);
    if (log) {
        appendSnapshotDump(log, "--- BEFORE makeMove ---", before);
        appendPositionDump(log, "--- AFTER undoMove ---", pos);
        logIntegrityMismatch(log, pos, before, move, "post-undo vs pre-make snapshot");
    }

    std::cout << "Integrity failure move " << move.toString()
              << " (details appended to " << kDebugLogPath << ")\n";
    return false;
}

static void collectPseudoMoves(const Position& pos, MoveList& moves) noexcept {
    MoveGenerator::generateKnightMoves(pos, moves);
    MoveGenerator::generateKingMoves(pos, moves);
    MoveGenerator::generatePawnMoves(pos, moves);
    MoveGenerator::generateSlidingMoves(pos, moves);
}

bool VerificationHarness::verifyMakeUndoIntegrity(const std::string& fen, int depth) noexcept {
    auto parsed = FenParser::parse(fen);
    if (!parsed) {
        std::cout << "verifyMakeUndoIntegrity: invalid FEN\n";
        return false;
    }

    {
        std::ofstream log(kDebugLogPath, std::ios::trunc);
        log << "=== Make/Undo Integrity Run ===\n";
        log << "FEN: " << fen << "\n";
        log << "Depth: " << depth << "\n";
    }

    struct Node {
        BoardSnapshot snapshot;
        int ply{0};
    };

    std::vector<Node> stack;
    stack.push_back({parsed->captureSnapshot(), 0});

    uint64_t checked = 0;
    uint64_t failed = 0;

    while (!stack.empty()) {
        Node node = stack.back();
        stack.pop_back();

        Position pos;
        pos.clearState();
        for (size_t p = 0; p < 12; ++p) {
            Bitboard bb = node.snapshot.pieces[p];
            while (bb) {
                unsigned long sq = 0;
#if defined(_MSC_VER)
                _BitScanForward64(&sq, bb);
#else
                sq = static_cast<unsigned long>(__builtin_ctzll(bb));
#endif
                pos.setPieceBit(static_cast<Square>(sq), static_cast<Piece>(p));
                bb &= bb - 1;
            }
        }
        pos.setSideToMove(node.snapshot.sideToMove);
        pos.setEnPassantSquare(node.snapshot.enPassantSquare);
        pos.setCastlingRights(node.snapshot.castlingRights);
        pos.setHalfmoveClock(node.snapshot.halfmoveClock);
        pos.setFullmoveNumber(node.snapshot.fullmoveNumber);
        pos.updateOccupancy();

        if (node.ply >= depth) {
            continue;
        }

        MoveList pseudo;
        collectPseudoMoves(pos, pseudo);

        for (int i = 0; i < pseudo.size(); ++i) {
            const Move& move = pseudo[i];
            ++checked;
            if (!verifyMakeUndoCycle(pos, move)) {
                ++failed;
            }

            UndoState undo;
            MoveExecutor::makeMove(pos, move, undo);

            Node child;
            child.snapshot = pos.captureSnapshot();
            child.ply = node.ply + 1;
            stack.push_back(child);

            MoveExecutor::undoMove(pos, move, undo);
        }
    }

    std::cout << "Make/undo integrity (" << fen << ", depth " << depth << "): "
              << checked << " cycles, " << failed << " failures\n";
    return failed == 0;
}

bool VerificationHarness::runComprehensivePerft() noexcept {
    std::cout << "\n=================================================================\n";
    std::cout << "🏛️  BOSON MODULE Ω.1 — RUNNING ALL-IN-ONE REALITY STRESS MATRIX\n";
    std::cout << "=================================================================\n";
    
    struct LocalTestCase {
        std::string fenString;
        int targetDepth;
        uint64_t nodesExpected;
        std::string testLabel;
    };

    const std::vector<LocalTestCase> realitySuite = {
        { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3, 8902ULL, "Pos 1: Initial Position" },
        { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281ULL, "Pos 1: Initial Position" },
        { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862ULL, "Pos 2: Kiwipete" },
        { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603ULL, "Pos 2: Kiwipete" },
        { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 3, 2812ULL, "Pos 3: CPW Open Field" },
        { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238ULL, "Pos 3: CPW Open Field" },
        { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 2", 3, 9467ULL, "Pos 4: Mirror Intersect" },
        { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 2", 4, 422333ULL, "Pos 4: Mirror Intersect" },
        { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379ULL, "Pos 5: CPW Evasions" },
        { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2103487ULL, "Pos 5: CPW Evasions" }
    };

    std::atomic<bool> allPassed{true};
    std::mutex coutMutex;

    std::for_each(std::execution::seq, realitySuite.begin(), realitySuite.end(), [&](const auto& test) {
        auto pos = FenParser::parse(test.fenString);
        uint64_t actualNodes = Search::perft(*pos, test.targetDepth);
        
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "  -> Evaluated: " << std::left << std::setw(32) << test.testLabel 
                  << " (Depth " << test.targetDepth << ")... ";
        
        if (actualNodes == test.nodesExpected) {
            std::cout << "✅ PASSED\n";
        } else {
            std::cout << "❌ MISMATCH!\n"
                      << "     Expected: " << test.nodesExpected << "\n"
                      << "     Actual:   " << actualNodes << "\n";
            allPassed.store(false, std::memory_order_relaxed);
        }
    });

    return allPassed.load();
}

void VerificationHarness::debugPerft(const std::string& fen, int depth) {
    auto pos = FenParser::parse(fen);
    MoveList moves;
    MoveGenerator::generateLegalMoves(*pos, moves);
    
    std::cout << "--- PERFT DIVIDE FOR: " << fen << " ---" << std::endl;
    for (size_t i = 0; i < moves.size(); ++i) {
        UndoState undo;
        MoveExecutor::makeMove(*pos, moves[i], undo);
        uint64_t nodes = Search::perft(*pos, depth - 1);
        std::cout << moves[i].toString() << ": " << nodes << std::endl;
        MoveExecutor::undoMove(*pos, moves[i], undo);
    }
}

void VerificationHarness::executePerformanceBenchmark() noexcept {}

} // namespace Boson
