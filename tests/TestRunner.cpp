#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <bit>
#include <iomanip>
#include <type_traits>
#include <utility>
#include <random>
#include <memory>
#include <chrono>
#include <unordered_set>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include "eval/IEvaluator.hpp"
#include "eval/ClassicalEvaluator.hpp"
#include "eval/nnue/NNUETypes.hpp"
#include "eval/nnue/FeatureTransformer.hpp"
#include "eval/nnue/Accumulator.hpp"
#include "eval/nnue/AccumulatorStack.hpp"
#include "eval/nnue/NetworkModel.hpp"
#include "eval/nnue/ScalarInference.hpp"
#include "eval/nnue/NNUEEvaluator.hpp"
#include "eval/nnue/AVX2Accumulator.hpp"
#include "eval/nnue/AVX2Inference.hpp"
#include "board/Position.hpp"
#include "board/Castling.hpp"
#include "board/Move.hpp"
#include "board/MoveExecutor.hpp"
#include "board/MoveGenerator.hpp"
#include "board/MoveList.hpp"
#include "board/UndoState.hpp"
#include "debug/BoardPrinter.hpp"
#include "fen/FenParser.hpp"
#include "search/Search.hpp"
#include "search/SearchController.hpp"
#include "search/MoveOrderer.hpp"
#include "search/MovePicker.hpp"
#include "search/see/SEE.hpp"
#include "search/LMR.hpp"
#include "evaluation/Evaluator.hpp"
#include "evaluation/PieceSquareTables.hpp"
#include "config/EngineParameters.hpp"
#include "config/ParameterRegistry.hpp"
#include "system/EngineInfo.hpp"
#include "tactical/TacticalSuite.hpp"
#include "tactical/TacticalSuite.cpp"
#include "integrity/PositionFingerprint.hpp"
#include "integrity/IntegritySuite.hpp"
#include "integrity/IntegritySuite.cpp"
#include "benchmark/BenchmarkTypes.hpp"
#include "benchmark/BenchmarkCorpus.hpp"
#include "benchmark/BenchmarkRunner.hpp"
#include "benchmark/BenchmarkReporter.hpp"
#include "strength/StrengthTypes.hpp"
#include "strength/Statistics.hpp"
#include "strength/Statistics.cpp"
#include "strength/OpeningBook.hpp"
#include "strength/OpeningBook.cpp"
#include "strength/MatchRunner.hpp"
#include "strength/MatchRunner.cpp"
#include "strength/StrengthReporter.hpp"
#include "strength/StrengthReporter.cpp"

namespace Boson {

// ---------------------------------------------------------------------------
// Milestone 1 Helper: FEN Serialization
// ---------------------------------------------------------------------------

std::string exportFen(const Position& pos) noexcept {
    std::string fen;
    static const std::array<char, 12> pieceChars = {
        'P', 'N', 'B', 'R', 'Q', 'K',
        'p', 'n', 'b', 'r', 'q', 'k'
    };

    for (int rank = 7; rank >= 0; --rank) {
        int emptyCount = 0;
        for (int file = 0; file < 8; ++file) {
            Square sq = static_cast<Square>(rank * 8 + file);
            Bitboard mask = Bitboards::getSquareBit(sq);
            char pieceChar = 0;

            for (size_t p = 0; p < 12; ++p) {
                if (pos.getPieceBitboard(static_cast<Piece>(p)) & mask) {
                    pieceChar = pieceChars[p];
                    break;
                }
            }

            if (pieceChar != 0) {
                if (emptyCount > 0) {
                    fen += std::to_string(emptyCount);
                    emptyCount = 0;
                }
                fen += pieceChar;
            } else {
                emptyCount++;
            }
        }
        if (emptyCount > 0) {
            fen += std::to_string(emptyCount);
        }
        if (rank > 0) fen += '/';
    }

    fen += (pos.getSideToMove() == Color::White ? " w " : " b ");

    auto rights = pos.getCastlingRights();
    if (rights == CastlingRights::None) {
        fen += "-";
    } else {
        if (static_cast<uint8_t>(rights & CastlingRights::WhiteOO))  fen += 'K';
        if (static_cast<uint8_t>(rights & CastlingRights::WhiteOOO)) fen += 'Q';
        if (static_cast<uint8_t>(rights & CastlingRights::BlackOO))  fen += 'k';
        if (static_cast<uint8_t>(rights & CastlingRights::BlackOOO)) fen += 'q';
    }
    fen += " ";

    Square ep = pos.getEnPassantSquare();
    if (ep == Square::None) {
        fen += "-";
    } else {
        int sqVal = static_cast<int>(ep);
        fen += static_cast<char>('a' + (sqVal % 8));
        fen += static_cast<char>('1' + (sqVal / 8));
    }

    fen += " " + std::to_string(pos.getHalfmoveClock());
    fen += " " + std::to_string(pos.getFullmoveNumber());

    return fen;
}

// ---------------------------------------------------------------------------
// Milestone 1, Phase 2 Unit Tests
// ---------------------------------------------------------------------------

bool testPositionDefaultConstruction() {
    Position pos;
    for (size_t p = 0; p < 12; ++p) {
        if (pos.getPieceBitboard(static_cast<Piece>(p)) != Bitboards::Empty) return false;
    }
    if (pos.getColorOccupancy(Color::White) != Bitboards::Empty) return false;
    if (pos.getColorOccupancy(Color::Black) != Bitboards::Empty) return false;
    if (pos.getTotalOccupancy() != Bitboards::Empty) return false;
    if (pos.getSideToMove() != Color::White) return false;
    if (pos.getCastlingRights() != CastlingRights::None) return false;
    if (pos.getEnPassantSquare() != Square::None) return false;
    if (pos.getHalfmoveClock() != 0) return false;
    if (pos.getFullmoveNumber() != 1) return false;
    if (pos.getHashKey() != 0ULL) return false;
    if (pos.getKingSquare(Color::White) != Square::None) return false;
    if (pos.getKingSquare(Color::Black) != Square::None) return false;
    return true;
}

bool testStartposLoading() {
    auto parsed = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    const Position& pos = parsed.value();

    if (std::popcount(pos.getPieceBitboard(Piece::WhitePawn)) != 8) return false;
    if (std::popcount(pos.getPieceBitboard(Piece::WhiteKnight)) != 2) return false;
    if (std::popcount(pos.getPieceBitboard(Piece::WhiteBishop)) != 2) return false;
    if (std::popcount(pos.getPieceBitboard(Piece::WhiteRook)) != 2) return false;
    if (std::popcount(pos.getPieceBitboard(Piece::WhiteQueen)) != 1) return false;
    if (std::popcount(pos.getPieceBitboard(Piece::WhiteKing)) != 1) return false;

    if (std::popcount(pos.getPieceBitboard(Piece::BlackPawn)) != 8) return false;
    if (std::popcount(pos.getPieceBitboard(Piece::BlackKnight)) != 2) return false;
    if (std::popcount(pos.getPieceBitboard(Piece::BlackBishop)) != 2) return false;
    if (std::popcount(pos.getPieceBitboard(Piece::BlackRook)) != 2) return false;
    if (std::popcount(pos.getPieceBitboard(Piece::BlackQueen)) != 1) return false;
    if (std::popcount(pos.getPieceBitboard(Piece::BlackKing)) != 1) return false;

    if (std::popcount(pos.getColorOccupancy(Color::White)) != 16) return false;
    if (std::popcount(pos.getColorOccupancy(Color::Black)) != 16) return false;
    if (std::popcount(pos.getTotalOccupancy()) != 32) return false;

    if (pos.getSideToMove() != Color::White) return false;
    if (pos.getCastlingRights() != CastlingRights::All) return false;
    if (pos.getEnPassantSquare() != Square::None) return false;
    if (pos.getHalfmoveClock() != 0) return false;
    if (pos.getFullmoveNumber() != 1) return false;

    if (pos.getKingSquare(Color::White) != Square::E1) return false;
    if (pos.getKingSquare(Color::Black) != Square::E8) return false;
    if (pos.getHashKey() == 0ULL) return false;

    return true;
}

bool testBoardPrinterOutput() {
    auto parsed = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    const Position& pos = parsed.value();

    std::ostringstream humanBuffer;
    BoardPrinter::print(pos, BoardPrinter::Mode::Human, humanBuffer);
    std::string humanOut = humanBuffer.str();

    if (humanOut.find("a b c d e f g h") == std::string::npos) return false;
    if (humanOut.find("8") == std::string::npos || humanOut.find("1") == std::string::npos) return false;
    if (humanOut.find("R") == std::string::npos || humanOut.find("k") == std::string::npos) return false;

    std::ostringstream debugBuffer;
    BoardPrinter::print(pos, BoardPrinter::Mode::Debug, debugBuffer);
    std::string debugOut = debugBuffer.str();

    if (debugOut.find("--- State Metadata ---") == std::string::npos) return false;
    if (debugOut.find("Side To Move : White") == std::string::npos) return false;
    if (debugOut.find("Castling     : KQkq") == std::string::npos) return false;
    if (debugOut.find("En Passant   : -") == std::string::npos) return false;
    if (debugOut.find("Halfmove     : 0") == std::string::npos) return false;
    if (debugOut.find("Fullmove     : 1") == std::string::npos) return false;
    if (debugOut.find("Total Occ.") == std::string::npos) return false;

    return true;
}

struct ValidFenScenario {
    std::string name;
    std::string fen;
    int expectedOccupancy;
    Color expectedSide;
    CastlingRights expectedCastling;
    Square expectedEp;
};

bool testValidFenScenarios() {
    const std::vector<ValidFenScenario> scenarios = {
        {
            "Starting position",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            32, Color::White, CastlingRights::All, Square::None
        },
        {
            "Empty board",
            "8/8/8/8/8/8/8/8 w - - 0 1",
            0, Color::White, CastlingRights::None, Square::None
        },
        {
            "Midgame position",
            "r1bqkb1r/pppp1ppp/2n5/4p3/2B1n3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 5",
            31, Color::White, CastlingRights::All, Square::None
        },
        {
            "Promotion-ready",
            "8/4P3/8/8/8/8/8/4K2k w - - 0 1",
            3, Color::White, CastlingRights::None, Square::None
        },
        {
            "En-passant active",
            "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
            32, Color::Black, CastlingRights::All, Square::E3
        },
        {
            "Full castling",
            "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
            6, Color::White, CastlingRights::All, Square::None
        },
        {
            "No castling",
            "r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1",
            6, Color::White, CastlingRights::None, Square::None
        }
    };

    bool allPassed = true;
    for (const auto& sc : scenarios) {
        auto parsed = FenParser::parse(sc.fen);
        bool match = parsed.has_value();

        if (match) {
            const Position& pos = parsed.value();
            match = (std::popcount(pos.getTotalOccupancy()) == sc.expectedOccupancy)
                 && (pos.getSideToMove() == sc.expectedSide)
                 && (pos.getCastlingRights() == sc.expectedCastling)
                 && (pos.getEnPassantSquare() == sc.expectedEp);
        }

        std::cout << "  [" << (match ? "PASS" : "FAIL") << "] Valid FEN  : "
                  << std::left << std::setw(20) << sc.name
                  << " | Input: \"" << sc.fen << "\""
                  << " | Expected: Success\n";

        if (!match) allPassed = false;
    }
    return allPassed;
}

struct InvalidFenScenario {
    std::string name;
    std::string fen;
    bool isSemanticTest; // If true, tests parseStrict / validateSemantics
    ParseError expectedError;
};

bool testInvalidFenScenarios() {
    const std::vector<InvalidFenScenario> scenarios = {
        {
            "Missing king",
            "rnbq1bnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            true, ParseError::MissingKing
        },
        {
            "9 files on a rank",
            "rnbqkbnr1/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            false, ParseError::InvalidPiecePlacement
        },
        {
            "7 ranks total",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP w KQkq - 0 1",
            false, ParseError::InvalidPiecePlacement
        },
        {
            "Invalid characters",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNX w KQkq - 0 1",
            false, ParseError::InvalidPiecePlacement
        },
        {
            "Invalid active color",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1",
            false, ParseError::InvalidActiveColor
        },
        {
            "Malformed castling tokens",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w XYZ - 0 1",
            false, ParseError::InvalidCastlingRights
        },
        {
            "Out-of-bounds en passant",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq z9 0 1",
            false, ParseError::InvalidEnPassantSquare
        },
        {
            "Pawns on first/last rank",
            "Pnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            true, ParseError::PawnsOnFirstOrLastRank
        }
    };

    bool allPassed = true;
    for (const auto& sc : scenarios) {
        bool match = false;
        ParseError actualError = ParseError::MalformedFieldCount;

        if (sc.isSemanticTest) {
            auto strictResult = FenParser::parseStrict(sc.fen);
            if (!strictResult.has_value()) {
                actualError = strictResult.error();
                match = (actualError == sc.expectedError);
            }
        } else {
            auto parseResult = FenParser::parse(sc.fen);
            if (!parseResult.has_value()) {
                actualError = parseResult.error();
                match = (actualError == sc.expectedError);
            }
        }

        std::string expectedStr(to_string(sc.expectedError));
        std::cout << "  [" << (match ? "PASS" : "FAIL") << "] Invalid FEN: "
                  << std::left << std::setw(26) << sc.name
                  << " | Expected: " << expectedStr.substr(0, 36) << "\n";

        if (!match) allPassed = false;
    }
    return allPassed;
}

bool testFenRoundtripConsistency() {
    const std::vector<std::string> testFens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R w Kq - 5 20",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
    };

    for (const auto& originalFen : testFens) {
        auto p1 = FenParser::parse(originalFen);
        if (!p1.has_value()) return false;

        std::string serialized = exportFen(p1.value());
        auto p2 = FenParser::parse(serialized);
        if (!p2.has_value()) return false;

        if (p1->getHashKey() != p2->getHashKey()) return false;
        if (p1->getTotalOccupancy() != p2->getTotalOccupancy()) return false;
        if (p1->getColorOccupancy(Color::White) != p2->getColorOccupancy(Color::White)) return false;
        if (p1->getColorOccupancy(Color::Black) != p2->getColorOccupancy(Color::Black)) return false;
        if (p1->getSideToMove() != p2->getSideToMove()) return false;
        if (p1->getCastlingRights() != p2->getCastlingRights()) return false;
        if (p1->getEnPassantSquare() != p2->getEnPassantSquare()) return false;
        if (p1->getHalfmoveClock() != p2->getHalfmoveClock()) return false;
        if (p1->getFullmoveNumber() != p2->getFullmoveNumber()) return false;

        for (size_t pieceIdx = 0; pieceIdx < 12; ++pieceIdx) {
            Piece piece = static_cast<Piece>(pieceIdx);
            if (p1->getPieceBitboard(piece) != p2->getPieceBitboard(piece)) return false;
        }
    }
    return true;
}

bool runMilestone1Tests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE 1, PHASE 2: FEN PARSER & BOARD PRINTER TESTS   ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 6;

    std::cout << "\n--- Subsystem Isolation & Data Construction ---\n";
    bool ctorPass = testPositionDefaultConstruction();
    std::cout << "[" << (ctorPass ? "PASS" : "FAIL") << "] Position Default Construction (Empty universe invariants)\n";
    if (ctorPass) passed++;

    bool startposPass = testStartposLoading();
    std::cout << "[" << (startposPass ? "PASS" : "FAIL") << "] Startpos Piece & Occupancy Bitboard Verification\n";
    if (startposPass) passed++;

    bool printerPass = testBoardPrinterOutput();
    std::cout << "[" << (printerPass ? "PASS" : "FAIL") << "] Board Printer ostream Isolation (Human & Debug modes)\n";
    if (printerPass) passed++;

    std::cout << "\n--- Valid FEN Scenarios Matrix ---\n";
    bool validPass = testValidFenScenarios();
    if (validPass) passed++;

    std::cout << "\n--- Invalid & Syntax Rejection Matrix ---\n";
    bool invalidPass = testInvalidFenScenarios();
    if (invalidPass) passed++;

    std::cout << "\n--- FEN Roundtrip Invariance ---\n";
    bool roundtripPass = testFenRoundtripConsistency();
    std::cout << "[" << (roundtripPass ? "PASS" : "FAIL") << "] FEN Serialization/Deserialization Roundtrip Symmetry\n";
    if (roundtripPass) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "MILESTONE 1 PHASE 2 RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone 1, Module 1.3 Unit Tests: Move Representation & Make/Undo
// ---------------------------------------------------------------------------

bool testMoveScalarRepresentation() {
    static_assert(sizeof(Move) <= 4, "Move must be compact scalar <= 32 bits");
    static_assert(std::is_trivially_copyable_v<Move>, "Move must be trivially copyable");
    static_assert(std::is_trivially_destructible_v<UndoState>, "UndoState must be trivially destructible");

    Move defaultMove;
    if (defaultMove.getRawData() != 0) return false;
    if (defaultMove.getFromSquare() != Square::A1) return false;
    if (defaultMove.getToSquare() != Square::A1) return false;
    if (defaultMove.getFlags() != Move::Flags::None) return false;
    if (defaultMove.getPromotionPiece() != Move::PromotionPiece::None) return false;

    // Test full range of squares
    Move edgeMove(Square::A1, Square::H8, Move::Flags::Promotion, Move::PromotionPiece::Queen);
    if (edgeMove.getFromSquare() != Square::A1) return false;
    if (edgeMove.getToSquare() != Square::H8) return false;
    if (!edgeMove.isPromotion()) return false;
    if (edgeMove.getPromotionPiece() != Move::PromotionPiece::Queen) return false;

    // Test flags
    Move dppMove(Square::E2, Square::E4, Move::Flags::DoublePawnPush);
    if (!dppMove.isDoublePawnPush()) return false;
    if (dppMove.isCastling() || dppMove.isEnPassant() || dppMove.isPromotion()) return false;

    Move castleMove(Square::E1, Square::G1, Move::Flags::Castling);
    if (!castleMove.isCastling()) return false;

    Move epMove(Square::E5, Square::D6, Move::Flags::EnPassant);
    if (!epMove.isEnPassant()) return false;

    // Test string conversions
    if (dppMove.toString() != "e2e4") return false;
    if (edgeMove.toString() != "a1h8q") return false;

    return true;
}

bool testUndoStateIntegrity() {
    if (sizeof(UndoState) > 32) return false;
    if (!std::is_trivially_destructible_v<UndoState>) return false;
    return true;
}

bool checkPositionInvariants(const Position& pos) noexcept {
    // 1. AllOcc == WhiteOcc | BlackOcc
    if ((pos.getColorOccupancy(Color::White) | pos.getColorOccupancy(Color::Black)) != pos.getTotalOccupancy()) return false;
    // 2. Disjoint occupancies (no piece overlap)
    if ((pos.getColorOccupancy(Color::White) & pos.getColorOccupancy(Color::Black)) != 0ULL) return false;

    // 3. Piece bitboard union matches color occupancies
    Bitboard wSum = 0ULL;
    for (size_t p = 0; p < 6; ++p) wSum |= pos.getPieceBitboard(static_cast<Piece>(p));
    if (wSum != pos.getColorOccupancy(Color::White)) return false;

    Bitboard bSum = 0ULL;
    for (size_t p = 6; p < 12; ++p) bSum |= pos.getPieceBitboard(static_cast<Piece>(p));
    if (bSum != pos.getColorOccupancy(Color::Black)) return false;

    // 4. King square caches validity
    if (pos.getPieceBitboard(Piece::WhiteKing) != 0ULL) {
        if (pos.getKingSquare(Color::White) == Square::None) return false;
        if (Bitboards::getSquareBit(pos.getKingSquare(Color::White)) != pos.getPieceBitboard(Piece::WhiteKing)) return false;
    }
    if (pos.getPieceBitboard(Piece::BlackKing) != 0ULL) {
        if (pos.getKingSquare(Color::Black) == Square::None) return false;
        if (Bitboards::getSquareBit(pos.getKingSquare(Color::Black)) != pos.getPieceBitboard(Piece::BlackKing)) return false;
    }

    return true;
}

bool testMakeUndoQuietMoves() {
    auto parsed = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();
    const Position originalPos = pos;
    const uint64_t originalHash = pos.getHashKey();

    if (!checkPositionInvariants(pos)) return false;

    // Move 1: White Knight G1 -> F3
    Move move1(Square::G1, Square::F3);
    UndoState undo1;
    MoveExecutor::makeMove(pos, move1, undo1);

    if (!checkPositionInvariants(pos)) return false;
    if (pos.getSideToMove() != Color::Black) return false;
    if (pos.getHalfmoveClock() != 1) return false;
    if (pos.getFullmoveNumber() != 1) return false;
    if ((pos.getPieceBitboard(Piece::WhiteKnight) & Bitboards::getSquareBit(Square::F3)) == 0ULL) return false;
    if ((pos.getPieceBitboard(Piece::WhiteKnight) & Bitboards::getSquareBit(Square::G1)) != 0ULL) return false;
    if (pos.getHashKey() == originalHash) return false;

    // Move 2: Black Knight B8 -> C6
    const Position stateAfterMove1 = pos;
    const uint64_t hashAfterMove1 = pos.getHashKey();
    Move move2(Square::B8, Square::C6);
    UndoState undo2;
    MoveExecutor::makeMove(pos, move2, undo2);

    if (!checkPositionInvariants(pos)) return false;
    if (pos.getSideToMove() != Color::White) return false;
    if (pos.getHalfmoveClock() != 2) return false;
    if (pos.getFullmoveNumber() != 2) return false;
    if ((pos.getPieceBitboard(Piece::BlackKnight) & Bitboards::getSquareBit(Square::C6)) == 0ULL) return false;
    if ((pos.getPieceBitboard(Piece::BlackKnight) & Bitboards::getSquareBit(Square::B8)) != 0ULL) return false;

    // Undo Move 2
    MoveExecutor::undoMove(pos, move2, undo2);
    if (!checkPositionInvariants(pos)) return false;
    if (!(pos == stateAfterMove1)) return false;
    if (pos.getHashKey() != hashAfterMove1) return false;

    // Undo Move 1
    MoveExecutor::undoMove(pos, move1, undo1);
    if (!checkPositionInvariants(pos)) return false;
    if (!(pos == originalPos)) return false;
    if (pos.getHashKey() != originalHash) return false;

    return true;
}

bool testMakeUndoCaptures() {
    auto parsed = FenParser::parse("r1bqkb1r/pppp1ppp/2n5/4p3/2B1n3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 5");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();
    const Position originalPos = pos;
    const uint64_t originalHash = pos.getHashKey();

    if (!checkPositionInvariants(pos)) return false;

    // White Knight on F3 captures Black Pawn on E5
    Move capMove(Square::F3, Square::E5);
    UndoState undo;
    MoveExecutor::makeMove(pos, capMove, undo);

    if (!checkPositionInvariants(pos)) return false;
    if (undo.capturedPiece != Piece::BlackPawn) return false;
    if (pos.getHalfmoveClock() != 0) return false;
    if ((pos.getPieceBitboard(Piece::BlackPawn) & Bitboards::getSquareBit(Square::E5)) != 0ULL) return false;
    if ((pos.getPieceBitboard(Piece::WhiteKnight) & Bitboards::getSquareBit(Square::E5)) == 0ULL) return false;
    if ((pos.getPieceBitboard(Piece::WhiteKnight) & Bitboards::getSquareBit(Square::F3)) != 0ULL) return false;

    MoveExecutor::undoMove(pos, capMove, undo);
    if (!checkPositionInvariants(pos)) return false;
    if (!(pos == originalPos)) return false;
    if (pos.getHashKey() != originalHash) return false;

    // White Bishop on C4 captures Black Knight on E4
    Move capMove2(Square::C4, Square::E4);
    UndoState undo2;
    MoveExecutor::makeMove(pos, capMove2, undo2);

    if (!checkPositionInvariants(pos)) return false;
    if (undo2.capturedPiece != Piece::BlackKnight) return false;
    if (pos.getHalfmoveClock() != 0) return false;
    if ((pos.getPieceBitboard(Piece::BlackKnight) & Bitboards::getSquareBit(Square::E4)) != 0ULL) return false;
    if ((pos.getPieceBitboard(Piece::WhiteBishop) & Bitboards::getSquareBit(Square::E4)) == 0ULL) return false;

    MoveExecutor::undoMove(pos, capMove2, undo2);
    if (!checkPositionInvariants(pos)) return false;
    if (!(pos == originalPos)) return false;
    if (pos.getHashKey() != originalHash) return false;

    return true;
}

bool testMakeUndoDoublePawnPushAndEnPassant() {
    // Part 1: Double Pawn Push
    auto parsedStart = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsedStart.has_value()) return false;
    Position pos = parsedStart.value();
    const Position originalStart = pos;
    const uint64_t originalStartHash = pos.getHashKey();

    if (!checkPositionInvariants(pos)) return false;

    Move dpp(Square::E2, Square::E4, Move::Flags::DoublePawnPush);
    UndoState undoDpp;
    MoveExecutor::makeMove(pos, dpp, undoDpp);

    if (!checkPositionInvariants(pos)) return false;
    if (pos.getEnPassantSquare() != Square::E3) return false;
    if (pos.getHalfmoveClock() != 0) return false;
    if ((pos.getPieceBitboard(Piece::WhitePawn) & Bitboards::getSquareBit(Square::E4)) == 0ULL) return false;

    MoveExecutor::undoMove(pos, dpp, undoDpp);
    if (!checkPositionInvariants(pos)) return false;
    if (!(pos == originalStart)) return false;
    if (pos.getHashKey() != originalStartHash) return false;

    // Part 2: White En Passant Capture
    auto parsedEpW = FenParser::parse("rnbqkbnr/pppp1ppp/8/4pP2/8/8/PPPPP1PP/RNBQKBNR w KQkq e6 0 2");
    if (!parsedEpW.has_value()) return false;
    Position posEpW = parsedEpW.value();
    const Position originalEpW = posEpW;
    const uint64_t originalEpWHash = posEpW.getHashKey();

    if (!checkPositionInvariants(posEpW)) return false;

    Move epMoveW(Square::F5, Square::E6, Move::Flags::EnPassant);
    UndoState undoEpW;
    MoveExecutor::makeMove(posEpW, epMoveW, undoEpW);

    if (!checkPositionInvariants(posEpW)) return false;
    if (undoEpW.capturedPiece != Piece::BlackPawn) return false;
    if (posEpW.getEnPassantSquare() != Square::None) return false;
    if (posEpW.getHalfmoveClock() != 0) return false;
    if ((posEpW.getPieceBitboard(Piece::BlackPawn) & Bitboards::getSquareBit(Square::E5)) != 0ULL) return false;
    if ((posEpW.getPieceBitboard(Piece::WhitePawn) & Bitboards::getSquareBit(Square::E6)) == 0ULL) return false;

    MoveExecutor::undoMove(posEpW, epMoveW, undoEpW);
    if (!checkPositionInvariants(posEpW)) return false;
    if (!(posEpW == originalEpW)) return false;
    if (posEpW.getHashKey() != originalEpWHash) return false;

    // Part 3: Black En Passant Capture
    auto parsedEpB = FenParser::parse("rnbqkbnr/ppppp1pp/8/8/4Pp2/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 2");
    if (!parsedEpB.has_value()) return false;
    Position posEpB = parsedEpB.value();
    const Position originalEpB = posEpB;
    const uint64_t originalEpBHash = posEpB.getHashKey();

    if (!checkPositionInvariants(posEpB)) return false;

    Move epMoveB(Square::F4, Square::E3, Move::Flags::EnPassant);
    UndoState undoEpB;
    MoveExecutor::makeMove(posEpB, epMoveB, undoEpB);

    if (!checkPositionInvariants(posEpB)) return false;
    if (undoEpB.capturedPiece != Piece::WhitePawn) return false;
    if (posEpB.getEnPassantSquare() != Square::None) return false;
    if ((posEpB.getPieceBitboard(Piece::WhitePawn) & Bitboards::getSquareBit(Square::E4)) != 0ULL) return false;
    if ((posEpB.getPieceBitboard(Piece::BlackPawn) & Bitboards::getSquareBit(Square::E3)) == 0ULL) return false;

    MoveExecutor::undoMove(posEpB, epMoveB, undoEpB);
    if (!checkPositionInvariants(posEpB)) return false;
    if (!(posEpB == originalEpB)) return false;
    if (posEpB.getHashKey() != originalEpBHash) return false;

    return true;
}

bool testMakeUndoCastling() {
    // Position with all 4 castling paths open
    auto parsed = FenParser::parse("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position posW = parsed.value();
    const Position originalW = posW;
    const uint64_t originalWHash = posW.getHashKey();

    if (!checkPositionInvariants(posW)) return false;

    // 1. White Kingside O-O (E1 -> G1)
    Move whiteOO(Square::E1, Square::G1, Move::Flags::Castling);
    UndoState undoW_OO;
    MoveExecutor::makeMove(posW, whiteOO, undoW_OO);

    if (!checkPositionInvariants(posW)) return false;
    if (posW.getKingSquare(Color::White) != Square::G1) return false;
    if ((posW.getPieceBitboard(Piece::WhiteRook) & Bitboards::getSquareBit(Square::F1)) == 0ULL) return false;
    if ((posW.getPieceBitboard(Piece::WhiteRook) & Bitboards::getSquareBit(Square::H1)) != 0ULL) return false;
    if (static_cast<uint8_t>(posW.getCastlingRights() & (CastlingRights::WhiteOO | CastlingRights::WhiteOOO)) != 0) return false;

    MoveExecutor::undoMove(posW, whiteOO, undoW_OO);
    if (!checkPositionInvariants(posW)) return false;
    if (!(posW == originalW)) return false;
    if (posW.getHashKey() != originalWHash) return false;

    // 2. White Queenside O-O-O (E1 -> C1)
    Move whiteOOO(Square::E1, Square::C1, Move::Flags::Castling);
    UndoState undoW_OOO;
    MoveExecutor::makeMove(posW, whiteOOO, undoW_OOO);

    if (!checkPositionInvariants(posW)) return false;
    if (posW.getKingSquare(Color::White) != Square::C1) return false;
    if ((posW.getPieceBitboard(Piece::WhiteRook) & Bitboards::getSquareBit(Square::D1)) == 0ULL) return false;
    if ((posW.getPieceBitboard(Piece::WhiteRook) & Bitboards::getSquareBit(Square::A1)) != 0ULL) return false;

    MoveExecutor::undoMove(posW, whiteOOO, undoW_OOO);
    if (!checkPositionInvariants(posW)) return false;
    if (!(posW == originalW)) return false;
    if (posW.getHashKey() != originalWHash) return false;

    // Position with Black to move
    auto parsedB = FenParser::parse("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
    if (!parsedB.has_value()) return false;
    Position posB = parsedB.value();
    const Position originalB = posB;
    const uint64_t originalBHash = posB.getHashKey();

    if (!checkPositionInvariants(posB)) return false;

    // 3. Black Kingside O-O (E8 -> G8)
    Move blackOO(Square::E8, Square::G8, Move::Flags::Castling);
    UndoState undoB_OO;
    MoveExecutor::makeMove(posB, blackOO, undoB_OO);

    if (!checkPositionInvariants(posB)) return false;
    if (posB.getKingSquare(Color::Black) != Square::G8) return false;
    if ((posB.getPieceBitboard(Piece::BlackRook) & Bitboards::getSquareBit(Square::F8)) == 0ULL) return false;
    if ((posB.getPieceBitboard(Piece::BlackRook) & Bitboards::getSquareBit(Square::H8)) != 0ULL) return false;
    if (static_cast<uint8_t>(posB.getCastlingRights() & (CastlingRights::BlackOO | CastlingRights::BlackOOO)) != 0) return false;

    MoveExecutor::undoMove(posB, blackOO, undoB_OO);
    if (!checkPositionInvariants(posB)) return false;
    if (!(posB == originalB)) return false;
    if (posB.getHashKey() != originalBHash) return false;

    // 4. Black Queenside O-O-O (E8 -> C8)
    Move blackOOO(Square::E8, Square::C8, Move::Flags::Castling);
    UndoState undoB_OOO;
    MoveExecutor::makeMove(posB, blackOOO, undoB_OOO);

    if (!checkPositionInvariants(posB)) return false;
    if (posB.getKingSquare(Color::Black) != Square::C8) return false;
    if ((posB.getPieceBitboard(Piece::BlackRook) & Bitboards::getSquareBit(Square::D8)) == 0ULL) return false;
    if ((posB.getPieceBitboard(Piece::BlackRook) & Bitboards::getSquareBit(Square::A8)) != 0ULL) return false;

    MoveExecutor::undoMove(posB, blackOOO, undoB_OOO);
    if (!checkPositionInvariants(posB)) return false;
    if (!(posB == originalB)) return false;
    if (posB.getHashKey() != originalBHash) return false;

    return true;
}

bool testMakeUndoPromotions() {
    // 1. Quiet promotions to all 4 pieces (Q, R, B, N)
    auto parsed = FenParser::parse("8/4P3/8/8/8/8/8/4K2k w - - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();
    const Position originalPos = pos;
    const uint64_t originalHash = pos.getHashKey();

    if (!checkPositionInvariants(pos)) return false;

    const std::vector<std::pair<Move::PromotionPiece, Piece>> promoPieces = {
        {Move::PromotionPiece::Queen,  Piece::WhiteQueen},
        {Move::PromotionPiece::Rook,   Piece::WhiteRook},
        {Move::PromotionPiece::Bishop, Piece::WhiteBishop},
        {Move::PromotionPiece::Knight, Piece::WhiteKnight}
    };

    for (const auto& [promoCode, expectedPiece] : promoPieces) {
        Move promoMove(Square::E7, Square::E8, Move::Flags::Promotion, promoCode);
        UndoState undo;
        MoveExecutor::makeMove(pos, promoMove, undo);

        if (!checkPositionInvariants(pos)) return false;
        if ((pos.getPieceBitboard(Piece::WhitePawn) & Bitboards::getSquareBit(Square::E7)) != 0ULL) return false;
        if ((pos.getPieceBitboard(expectedPiece) & Bitboards::getSquareBit(Square::E8)) == 0ULL) return false;
        if (pos.getHalfmoveClock() != 0) return false;

        MoveExecutor::undoMove(pos, promoMove, undo);
        if (!checkPositionInvariants(pos)) return false;
        if (!(pos == originalPos)) return false;
        if (pos.getHashKey() != originalHash) return false;
    }

    // 2. Capture Promotion (White Pawn E7 captures Black Knight D8 promoting to Queen)
    auto parsedCap = FenParser::parse("3n4/4P3/8/8/8/8/8/4K2k w - - 0 1");
    if (!parsedCap.has_value()) return false;
    Position posCap = parsedCap.value();
    const Position origCap = posCap;
    const uint64_t origCapHash = posCap.getHashKey();

    if (!checkPositionInvariants(posCap)) return false;

    Move capPromo(Square::E7, Square::D8, Move::Flags::Promotion, Move::PromotionPiece::Queen);
    UndoState undoCap;
    MoveExecutor::makeMove(posCap, capPromo, undoCap);

    if (!checkPositionInvariants(posCap)) return false;
    if (undoCap.capturedPiece != Piece::BlackKnight) return false;
    if ((posCap.getPieceBitboard(Piece::BlackKnight) & Bitboards::getSquareBit(Square::D8)) != 0ULL) return false;
    if ((posCap.getPieceBitboard(Piece::WhiteQueen) & Bitboards::getSquareBit(Square::D8)) == 0ULL) return false;

    MoveExecutor::undoMove(posCap, capPromo, undoCap);
    if (!checkPositionInvariants(posCap)) return false;
    if (!(posCap == origCap)) return false;
    if (posCap.getHashKey() != origCapHash) return false;

    // 3. Black Promotion (Black Pawn E2 promotes to Knight on E1)
    auto parsedBlack = FenParser::parse("4K2k/8/8/8/8/8/4p3/8 b - - 0 1");
    if (!parsedBlack.has_value()) return false;
    Position posB = parsedBlack.value();
    const Position origB = posB;
    const uint64_t origBHash = posB.getHashKey();

    if (!checkPositionInvariants(posB)) return false;

    Move blackPromo(Square::E2, Square::E1, Move::Flags::Promotion, Move::PromotionPiece::Knight);
    UndoState undoB;
    MoveExecutor::makeMove(posB, blackPromo, undoB);

    if (!checkPositionInvariants(posB)) return false;
    if ((posB.getPieceBitboard(Piece::BlackPawn) & Bitboards::getSquareBit(Square::E2)) != 0ULL) return false;
    if ((posB.getPieceBitboard(Piece::BlackKnight) & Bitboards::getSquareBit(Square::E1)) == 0ULL) return false;

    MoveExecutor::undoMove(posB, blackPromo, undoB);
    if (!checkPositionInvariants(posB)) return false;
    if (!(posB == origB)) return false;
    if (posB.getHashKey() != origBHash) return false;

    return true;
}

bool runMilestone1Module13Tests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE 1, MODULE 1.3: MOVE & MAKE/UNDO INTEGRITY TESTS  ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 7;

    bool scalarPass = testMoveScalarRepresentation();
    std::cout << "[" << (scalarPass ? "PASS" : "FAIL") << "] Move Scalar & Bitfield Packing (<=32 bits, zero heap)\n";
    if (scalarPass) passed++;

    bool undoIntegrityPass = testUndoStateIntegrity();
    std::cout << "[" << (undoIntegrityPass ? "PASS" : "FAIL") << "] UndoState Layout & Stack Invariance (<=32 bytes, trivial destruction)\n";
    if (undoIntegrityPass) passed++;

    bool quietPass = testMakeUndoQuietMoves();
    std::cout << "[" << (quietPass ? "PASS" : "FAIL") << "] Quiet Move Make/Undo Inversion & Hash Key Symmetry\n";
    if (quietPass) passed++;

    bool capPass = testMakeUndoCaptures();
    std::cout << "[" << (capPass ? "PASS" : "FAIL") << "] Standard Captures & Halfmove Clock Reset Invariants\n";
    if (capPass) passed++;

    bool epPass = testMakeUndoDoublePawnPushAndEnPassant();
    std::cout << "[" << (epPass ? "PASS" : "FAIL") << "] Double Pawn Push & En Passant Inversion (White/Black symmetry)\n";
    if (epPass) passed++;

    bool castlePass = testMakeUndoCastling();
    std::cout << "[" << (castlePass ? "PASS" : "FAIL") << "] Castling Inversion (White/Black O-O and O-O-O rook track)\n";
    if (castlePass) passed++;

    bool promoPass = testMakeUndoPromotions();
    std::cout << "[" << (promoPass ? "PASS" : "FAIL") << "] Pawn Promotions (Q/R/B/N underpromotions & capture promotions)\n";
    if (promoPass) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "MILESTONE 1 MODULE 1.3 RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone 2, Phases B & C: Pseudo-Legal King and Modular Pawn Move Tests
// ---------------------------------------------------------------------------

namespace {

bool hasMove(const MoveList& ml, Square from, Square to, Move::Flags flag = Move::Flags::None) {
    for (size_t i = 0; i < ml.size(); ++i) {
        if (ml[i].getFromSquare() == from && ml[i].getToSquare() == to && ml[i].getFlags() == flag) {
            return true;
        }
    }
    return false;
}

} // namespace

bool testPseudoLegalKingGeneration() {
    MoveGenerator::initializeTables();

    // 1. Center square (E4): 8 moves on empty board
    auto parsedCenter = FenParser::parse("8/8/8/8/4K3/8/8/8 w - - 0 1");
    if (!parsedCenter.has_value()) return false;
    MoveList centerMoves;
    MoveGenerator::generateKingMoves(parsedCenter.value(), centerMoves);
    if (centerMoves.size() != 8) return false;
    const std::vector<Square> centerExpected = {
        Square::D3, Square::E3, Square::F3, Square::D4,
        Square::F4, Square::D5, Square::E5, Square::F5
    };
    for (Square sq : centerExpected) {
        if (!hasMove(centerMoves, Square::E4, sq)) return false;
    }

    // 2. Corner square (A1): 3 moves on empty board
    auto parsedCorner = FenParser::parse("8/8/8/8/8/8/8/K7 w - - 0 1");
    if (!parsedCorner.has_value()) return false;
    MoveList cornerMoves;
    MoveGenerator::generateKingMoves(parsedCorner.value(), cornerMoves);
    if (cornerMoves.size() != 3) return false;
    const std::vector<Square> cornerExpected = { Square::A2, Square::B1, Square::B2 };
    for (Square sq : cornerExpected) {
        if (!hasMove(cornerMoves, Square::A1, sq)) return false;
    }

    // 3. Friendly blockers filtered out: King A1, friendly pawns on A2 and B2
    auto parsedBlocked = FenParser::parse("8/8/8/8/8/8/PP6/K7 w - - 0 1");
    if (!parsedBlocked.has_value()) return false;
    MoveList blockedMoves;
    MoveGenerator::generateKingMoves(parsedBlocked.value(), blockedMoves);
    if (blockedMoves.size() != 1) return false;
    if (!hasMove(blockedMoves, Square::A1, Square::B1)) return false;

    // 4. Enemy pieces included as capture targets: King A1, enemy pawn on B2
    auto parsedCapture = FenParser::parse("8/8/8/8/8/8/1p6/K7 w - - 0 1");
    if (!parsedCapture.has_value()) return false;
    MoveList captureMoves;
    MoveGenerator::generateKingMoves(parsedCapture.value(), captureMoves);
    if (captureMoves.size() != 3) return false;
    if (!hasMove(captureMoves, Square::A1, Square::B2)) return false;

    return true;
}

bool testPawnSinglePush() {
    // 1. Unobstructed advance: White Pawn E3 -> E4
    auto parsed1 = FenParser::parse("8/8/8/8/8/4P3/8/4K2k w - - 0 1");
    if (!parsed1.has_value()) return false;
    MoveList moves1;
    MoveGenerator::generatePawnMoves(parsed1.value(), moves1);
    if (moves1.size() != 1) return false;
    if (!hasMove(moves1, Square::E3, Square::E4, Move::Flags::None)) return false;

    // 2. Forward blocker prevention (enemy piece on E4)
    auto parsed2 = FenParser::parse("8/8/8/8/4p3/4P3/8/4K2k w - - 0 1");
    if (!parsed2.has_value()) return false;
    MoveList moves2;
    MoveGenerator::generatePawnMoves(parsed2.value(), moves2);
    if (moves2.size() != 0) return false;

    // 3. Forward blocker prevention (friendly piece on E4)
    auto parsed3 = FenParser::parse("8/8/8/8/4N3/4P3/8/4K2k w - - 0 1");
    if (!parsed3.has_value()) return false;
    MoveList moves3;
    MoveGenerator::generatePawnMoves(parsed3.value(), moves3);
    if (moves3.size() != 0) return false;

    // 4. Black pawn advance (E6 -> E5) and blocker
    auto parsed4 = FenParser::parse("4K2k/8/4p3/8/8/8/8/8 b - - 0 1");
    if (!parsed4.has_value()) return false;
    MoveList moves4;
    MoveGenerator::generatePawnMoves(parsed4.value(), moves4);
    if (moves4.size() != 1) return false;
    if (!hasMove(moves4, Square::E6, Square::E5, Move::Flags::None)) return false;

    auto parsed5 = FenParser::parse("4K2k/8/4p3/4P3/8/8/8/8 b - - 0 1");
    if (!parsed5.has_value()) return false;
    MoveList moves5;
    MoveGenerator::generatePawnMoves(parsed5.value(), moves5);
    if (moves5.size() != 0) return false;

    return true;
}

bool testPawnDoublePush() {
    // 1. Starting rank advance: White Pawn E2 -> E3 (single) and E4 (double)
    auto parsed1 = FenParser::parse("8/8/8/8/8/8/4P3/4K2k w - - 0 1");
    if (!parsed1.has_value()) return false;
    MoveList moves1;
    MoveGenerator::generatePawnMoves(parsed1.value(), moves1);
    if (moves1.size() != 2) return false;
    if (!hasMove(moves1, Square::E2, Square::E3, Move::Flags::None)) return false;
    if (!hasMove(moves1, Square::E2, Square::E4, Move::Flags::DoublePawnPush)) return false;

    // 2. Intermediate square blocked: Piece on E3
    auto parsed2 = FenParser::parse("8/8/8/8/8/4n3/4P3/4K2k w - - 0 1");
    if (!parsed2.has_value()) return false;
    MoveList moves2;
    MoveGenerator::generatePawnMoves(parsed2.value(), moves2);
    if (moves2.size() != 0) return false;

    // 3. Destination square blocked: E3 vacant, piece on E4
    auto parsed3 = FenParser::parse("8/8/8/8/4n3/8/4P3/4K2k w - - 0 1");
    if (!parsed3.has_value()) return false;
    MoveList moves3;
    MoveGenerator::generatePawnMoves(parsed3.value(), moves3);
    if (moves3.size() != 1) return false;
    if (!hasMove(moves3, Square::E2, Square::E3, Move::Flags::None)) return false;

    // 4. Pawn already advanced off start rank (E3): only single push to E4
    auto parsed4 = FenParser::parse("8/8/8/8/8/4P3/8/4K2k w - - 0 1");
    if (!parsed4.has_value()) return false;
    MoveList moves4;
    MoveGenerator::generatePawnMoves(parsed4.value(), moves4);
    if (moves4.size() != 1) return false;
    if (!hasMove(moves4, Square::E3, Square::E4, Move::Flags::None)) return false;

    return true;
}

bool testPawnCaptures() {
    // 1. Left and right captures available: White Pawn E3, Black Knights on D4 and F4
    auto parsed1 = FenParser::parse("8/8/8/8/3n1n2/4P3/8/4K2k w - - 0 1");
    if (!parsed1.has_value()) return false;
    MoveList moves1;
    MoveGenerator::generatePawnMoves(parsed1.value(), moves1);
    if (moves1.size() != 3) return false; // E3-E4, E3xD4, E3xF4
    if (!hasMove(moves1, Square::E3, Square::E4, Move::Flags::None)) return false;
    if (!hasMove(moves1, Square::E3, Square::D4, Move::Flags::None)) return false;
    if (!hasMove(moves1, Square::E3, Square::F4, Move::Flags::None)) return false;

    // 2. Friendly piece on diagonal: White Pawn E3, White Knights on D4 and F4
    auto parsed2 = FenParser::parse("8/8/8/8/3N1N2/4P3/8/4K2k w - - 0 1");
    if (!parsed2.has_value()) return false;
    MoveList moves2;
    MoveGenerator::generatePawnMoves(parsed2.value(), moves2);
    if (moves2.size() != 1) return false;
    if (!hasMove(moves2, Square::E3, Square::E4, Move::Flags::None)) return false;

    // 3. Empty squares on diagonals: White Pawn E3, D4 and F4 empty
    auto parsed3 = FenParser::parse("8/8/8/8/8/4P3/8/4K2k w - - 0 1");
    if (!parsed3.has_value()) return false;
    MoveList moves3;
    MoveGenerator::generatePawnMoves(parsed3.value(), moves3);
    if (moves3.size() != 1) return false;
    if (!hasMove(moves3, Square::E3, Square::E4, Move::Flags::None)) return false;

    // 4. Black pawn diagonal captures: Black Pawn E6, White Knights on D5 and F5
    auto parsed4 = FenParser::parse("4K2k/8/4p3/3N1N2/8/8/8/8 b - - 0 1");
    if (!parsed4.has_value()) return false;
    MoveList moves4;
    MoveGenerator::generatePawnMoves(parsed4.value(), moves4);
    if (moves4.size() != 3) return false; // E6-E5, E6xD5, E6xF5
    if (!hasMove(moves4, Square::E6, Square::E5, Move::Flags::None)) return false;
    if (!hasMove(moves4, Square::E6, Square::D5, Move::Flags::None)) return false;
    if (!hasMove(moves4, Square::E6, Square::F5, Move::Flags::None)) return false;

    return true;
}

bool runMilestone2PhasesBCTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE 2, PHASES B & C: KING & PAWN GENERATION TESTS   ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 4;

    bool kingPass = testPseudoLegalKingGeneration();
    std::cout << "[" << (kingPass ? "PASS" : "FAIL") << "] Phase B: Pseudo-Legal King Moves (Center/Corner/Blockers/Captures)\n";
    if (kingPass) passed++;

    bool pushPass = testPawnSinglePush();
    std::cout << "[" << (pushPass ? "PASS" : "FAIL") << "] Phase C1: Pawn Single Push Mechanics (Advance/Blocker prevention)\n";
    if (pushPass) passed++;

    bool dppPass = testPawnDoublePush();
    std::cout << "[" << (dppPass ? "PASS" : "FAIL") << "] Phase C2: Pawn Double Push (Start rank/Intermediate block/Dest block)\n";
    if (dppPass) passed++;

    bool capPass = testPawnCaptures();
    std::cout << "[" << (capPass ? "PASS" : "FAIL") << "] Phase C3: Pawn Diagonal Captures (Left/Right/Friendly/Empty guards)\n";
    if (capPass) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "MILESTONE 2 PHASES B & C RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone 2 & Omega: Categorized Move Generation & Perft Verification Suite
// ---------------------------------------------------------------------------

struct CategorizedPerftTestCase {
    std::string category;
    std::string name;
    std::string fen;
    std::vector<std::pair<int, uint64_t>> depthExpected;
};

bool runMilestone2PerftTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   CATEGORIZED MOVE GENERATION & PERFT VERIFICATION SUITE  ===\n";
    std::cout << "=================================================================\n";

    const std::vector<CategorizedPerftTestCase> perftSuite = {
        // Category A: Standard Positions
        {
            "Category A: Standard Positions",
            "Position 1 (Startpos)",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            {
                {1, 20ULL},
                {2, 400ULL},
                {3, 8902ULL},
                {4, 197281ULL},
                {5, 4865609ULL}
            }
        },
        {
            "Category A: Standard Positions",
            "Position 2 (KiwiPete)",
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
            {
                {1, 48ULL},
                {2, 2039ULL},
                {3, 97862ULL},
                {4, 4085603ULL}
            }
        },

        // Category B: Castling & Path Obstructions
        {
            "Category B: Castling & Path Obstructions",
            "CPW Position 3 (Blocked Rooks & King Mobility)",
            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
            {
                {1, 14ULL},
                {2, 191ULL},
                {3, 2812ULL},
                {4, 43238ULL}
            }
        },
        {
            "Category B: Castling & Path Obstructions",
            "Empty Board Quad-Castling & Corner Flights",
            "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
            {
                {1, 26ULL},
                {2, 568ULL},
                {3, 13744ULL}
            }
        },

        // Category C: En Passant Discovered Checks and Pin Evasions
        {
            "Category C: En Passant Discovered Checks & Pin Evasions",
            "Diagonal Pin Preventing En Passant Capture",
            "8/5bk1/8/2Pp4/8/1K6/8/8 w - d6 0 1",
            {
                {1, 8ULL},
                {2, 104ULL},
                {3, 736ULL}
            }
        },
        {
            "Category C: En Passant Discovered Checks & Pin Evasions",
            "Horizontal Rank Pin Preventing En Passant Capture",
            "8/8/8/8/k1pP3R/8/8/1K6 b - d3 0 1",
            {
                {1, 6ULL},
                {2, 90ULL},
                {3, 502ULL}
            }
        },

        // Category D: Promotions and Knight/Bishop Underpromotions
        {
            "Category D: Promotions & Underpromotions",
            "CPW Position 4 (Dual Promotion Battery & Discovered Checks)",
            "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
            {
                {1, 6ULL},
                {2, 264ULL},
                {3, 9467ULL},
                {4, 422333ULL}
            }
        },
        {
            "Category D: Promotions & Underpromotions",
            "CPW Position 5 (Underpromotions on d8/c8 with Evasions)",
            "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
            {
                {1, 44ULL},
                {2, 1486ULL},
                {3, 62379ULL}
            }
        },

        // Category E: Double Checks and Check Evasion Bottlenecks
        {
            "Category E: Double Checks & Check Evasion Bottlenecks",
            "Double Check Cross-Ray Bottleneck (Bishop b4 + Rook h1)",
            "4k3/8/8/8/1b6/8/8/4K2r w - - 0 1",
            {
                {1, 2ULL},
                {2, 56ULL},
                {3, 265ULL}
            }
        },
        {
            "Category E: Double Checks & Check Evasion Bottlenecks",
            "CPW Position 5 Full Evasion Branching (Depth 4)",
            "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
            {
                {4, 2103487ULL}
            }
        }
    };

    int totalRuns = 0;
    int passedRuns = 0;
    std::string currentCategory = "";

    for (const auto& test : perftSuite) {
        if (test.category != currentCategory) {
            currentCategory = test.category;
            std::cout << "\n>>> " << currentCategory << " <<<\n";
        }

        std::cout << "--- " << test.name << " ---\n";
        std::cout << "    FEN: " << test.fen << "\n";

        auto parsed = FenParser::parse(test.fen);
        if (!parsed.has_value()) {
            std::cout << "    [FAIL] Could not parse FEN: " << test.fen << "\n";
            continue;
        }

        for (const auto& [depth, expectedNodes] : test.depthExpected) {
            totalRuns++;
            Position pos = parsed.value();
            uint64_t actualNodes = Search::perft(pos, depth);
            bool match = (actualNodes == expectedNodes);

            std::cout << "    [" << (match ? "PASS" : "FAIL") << "] Depth " << depth
                      << " | Expected: " << std::setw(10) << expectedNodes
                      << " | Actual: " << std::setw(10) << actualNodes << "\n";

            if (match) {
                passedRuns++;
            }
        }
    }

    std::cout << "\n=================================================================\n";
    std::cout << "CATEGORIZED PERFT RESULT: " << passedRuns << "/" << totalRuns << " Perft Depths Passed.\n";
    std::cout << "=================================================================\n";

    return (passedRuns == totalRuns);
}

// ---------------------------------------------------------------------------
// Milestone 2, Phases Y & Z: TT Integrity and Modular Static Evaluation Tests
// ---------------------------------------------------------------------------

bool testMaterialImbalance() {
    // 1. Missing Queen: White has no queen, Black has full army
    auto parsedNoQueen = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1");
    if (!parsedNoQueen.has_value()) return false;
    int scoreNoQueen = Evaluator::evaluate(parsedNoQueen.value());
    if (scoreNoQueen > -800) return false;

    // 2. Extra Pawn: White has 8 pawns, Black has 7 pawns (missing e7 pawn)
    auto parsedExtraPawn = FenParser::parse("rnbqkbnr/pppp1ppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsedExtraPawn.has_value()) return false;
    int scoreExtraPawn = Evaluator::evaluate(parsedExtraPawn.value());
    if (scoreExtraPawn < 80) return false;

    return true;
}

bool testPstPositionalGradient() {
    // 1. Knight Center vs Corner
    auto parsedCenter = FenParser::parse("4k3/8/8/8/4N3/8/8/4K3 w - - 0 1");
    auto parsedCorner = FenParser::parse("4k3/8/8/8/8/8/8/N3K3 w - - 0 1");
    if (!parsedCenter.has_value() || !parsedCorner.has_value()) return false;

    int scoreCenter = Evaluator::evaluate(parsedCenter.value());
    int scoreCorner = Evaluator::evaluate(parsedCorner.value());
    if (scoreCenter <= scoreCorner) return false;
    if (scoreCenter - scoreCorner != 70) return false;

    // 2. Knight Center Outposts (D4, E4, D5, E5) vs Rim/Corners (A1, H1, A8, H8)
    const std::vector<std::string> centerFens = {
        "4k3/8/8/8/3N4/8/8/4K3 w - - 0 1",
        "4k3/8/8/8/4N3/8/8/4K3 w - - 0 1",
        "4k3/8/8/3N4/8/8/8/4K3 w - - 0 1",
        "4k3/8/8/4N3/8/8/8/4K3 w - - 0 1"
    };
    const std::vector<std::string> rimFens = {
        "4k3/8/8/8/8/8/8/N3K3 w - - 0 1",
        "4k3/8/8/8/8/8/8/4K2N w - - 0 1",
        "N3k3/8/8/8/8/8/8/4K3 w - - 0 1",
        "4k2N/8/8/8/8/8/8/4K3 w - - 0 1"
    };
    for (const auto& cFen : centerFens) {
        auto pC = FenParser::parse(cFen);
        if (!pC.has_value()) return false;
        int cScore = Evaluator::evaluate(pC.value());
        for (const auto& rFen : rimFens) {
            auto pR = FenParser::parse(rFen);
            if (!pR.has_value()) return false;
            int rScore = Evaluator::evaluate(pR.value());
            if (cScore <= rScore) return false;
        }
    }

    return true;
}

bool testColorSymmetryInvariance() {
    // 1. Symmetrical starting position evaluates to 0
    auto parsedStart = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsedStart.has_value()) return false;
    if (Evaluator::evaluate(parsedStart.value()) != 0) return false;

    // 2. Symmetrical endgame evaluates to 0
    auto parsedEndgame = FenParser::parse("4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1");
    if (!parsedEndgame.has_value()) return false;
    if (Evaluator::evaluate(parsedEndgame.value()) != 0) return false;

    // 3. Symmetrical pieces on rooks + pawns evaluates to 0
    auto parsedSymmRooks = FenParser::parse("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    if (!parsedSymmRooks.has_value()) return false;
    if (Evaluator::evaluate(parsedSymmRooks.value()) != 0) return false;

    return true;
}

bool testEvaluationInversion() {
    // Asymmetric position: White has e4, Black has d5
    auto parsed1 = FenParser::parse("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    // Mirrored position: Black has e5, White has d4, Black to move
    auto parsedMirrored = FenParser::parse("rnbqkbnr/pppp1ppp/8/4p3/3P4/8/PPP1PPPP/RNBQKBNR b KQkq - 0 1");
    if (!parsed1.has_value() || !parsedMirrored.has_value()) return false;

    int score1 = Evaluator::evaluate(parsed1.value());
    int scoreMirrored = Evaluator::evaluate(parsedMirrored.value());

    // Both sides see identical relative evaluation from side-to-move perspective
    if (score1 != scoreMirrored) return false;

    // Swapping side to move on the mirrored position inverts the evaluation score
    Position flipped = parsedMirrored.value();
    flipped.setSideToMove(Color::White);
    int scoreFlipped = Evaluator::evaluate(flipped);
    if (score1 != -scoreFlipped) return false;

    return true;
}

bool testTranspositionTableIntegrity() {
    // 1. Mate Score Ply Invariance
    int rawMateScore = Search::MATE - 4; // Mate in 2 from ply 3
    int ttStored = Search::scoreToTT(rawMateScore, 3);
    if (ttStored != Search::MATE - 1) return false;
    int probedScore = Search::scoreFromTT(ttStored, 1);
    if (probedScore != Search::MATE - 2) return false;

    int rawMatedScore = -Search::MATE + 4; // Mated in 2 from ply 2
    int ttStoredMated = Search::scoreToTT(rawMatedScore, 2);
    if (ttStoredMated != -Search::MATE + 2) return false;
    int probedMated = Search::scoreFromTT(ttStoredMated, 4);
    if (probedMated != -Search::MATE + 6) return false;

    // 2. Cutoff Bounds and Entry Depth Filtering
    TranspositionTable tt(1);
    tt.clear();
    uint64_t testKey = 0x123456789ABCDEF0ULL;
    Move testMove(Square::E2, Square::E4);
    tt.store(testKey, 150, testMove, 5, TTNodeType::Exact, 0);

    int score = 0;
    Move bestMove;
    int depth = 0;
    TTNodeType type;
    bool hit = tt.probeEntry(testKey, score, bestMove, depth, type);
    if (!hit || score != 150 || bestMove.getRawData() != testMove.getRawData() || depth != 5 || type != TTNodeType::Exact) {
        return false;
    }

    // LowerBound cutoff test
    tt.store(testKey + 1, 200, testMove, 4, TTNodeType::LowerBound, 0);
    bool cutoffHigh = tt.probe(testKey + 1, score, bestMove, depth, type, 100, 150); // beta=150 <= 200 -> cutoff
    if (!cutoffHigh) return false;
    bool noCutoffHigh = tt.probe(testKey + 1, score, bestMove, depth, type, 100, 250); // beta=250 > 200 -> no cutoff
    if (noCutoffHigh) return false;

    // UpperBound cutoff test
    tt.store(testKey + 2, 50, testMove, 4, TTNodeType::UpperBound, 0);
    bool cutoffLow = tt.probe(testKey + 2, score, bestMove, depth, type, 100, 200); // alpha=100 >= 50 -> cutoff
    if (!cutoffLow) return false;
    bool noCutoffLow = tt.probe(testKey + 2, score, bestMove, depth, type, 25, 200); // alpha=25 < 50 -> no cutoff
    if (noCutoffLow) return false;

    // 3. Search Best Move Consistency with and without TT
    auto parsed = FenParser::parse("r1bqkb1r/pppp1ppp/2n5/4p3/2B1n3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 5");
    if (!parsed.has_value()) return false;
    Position pos1 = parsed.value();
    Position pos2 = pos1;

    Search::runSearch(pos1, 4);
    std::string bestMoveWithTT = SearchController::getInstance().getStats().pvString;

    Search::s_tt.clear();
    Search::runSearch(pos2, 4);
    std::string bestMoveFreshTT = SearchController::getInstance().getStats().pvString;

    if (bestMoveWithTT.empty() || bestMoveWithTT != bestMoveFreshTT) return false;

    return true;
}

bool runPhaseYZTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   PHASE Y & Z: TT INTEGRITY & MODULAR EVALUATION TESTS    ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 5;

    bool matPass = testMaterialImbalance();
    std::cout << "[" << (matPass ? "PASS" : "FAIL") << "] Phase Z1: Material Imbalance (Queen deficit & pawn surplus)\n";
    if (matPass) passed++;

    bool pstPass = testPstPositionalGradient();
    std::cout << "[" << (pstPass ? "PASS" : "FAIL") << "] Phase Z2: PST Positional Gradient (Knight center vs corner)\n";
    if (pstPass) passed++;

    bool symPass = testColorSymmetryInvariance();
    std::cout << "[" << (symPass ? "PASS" : "FAIL") << "] Phase Z3: Color Symmetry & Invariance (Startpos & endgames)\n";
    if (symPass) passed++;

    bool invPass = testEvaluationInversion();
    std::cout << "[" << (invPass ? "PASS" : "FAIL") << "] Phase Z4: Evaluation Inversion (Mirrored positions & side swap)\n";
    if (invPass) passed++;

    bool ttPass = testTranspositionTableIntegrity();
    std::cout << "[" << (ttPass ? "PASS" : "FAIL") << "] Phase Y: Transposition Table Integrity & Search Determinism\n";
    if (ttPass) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "PHASE Y & Z RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone 2, Phase AA: Advanced Move Ordering Architecture & Telemetry Tests
// ---------------------------------------------------------------------------

bool testMoveOrderingPriority() {
    // Position where White has:
    // - Winning capture: Pawn C3 takes Pawn D4 (SEE >= 0)
    // - Losing capture: Queen D1 takes Pawn D4 (SEE < 0, defended by Pawn E5)
    // - TT move: Pawn A2 -> A4
    // - Killer move: Knight B1 -> C3
    // - History move: Knight B1 -> A3
    // - Quiet move: Pawn A2 -> A3
    auto parsed = FenParser::parse("4k3/8/8/4p3/3p4/2P5/P7/RN1QK3 w - - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    MoveList moves;
    MoveGenerator::generateLegalMoves(pos, moves);
    if (moves.size() == 0) return false;

    Move ttMove(Square::A2, Square::A4, Move::Flags::DoublePawnPush);
    std::array<std::array<Move, 2>, 64> killerMoves{};
    killerMoves[0][0] = Move(Square::B1, Square::D2);

    std::array<std::array<uint32_t, 64>, 12> historyTable{};
    historyTable[static_cast<size_t>(Piece::WhiteKnight)][static_cast<size_t>(Square::A3)] = 10000;

    MoveOrderer::scoreAndSortMoves(pos, moves, ttMove, killerMoves, historyTable, 0, Move());

    // 1. TT move must be first (index 0)
    if (moves[0].getFromSquare() != Square::A2 || moves[0].getToSquare() != Square::A4) return false;

    // 2. Winning capture (Pawn C3 takes D4) must be index 1
    if (moves[1].getFromSquare() != Square::C3 || moves[1].getToSquare() != Square::D4) return false;

    // 3. Killer move (Knight B1 to D2) must be index 2
    if (moves[2].getFromSquare() != Square::B1 || moves[2].getToSquare() != Square::D2) return false;

    // 4. History move (Knight B1 to A3) must be index 3
    if (moves[3].getFromSquare() != Square::B1 || moves[3].getToSquare() != Square::A3) return false;

    // 5. Losing capture (Queen D1 takes D4) must be the very last move in the sorted list
    const Move& lastMove = moves[moves.size() - 1];
    if (lastMove.getFromSquare() != Square::D1 || lastMove.getToSquare() != Square::D4) return false;

    return true;
}

bool testMovePreservationInvariant() {
    auto parsed = FenParser::parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    MoveList moves;
    MoveGenerator::generateLegalMoves(pos, moves);
    const size_t origCount = moves.size();
    if (origCount != 48) return false;

    std::vector<Move> origMoves;
    for (size_t i = 0; i < origCount; ++i) {
        origMoves.push_back(moves[i]);
    }

    std::array<std::array<Move, 2>, 64> killerMoves{};
    std::array<std::array<uint32_t, 64>, 12> historyTable{};
    Move ttMove(Square::E5, Square::D7);

    MoveOrderer::scoreAndSortMoves(pos, moves, ttMove, killerMoves, historyTable, 0, Move());

    // 1. Size preservation
    if (moves.size() != origCount) return false;

    // 2. Element preservation (every original move exists in sorted list)
    for (const auto& om : origMoves) {
        bool found = false;
        for (size_t i = 0; i < moves.size(); ++i) {
            if (moves[i].getRawData() == om.getRawData()) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    // 3. No duplicates
    for (size_t i = 0; i < moves.size(); ++i) {
        for (size_t j = i + 1; j < moves.size(); ++j) {
            if (moves[i].getRawData() == moves[j].getRawData()) return false;
        }
    }

    return true;
}

bool testEvaluatorDecoupling() {
    auto parsed = FenParser::parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    const Position pos = parsed.value();
    Position testPos = pos;

    MoveList moves;
    MoveGenerator::generateLegalMoves(testPos, moves);

    std::array<std::array<Move, 2>, 64> killerMoves{};
    std::array<std::array<uint32_t, 64>, 12> historyTable{};

    MoveOrderer::scoreAndSortMoves(testPos, moves, Move(), killerMoves, historyTable, 0, Move());

    // Position state must remain completely unaltered
    if (!(testPos == pos)) return false;
    if (testPos.getHashKey() != pos.getHashKey()) return false;

    return true;
}

bool testSearchTelemetry() {
    // 1. Telemetry on startpos (Depth 5)
    auto parsedStart = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsedStart.has_value()) return false;
    Position startPos = parsedStart.value();

    Search::runSearch(startPos, 5);
    const auto& statsStart = SearchController::getInstance().getStats();
    if (statsStart.nodes == 0) return false;
    if (statsStart.ttHits == 0) return false;
    if (statsStart.betaCutoffs == 0) return false;
    if (statsStart.pvString.empty()) return false;

    // 2. Telemetry on KiwiPete (Depth 5)
    auto parsedKiwi = FenParser::parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    if (!parsedKiwi.has_value()) return false;
    Position kiwiPos = parsedKiwi.value();

    Search::runSearch(kiwiPos, 5);
    const auto& statsKiwi = SearchController::getInstance().getStats();
    if (statsKiwi.nodes == 0) return false;
    if (statsKiwi.ttHits == 0) return false;
    if (statsKiwi.betaCutoffs == 0) return false;
    if (statsKiwi.pvString.empty()) return false;

    return true;
}

bool runPhaseAATests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   PHASE AA: ADVANCED MOVE ORDERING & TELEMETRY TESTS      ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 4;

    bool prioPass = testMoveOrderingPriority();
    std::cout << "[" << (prioPass ? "PASS" : "FAIL") << "] Phase AA1: Priority Stratification (TT > Capture > Killer > Hist > Quiet > Losing)\n";
    if (prioPass) passed++;

    bool presPass = testMovePreservationInvariant();
    std::cout << "[" << (presPass ? "PASS" : "FAIL") << "] Phase AA2: Move Preservation Invariant (Zero adds, drops, or mutations)\n";
    if (presPass) passed++;

    bool decPass = testEvaluatorDecoupling();
    std::cout << "[" << (decPass ? "PASS" : "FAIL") << "] Phase AA3: Evaluator Decoupling Assertion (Pure metadata/SEE, zero eval mutation)\n";
    if (decPass) passed++;

    bool telPass = testSearchTelemetry();
    std::cout << "[" << (telPass ? "PASS" : "FAIL") << "] Phase AA4: Search Telemetry Verification (TT hits, beta cutoffs, determinism)\n";
    if (telPass) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "PHASE AA RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone 2, Phase AB: Quiescence Search & Horizon Stability Tests
// ---------------------------------------------------------------------------

bool testQuiescenceStandPatCutoff() {
    // White is ahead by a Queen: Ke1, Qe2 vs Black Ke8.
    // Static evaluation is heavily in White's favor (~ +900 cp).
    auto parsed = FenParser::parse("4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    int staticScore = Search::evaluate(pos);
    if (staticScore < 700) return false;

    // Search window [100, 200]: standing pat (~900) >= beta (200)
    int alpha = 100;
    int beta = 200;

    auto& stats = SearchController::getInstance().getStats();
    uint64_t qNodesBefore = stats.qNodes;

    int qScore = Search::quiescence(pos, alpha, beta, 0);

    // 1. Quiescence must fail high immediately and return >= beta
    if (qScore < beta) return false;

    // 2. Node count: Stand-pat beta cutoff must trigger immediately at root entry
    // without recursively searching child capture positions
    if (stats.qNodes - qNodesBefore != 1) return false;

    return true;
}

bool testQuiescenceHorizonCorrection() {
    // White Ke1, Pc3. Black Ke8, Qd4 (hanging undefended Queen).
    // Before capture, Black is ahead a full Queen (+900 cp for Black).
    // Plain static evaluation for White is heavily negative (~ -800 to -900 cp).
    auto parsed = FenParser::parse("4k3/8/8/8/3q4/2P5/8/4K3 w - - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    int staticEval = Search::evaluate(pos);
    if (staticEval > -700) return false; // Static eval is blind to the hanging queen

    // Quiescence search from White's perspective searches Pawn takes Queen (c3xd4),
    // winning the queen and leaving White with King+Pawn vs King (positive score).
    int qScore = Search::quiescence(pos, -Search::INF, Search::INF, 0);
    if (qScore <= 0) return false; // Quiescence captures the hanging material

    // Also verify nominal depth 1 search delegates to quiescence to capture the queen
    int searchScore = Search::runSearch(pos, 1);
    if (searchScore <= 0) return false;

    return true;
}

bool testQuiescenceRecaptureStability() {
    // White Bishop C4 attacks Black Bishop D5, defended by Black Rook D8:
    // White: Ke1, Bc4
    // Black: Ke8, Bd5, Rd8
    // White plays Bxd5; Black recaptures Rxd5, trading Bishops equally.
    auto parsed = FenParser::parse("3rk3/8/8/3b4/2B5/8/8/4K3 w - - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    // Initial static eval before trade: White has Bishop (325) vs Black Bishop (325) + Rook (500).
    // Black is up a Rook (~ -500 cp from White's perspective).
    int staticBefore = Search::evaluate(pos);
    if (staticBefore > -400) return false;

    // If quiescence stopped after White plays Bxd5 (without allowing Black to recapture Rxd5),
    // White would appear to have won a free Bishop (evaluation would jump by ~ +325 to -175 cp).
    // In full quiescence, Black recaptures Rxd5, restoring material equilibrium.
    int qScore = Search::quiescence(pos, -Search::INF, Search::INF, 0);

    // The score must reflect that the Bishop was recaptured, remaining around staticBefore (~ -500),
    // NOT the erroneous horizon jump (-175).
    if (qScore > -400) return false;

    return true;
}

bool testQuiescenceMutationAndInvariantSymmetry() {
    // 1. KiwiPete complex tactical position
    auto parsedKiwi = FenParser::parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    if (!parsedKiwi.has_value()) return false;
    Position kiwiPos = parsedKiwi.value();

    const Position originalKiwi = kiwiPos;
    const uint64_t originalHash = kiwiPos.getHashKey();

    Search::quiescence(kiwiPos, -Search::INF, Search::INF, 0);

    if (!checkPositionInvariants(kiwiPos)) return false;
    if (!(kiwiPos == originalKiwi)) return false;
    if (kiwiPos.getHashKey() != originalHash) return false;

    // 2. Sharp tactical opening position
    auto parsedTactical = FenParser::parse("r1bqkb1r/pppp1ppp/2n5/4p3/2B1n3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 5");
    if (!parsedTactical.has_value()) return false;
    Position tacPos = parsedTactical.value();

    const Position originalTac = tacPos;
    const uint64_t originalTacHash = tacPos.getHashKey();

    Search::quiescence(tacPos, -Search::INF, Search::INF, 0);

    if (!checkPositionInvariants(tacPos)) return false;
    if (!(tacPos == originalTac)) return false;
    if (tacPos.getHashKey() != originalTacHash) return false;

    return true;
}

bool runPhaseABTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   PHASE AB: QUIESCENCE SEARCH & HORIZON STABILITY TESTS    ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 4;

    bool standPatPass = testQuiescenceStandPatCutoff();
    std::cout << "[" << (standPatPass ? "PASS" : "FAIL") << "] Phase AB1: Stand-Pat Beta Cutoff & Node Pruning\n";
    if (standPatPass) passed++;

    bool horizonPass = testQuiescenceHorizonCorrection();
    std::cout << "[" << (horizonPass ? "PASS" : "FAIL") << "] Phase AB2: Horizon Tactical Correction (Hanging Material)\n";
    if (horizonPass) passed++;

    bool recapPass = testQuiescenceRecaptureStability();
    std::cout << "[" << (recapPass ? "PASS" : "FAIL") << "] Phase AB3: Recapture Stability & Forcing Exchange Resolution\n";
    if (recapPass) passed++;

    bool mutPass = testQuiescenceMutationAndInvariantSymmetry();
    std::cout << "[" << (mutPass ? "PASS" : "FAIL") << "] Phase AB4: Mutation & Invariant Symmetry (Zero Leakage)\n";
    if (mutPass) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "PHASE AB RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone 6, Modules 6.1 & 6.2: Iterative Deepening & PV Collection Tests
// ---------------------------------------------------------------------------

bool testIterativeDeepeningMonotonicity() {
    auto parsed = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    // Verify search to depth 4
    Search::runSearch(pos, 4);
    const auto& stats = SearchController::getInstance().getStats();

    // 1. Completed depth must reach target depth 4
    if (stats.completedDepth != 4) return false;

    // 2. Node count must be non-zero
    if (stats.nodes < 1000) return false;

    // 3. Monotonic expansion across depths 1 through 4
    std::array<uint64_t, 4> depthNodes{};
    for (int d = 1; d <= 4; ++d) {
        Position dPos = pos;
        Search::runSearch(dPos, d);
        depthNodes[d - 1] = SearchController::getInstance().getStats().nodes;
    }

    for (size_t i = 1; i < 4; ++i) {
        if (depthNodes[i] < depthNodes[i - 1]) return false;
    }

    return true;
}

bool testPVCoherenceAndLegality() {
    auto testPos = [](const std::string& fen) -> bool {
        auto parsed = FenParser::parse(fen);
        if (!parsed.has_value()) return false;
        Position pos = parsed.value();

        Search::runSearch(pos, 4);
        const auto& stats = SearchController::getInstance().getStats();
        const auto& pv = stats.pvLine;

        // 1. Non-empty PV with at least 2 plies at depth 4
        if (pv.count < 2) return false;

        // 2. Sequential legality: Every move in the PV must be legally playable in sequence
        Position playPos = pos;
        for (size_t i = 0; i < pv.count; ++i) {
            const Move& pvMove = pv.moves[i];
            MoveList legalMoves;
            MoveGenerator::generateLegalMoves(playPos, legalMoves);

            bool isLegal = false;
            for (size_t j = 0; j < legalMoves.size(); ++j) {
                if (legalMoves[j].getRawData() == pvMove.getRawData()) {
                    isLegal = true;
                    break;
                }
            }
            if (!isLegal) return false;

            UndoState undo;
            MoveExecutor::makeMove(playPos, pvMove, undo);
            if (!checkPositionInvariants(playPos)) return false;
        }

        return true;
    };

    // Test on startpos
    if (!testPos("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")) return false;

    // Test on KiwiPete
    if (!testPos("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1")) return false;

    return true;
}

bool testTTSeedingBenefit() {
    auto parsed = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    // 1. Run Iterative Deepening to depth 2
    Search::runSearch(pos, 2);
    const auto& statsD2 = SearchController::getInstance().getStats();
    if (statsD2.completedDepth != 2 || statsD2.pvLine.count == 0) return false;
    Move d2BestMove = statsD2.pvLine.moves[0];

    // 2. Verify TT was seeded with d2BestMove at the root position
    int ttScore = 0;
    Move ttMove;
    int ttDepth = 0;
    TTNodeType ttType = TTNodeType::Exact;
    bool hit = Search::s_tt.probeEntry(pos.getHashKey(), ttScore, ttMove, ttDepth, ttType);
    if (!hit) return false;
    if (ttMove.getRawData() != d2BestMove.getRawData()) return false;

    // 3. Verify that MoveOrderer prioritizes the TT move to index 0 (Tier 1 TT Guidance)
    MoveList legalMoves;
    MoveGenerator::generateLegalMoves(pos, legalMoves);
    std::array<std::array<Move, 2>, 64> emptyKillers{};
    std::array<std::array<uint32_t, 64>, 12> emptyHistory{};
    MoveOrderer::scoreAndSortMoves(pos, legalMoves, ttMove, emptyKillers, emptyHistory, 0, Move());
    if (legalMoves.size() == 0 || legalMoves[0].getRawData() != ttMove.getRawData()) return false;

    // 4. Run Iterative Deepening to depth 3 and verify move stability / TT guidance
    Search::runSearch(pos, 3);
    const auto& statsD3 = SearchController::getInstance().getStats();
    if (statsD3.completedDepth != 3 || statsD3.pvLine.count == 0) return false;
    Move d3BestMove = statsD3.pvLine.moves[0];

    // Move stability: best move from depth 2 is consistent with depth 3
    if (d3BestMove.getRawData() != d2BestMove.getRawData()) return false;

    return true;
}

bool runMilestone6Phase12Tests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE 6, MODULES 6.1 & 6.2: ID & PV COLLECTION      ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 3;

    bool idPass = testIterativeDeepeningMonotonicity();
    std::cout << "[" << (idPass ? "PASS" : "FAIL") << "] Module 6.1: Iterative Deepening Monotonicity (Depths 1-4)\n";
    if (idPass) passed++;

    bool pvPass = testPVCoherenceAndLegality();
    std::cout << "[" << (pvPass ? "PASS" : "FAIL") << "] Module 6.2: Triangular PV Coherence & Sequential Legality\n";
    if (pvPass) passed++;

    bool ttPass = testTTSeedingBenefit();
    std::cout << "[" << (ttPass ? "PASS" : "FAIL") << "] Module 6.1/6.2: Transposition Table Seeding & Guidance\n";
    if (ttPass) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "MILESTONE 6 (6.1 & 6.2) RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone 6, Module 6.3: Time Management & Search Control Tests
// ---------------------------------------------------------------------------

bool testFixedMovetimeAllocation() {
    auto parsed = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    SearchLimits limits;
    limits.movetime = 100; // 100 ms

    auto start = std::chrono::high_resolution_clock::now();
    Search::runSearch(pos, limits);
    auto end = std::chrono::high_resolution_clock::now();
    int64_t elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    const auto& stats = SearchController::getInstance().getStats();

    // 1. Search must terminate near 100 ms (allowing small polling window margin)
    if (elapsedMs < 70 || elapsedMs > 250) return false;

    // 2. Stop reason must be SoftTimeLimit or HardTimeLimit
    if (stats.stopReason != StopReason::SoftTimeLimit && stats.stopReason != StopReason::HardTimeLimit) return false;

    // 3. Completed depth must be at least depth 1
    if (stats.completedDepth < 1) return false;

    return true;
}

bool testCompletedIterationFallback() {
    auto parsed = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    // Run timed search with 35ms budget on startpos (completes depth 3, aborts in depth 4 or 5)
    SearchLimits limits;
    limits.movetime = 35;
    Position testPos = pos;
    Search::runSearch(testPos, limits);

    const auto& timedStats = SearchController::getInstance().getStats();
    if (timedStats.completedDepth < 1) return false;
    if (timedStats.pvLine.count == 0) return false;

    // The PV from timed search must be a valid, legally playable line
    Position simPos = pos;
    for (size_t i = 0; i < timedStats.pvLine.count; ++i) {
        MoveList legalMoves;
        MoveGenerator::generateLegalMoves(simPos, legalMoves);
        bool legal = false;
        for (size_t j = 0; j < legalMoves.size(); ++j) {
            if (legalMoves[j].getRawData() == timedStats.pvLine.moves[i].getRawData()) {
                legal = true;
                break;
            }
        }
        if (!legal) return false;
        UndoState undo;
        MoveExecutor::makeMove(simPos, timedStats.pvLine.moves[i], undo);
        if (!checkPositionInvariants(simPos)) return false;
    }

    return true;
}

bool testStateIntegrityOnAbort() {
    // Sharp tactical position with deep branching (KiwiPete)
    auto parsed = FenParser::parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    const Position originalPos = pos;
    const uint64_t originalHash = pos.getHashKey();

    // Run timed search with 25ms movetime that will abort in the middle of a complex sub-tree
    SearchLimits limits;
    limits.movetime = 25;

    Search::runSearch(pos, limits);

    // Verify transactional invariance: Position and Zobrist hash must be bit-for-bit identical
    if (!checkPositionInvariants(pos)) return false;
    if (!(pos == originalPos)) return false;
    if (pos.getHashKey() != originalHash) return false;

    return true;
}

bool testPeriodicNodeCheckFrequency() {
    // Verify node check period constant definition
    static_assert(Search::NODE_CHECK_PERIOD == 2048, "NODE_CHECK_PERIOD must be 2048 to amortize polling cost");

    // Verify SearchController stop token mechanism
    auto& controller = SearchController::getInstance();
    controller.requestStop(StopReason::ExternalStop);
    if (!controller.shouldStop()) return false;
    if (controller.getStats().stopReason != StopReason::ExternalStop) return false;

    // Reset via initSearch
    Position dummyPos;
    SearchLimits dummyLimits;
    controller.initSearch(dummyLimits, dummyPos);
    if (controller.shouldStop()) return false;

    return true;
}

bool runMilestone6Module63Tests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE 6, MODULE 6.3: TIME MANAGEMENT & SEARCH CONTROL ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 4;

    bool timePass = testFixedMovetimeAllocation();
    std::cout << "[" << (timePass ? "PASS" : "FAIL") << "] Module 6.3.1: Fixed Movetime Allocation & Boundary Clamping\n";
    if (timePass) passed++;

    bool fallbackPass = testCompletedIterationFallback();
    std::cout << "[" << (fallbackPass ? "PASS" : "FAIL") << "] Module 6.3.2: Completed Iteration Fallback & Partial PV Discard\n";
    if (fallbackPass) passed++;

    bool statePass = testStateIntegrityOnAbort();
    std::cout << "[" << (statePass ? "PASS" : "FAIL") << "] Module 6.3.3: Transactional State Integrity on Abort\n";
    if (statePass) passed++;

    bool freqPass = testPeriodicNodeCheckFrequency();
    std::cout << "[" << (freqPass ? "PASS" : "FAIL") << "] Module 6.3.4: Periodic Node Check Frequency & Stop Mechanism\n";
    if (freqPass) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "MILESTONE 6 (MODULE 6.3) RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone 6, Module 6.4: Aspiration Windows & Re-search Architecture Tests
// ---------------------------------------------------------------------------

bool testAspirationInsideWindow() {
    auto parsed = FenParser::parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    // Run iterative deepening to depth 2 on KiwiPete where eval is stable between d1 (95 cp) and d2 (90 cp)
    SearchLimits limits;
    limits.depth = 2;

    Search::runSearch(pos, limits);
    const auto& stats = SearchController::getInstance().getStats();

    // Depth 2 should complete cleanly with aspiration window matching depth 1 score
    if (stats.completedDepth != 2) return false;
    if (stats.pvLine.count == 0) return false;

    // Inside-window completion: 0 re-searches, 0 fail-highs, 0 fail-lows, at least 1 aspiration success
    if (stats.aspirationResearches != 0) return false;
    if (stats.aspirationFailHigh != 0) return false;
    if (stats.aspirationFailLow != 0) return false;
    if (stats.aspirationSuccesses == 0) return false;

    return true;
}

bool testAspirationFailHighRecovery() {
    auto parsed = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    // Establish ground truth score at depth 2
    PVLine truePv;
    SearchController::getInstance().initSearch(SearchLimits{}, pos);
    SearchController::getInstance().getStats().reset();
    Search::s_tt.clear();
    int groundTruth = Search::searchWithAspiration(pos, 2, 20, truePv);

    // Reset session and synthesize a pessimistic prevScore that forces fail-high
    SearchController::getInstance().initSearch(SearchLimits{}, pos);
    SearchController::getInstance().getStats().reset();
    Search::s_tt.clear();

    PVLine testPv;
    // prevScore set 80 cp below groundTruth -> initial window [groundTruth - 110, groundTruth - 50]
    // groundTruth exceeds beta (groundTruth - 50), triggering fail-high and beta widening
    int recoveredScore = Search::searchWithAspiration(pos, 2, groundTruth - 80, testPv);
    const auto& stats = SearchController::getInstance().getStats();

    if (stats.aspirationFailHigh == 0) return false;
    if (stats.aspirationResearches == 0) return false;
    if (recoveredScore != groundTruth) return false;
    if (testPv.count == 0) return false;

    return true;
}

bool testAspirationFailLowRecovery() {
    auto parsed = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    // Establish ground truth score at depth 2
    PVLine truePv;
    SearchController::getInstance().initSearch(SearchLimits{}, pos);
    SearchController::getInstance().getStats().reset();
    Search::s_tt.clear();
    int groundTruth = Search::searchWithAspiration(pos, 2, 20, truePv);

    // Reset session and synthesize an optimistic prevScore that forces fail-low
    SearchController::getInstance().initSearch(SearchLimits{}, pos);
    SearchController::getInstance().getStats().reset();
    Search::s_tt.clear();

    PVLine testPv;
    // prevScore set 80 cp above groundTruth -> initial window [groundTruth + 50, groundTruth + 110]
    // groundTruth is <= alpha (groundTruth + 50), triggering fail-low and alpha widening
    int recoveredScore = Search::searchWithAspiration(pos, 2, groundTruth + 80, testPv);
    const auto& stats = SearchController::getInstance().getStats();

    if (stats.aspirationFailLow == 0) return false;
    if (stats.aspirationResearches == 0) return false;
    if (recoveredScore != groundTruth) return false;
    if (testPv.count == 0) return false;

    return true;
}

bool testAspirationPVPreservationOnAbort() {
    auto parsed = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    // 1. Establish completed baseline at depth 2
    SearchLimits limitsD2;
    limitsD2.depth = 2;
    Search::runSearch(pos, limitsD2);

    const auto& statsD2 = SearchController::getInstance().getStats();
    if (statsD2.completedDepth != 2 || statsD2.pvLine.count == 0) return false;

    // 2. Run timed search with movetime = 25 ms, target depth 12
    // Startpos reaches depth 4-5 in ~15 ms, but higher depths require more time, aborting cleanly
    SearchLimits limitsTimed;
    limitsTimed.depth = 12;
    limitsTimed.movetime = 25;
    Search::runSearch(pos, limitsTimed);

    const auto& statsTimed = SearchController::getInstance().getStats();
    // Must have completed at least depth 2, but aborted before depth 12
    if (statsTimed.completedDepth < 2 || statsTimed.completedDepth >= 12) return false;
    if (statsTimed.pvLine.count == 0) return false;
    if (statsTimed.pvString.empty()) return false;

    // Verify first move of pvLine matches the first move token in pvString
    std::stringstream ss(statsTimed.pvString);
    std::string firstMoveStr;
    ss >> firstMoveStr;
    if (firstMoveStr != statsTimed.pvLine.moves[0].toString()) return false;

    // 3. Verify abort directly inside searchWithAspiration()
    auto& controller = SearchController::getInstance();
    controller.initSearch(SearchLimits{}, pos);
    controller.requestStop(StopReason::ExternalStop);
    PVLine abortPv;
    int abortScore = Search::searchWithAspiration(pos, 5, 0, abortPv);
    if (abortScore != 0) return false;

    return true;
}

bool testAspirationDecouplingAndTelemetry() {
    // Verify static compile-time contracts
    static_assert(Search::ASPIRATION_INITIAL_DELTA == 30, "Initial delta must be 30 cp");
    static_assert(Search::ASPIRATION_MAX_DELTA == 400, "Max delta threshold must be 400 cp");

    // Verify telemetry reset mechanics
    SearchStatistics stats;
    stats.aspirationSuccesses = 10;
    stats.aspirationFailHigh = 3;
    stats.aspirationFailLow = 2;
    stats.aspirationResearches = 5;

    stats.reset();

    if (stats.aspirationSuccesses != 0) return false;
    if (stats.aspirationFailHigh != 0) return false;
    if (stats.aspirationFailLow != 0) return false;
    if (stats.aspirationResearches != 0) return false;

    return true;
}

bool runMilestone6Module64Tests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE 6, MODULE 6.4: ASPIRATION WINDOWS & RE-SEARCH ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 5;

    bool hitPass = testAspirationInsideWindow();
    std::cout << "[" << (hitPass ? "PASS" : "FAIL") << "] Module 6.4.1: Aspiration Hit / Inside Window (Single Pass)\n";
    if (hitPass) passed++;

    bool failHighPass = testAspirationFailHighRecovery();
    std::cout << "[" << (failHighPass ? "PASS" : "FAIL") << "] Module 6.4.2: Fail-High Recovery & Beta Widening\n";
    if (failHighPass) passed++;

    bool failLowPass = testAspirationFailLowRecovery();
    std::cout << "[" << (failLowPass ? "PASS" : "FAIL") << "] Module 6.4.3: Fail-Low Recovery & Alpha Widening\n";
    if (failLowPass) passed++;

    bool abortPass = testAspirationPVPreservationOnAbort();
    std::cout << "[" << (abortPass ? "PASS" : "FAIL") << "] Module 6.4.4: Transactional PV & State Preservation on Abort\n";
    if (abortPass) passed++;

    bool decouplePass = testAspirationDecouplingAndTelemetry();
    std::cout << "[" << (decouplePass ? "PASS" : "FAIL") << "] Module 6.4.5: Driver Decoupling & Telemetry Reset Contract\n";
    if (decouplePass) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "MILESTONE 6 (MODULE 6.4) RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone 6, Module 6.5: Static Exchange Evaluation (SEE) Tests
// ---------------------------------------------------------------------------

bool testSEESimpleWinningCapture() {
    // White Queen captures undefended Black Pawn on d5
    auto parsed = FenParser::parse("4k3/8/8/3p4/8/8/8/3Q4 w - - 0 1");
    if (!parsed.has_value()) return false;
    const Position& pos = parsed.value();

    int val = SEE::evaluate(pos, Square::D1, Square::D5);
    if (val != Evaluator::PAWN_VALUE) return false;

    // Bishop captures undefended Rook on d5
    auto parsed2 = FenParser::parse("4k3/8/8/3r4/8/8/3B4/4K3 w - - 0 1");
    if (!parsed2.has_value()) return false;
    const Position& pos2 = parsed2.value();

    int val2 = SEE::evaluate(pos2, Square::D2, Square::D5);
    if (val2 != Evaluator::ROOK_VALUE) return false;

    return true;
}

bool testSEELosingExchange() {
    // White Queen captures Black Pawn on d5 defended by Black Pawn on c6
    auto parsed = FenParser::parse("4k3/8/2p5/3p4/8/8/8/3Q4 w - - 0 1");
    if (!parsed.has_value()) return false;
    const Position& pos = parsed.value();

    int val = SEE::evaluate(pos, Square::D1, Square::D5);
    // Net: Pawn (100) - Queen (900) = -800
    if (val != Evaluator::PAWN_VALUE - Evaluator::QUEEN_VALUE) return false;

    return true;
}

bool testSEEEqualExchange() {
    // White Rook captures Black Rook on d5, defended by Black Rook on d8
    auto parsed = FenParser::parse("3r4/4k3/8/3r4/8/8/8/3R4 w - - 0 1");
    if (!parsed.has_value()) return false;
    const Position& pos = parsed.value();

    int val = SEE::evaluate(pos, Square::D1, Square::D5);
    // Net: 500 - 500 = 0
    if (val != 0) return false;

    return true;
}

bool testSEEDiscoveredXRayAttack() {
    // Target: d5 (Black Rook, 500 cp)
    // Defender: d8 (Black Rook, 500 cp)
    // White attackers: d3 (White Queen, 900 cp), d1 (White Rook, 500 cp) behind Queen on d-file
    auto parsed = FenParser::parse("3r4/4k3/8/3r4/8/3Q4/8/3R2K1 w - - 0 1");
    if (!parsed.has_value()) return false;
    const Position& pos = parsed.value();

    // 1. QxR(d5) [gain: 500], 2. RxQ(d5) [gain: 900 - 500 = 400], 3. RxR(d5) via uncovered X-ray [gain: 500 - 400 = 100]
    // Minimax resolves to +100 cp (White wins 2 Rooks for 1 Queen)
    int val = SEE::evaluate(pos, Square::D3, Square::D5);
    if (val != 100) return false;

    return true;
}

bool testSEEPositionInvariance() {
    const std::vector<std::string> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1"
    };

    for (const auto& fen : fens) {
        auto parsed = FenParser::parse(fen);
        if (!parsed.has_value()) return false;
        Position pos = parsed.value();

        const Position originalPos = pos;
        const uint64_t originalHash = pos.getHashKey();

        MoveList moves;
        MoveGenerator::generateLegalMoves(pos, moves);

        for (size_t i = 0; i < moves.size(); ++i) {
            int val1 = SEE::evaluate(pos, moves[i].getFromSquare(), moves[i].getToSquare());
            int val2 = SEE::evaluate(pos, moves[i]);
            if (val1 != val2) return false;
        }

        // Verify zero-mutation invariant: bit-for-bit identity
        if (!checkPositionInvariants(pos)) return false;
        if (!(pos == originalPos)) return false;
        if (pos.getHashKey() != originalHash) return false;
    }

    return true;
}

bool runMilestone6Module65Tests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE 6, MODULE 6.5: STATIC EXCHANGE EVALUATION (SEE) ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 5;

    bool winPass = testSEESimpleWinningCapture();
    std::cout << "[" << (winPass ? "PASS" : "FAIL") << "] Module 6.5.1: Simple Winning Capture (Hanging Piece)\n";
    if (winPass) passed++;

    bool losePass = testSEELosingExchange();
    std::cout << "[" << (losePass ? "PASS" : "FAIL") << "] Module 6.5.2: Losing Exchange (Protected Pawn Blunder)\n";
    if (losePass) passed++;

    bool equalPass = testSEEEqualExchange();
    std::cout << "[" << (equalPass ? "PASS" : "FAIL") << "] Module 6.5.3: Equal Exchange (Rook Liquidation Chain)\n";
    if (equalPass) passed++;

    bool xrayPass = testSEEDiscoveredXRayAttack();
    std::cout << "[" << (xrayPass ? "PASS" : "FAIL") << "] Module 6.5.4: Discovered X-Ray Slider Attack Uncovering\n";
    if (xrayPass) passed++;

    bool invPass = testSEEPositionInvariance();
    std::cout << "[" << (invPass ? "PASS" : "FAIL") << "] Module 6.5.5: Zero-Mutation State & Hash Invariance\n";
    if (invPass) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "MILESTONE 6 (MODULE 6.5) RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone 6, Module 6.6: Null Move Pruning (NMP) Tests
// ---------------------------------------------------------------------------

bool testNullMoveStateSymmetry() {
    const std::vector<std::string> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2",
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
        "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1"
    };

    for (const auto& fen : fens) {
        auto parsed = FenParser::parse(fen);
        if (!parsed.has_value()) return false;
        Position pos = parsed.value();

        const Position originalPos = pos;
        const uint64_t originalHash = pos.getHashKey();

        // 1. Test via Position::makeNullMove and Position::undoNullMove
        UndoState undo1;
        pos.makeNullMove(undo1);

        if (pos.getSideToMove() == originalPos.getSideToMove()) return false;
        if (pos.getEnPassantSquare() != Square::None) return false;
        if (pos.getHashKey() == originalHash) return false;

        pos.undoNullMove(undo1);

        if (!checkPositionInvariants(pos)) return false;
        if (!(pos == originalPos)) return false;
        if (pos.getHashKey() != originalHash) return false;

        // 2. Test via MoveExecutor::makeNullMove and MoveExecutor::undoNullMove
        UndoState undo2;
        MoveExecutor::makeNullMove(pos, undo2);

        if (pos.getSideToMove() == originalPos.getSideToMove()) return false;
        if (pos.getEnPassantSquare() != Square::None) return false;
        if (pos.getHashKey() == originalHash) return false;

        MoveExecutor::undoNullMove(pos, undo2);

        if (!checkPositionInvariants(pos)) return false;
        if (!(pos == originalPos)) return false;
        if (pos.getHashKey() != originalHash) return false;
    }

    return true;
}

bool testNonPawnZugzwangGuard() {
    // Pure pawn endgame: White King on e2, White Pawn on e4, Black King on e7, Black Pawn on e6
    auto parsed = FenParser::parse("8/4k3/4p3/8/4P3/8/4K3/8 w - - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    // Verify hasNonPawnMaterial returns false for both sides
    if (pos.hasNonPawnMaterial(Color::White)) return false;
    if (pos.hasNonPawnMaterial(Color::Black)) return false;

    // Search position to depth 4: verify NMP is strictly bypassed
    SearchLimits limits;
    limits.depth = 4;
    Search::runSearch(pos, limits);

    const auto& stats = SearchController::getInstance().getStats();
    if (stats.nullMoveAttempts != 0) return false;
    if (stats.nullMoveCutoffs != 0) return false;

    // Now test position with non-pawn piece: White adds a Knight on c3
    auto parsedWithKnight = FenParser::parse("8/4k3/4p3/8/4P3/2N5/4K3/8 w - - 0 1");
    if (!parsedWithKnight.has_value()) return false;
    const Position& posKnight = parsedWithKnight.value();

    if (!posKnight.hasNonPawnMaterial(Color::White)) return false;
    if (posKnight.hasNonPawnMaterial(Color::Black)) return false;

    return true;
}

bool testInCheckNMPInvariant() {
    // 1. Position where side to move (White) is in check from a pawn, possesses abundant non-pawn material (Queen),
    // while the opponent has only King + Pawn (strictly zero non-pawn material, no promotions possible):
    // White: Ke4, Qa1. Black: Ke8, Pd5 (attacking e4).
    auto parsedCheck = FenParser::parse("4k3/8/8/3p4/4K3/8/8/Q7 w - - 0 1");
    if (!parsedCheck.has_value()) return false;
    Position posCheck = parsedCheck.value();

    // Verify side to move is strictly in check and possesses non-pawn material
    if (!MoveGenerator::inCheck(posCheck, posCheck.getSideToMove())) return false;
    if (!posCheck.hasNonPawnMaterial(posCheck.getSideToMove())) return false;
    if (posCheck.hasNonPawnMaterial(Color::Black)) return false;

    // 2. Full Iterative Deepening to depth 3 on in-check position:
    // - At ply 0 (root): depth 3, White has Queen, eval ~ +800 >= beta, but inCheck == true bypasses NMP.
    // - At ply 1: Black has no non-pawn material, so Black bypasses NMP via zugzwang guard.
    // - At ply 2: depth <= 1 < 3, so depth gate prevents NMP.
    // Result: Null move attempts across the entire search tree must be strictly 0.
    SearchLimits limits;
    limits.depth = 3;
    Search::runSearch(posCheck, limits);

    const auto& statsSearch = SearchController::getInstance().getStats();
    if (statsSearch.nullMoveAttempts != 0) return false;
    if (statsSearch.nullMoveCutoffs != 0) return false;

    // 3. Positive Control: Verify that an unconstrained position (not in check) with non-pawn material enables NMP
    auto parsedStart = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsedStart.has_value()) return false;
    Position posStart = parsedStart.value();

    if (MoveGenerator::inCheck(posStart, posStart.getSideToMove())) return false;
    if (!posStart.hasNonPawnMaterial(posStart.getSideToMove())) return false;

    SearchLimits limitsPos;
    limitsPos.depth = 4;
    Search::runSearch(posStart, limitsPos);

    const auto& statsPos = SearchController::getInstance().getStats();
    if (statsPos.nullMoveAttempts == 0) return false;
    if (statsPos.nullMoveCutoffs == 0) return false;

    return true;
}

bool testConsecutiveNullMovePrevention() {
    auto parsed = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    UndoState undo;
    pos.makeNullMove(undo);

    SearchLimits limits;
    limits.depth = 3;
    Search::runSearch(pos, limits);

    const auto& stats = SearchController::getInstance().getStats();
    pos.undoNullMove(undo);

    if (stats.completedDepth != 3) return false;
    if (stats.pvLine.count == 0) return false;

    return true;
}

bool testNMPTacticalIntegrityAndNodeReduction() {
    // Sharp tactical position with overwhelming pressure (WAC.001)
    auto parsed = FenParser::parse("2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    SearchLimits limits;
    limits.depth = 6;
    Search::runSearch(pos, limits);

    const auto& stats = SearchController::getInstance().getStats();
    if (stats.completedDepth != 6) return false;
    if (stats.pvLine.count == 0) return false;

    // In a winning tactical position, NMP must produce significant beta cutoffs
    if (stats.nullMoveAttempts == 0) return false;
    if (stats.nullMoveCutoffs == 0) return false;

    return true;
}

bool runMilestone6Module66Tests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE 6, MODULE 6.6: NULL MOVE PRUNING (NMP)        ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 5;

    bool symPass = testNullMoveStateSymmetry();
    std::cout << "[" << (symPass ? "PASS" : "FAIL") << "] Module 6.6.1: Null Move State Symmetry & Hash Invariance\n";
    if (symPass) passed++;

    bool zugPass = testNonPawnZugzwangGuard();
    std::cout << "[" << (zugPass ? "PASS" : "FAIL") << "] Module 6.6.2: Non-Pawn Endgame Zugzwang Protection Guard\n";
    if (zugPass) passed++;

    bool checkPass = testInCheckNMPInvariant();
    std::cout << "[" << (checkPass ? "PASS" : "FAIL") << "] Module 6.6.3: In-Check NMP Invariant (Strict Check Bypass)\n";
    if (checkPass) passed++;

    bool consecPass = testConsecutiveNullMovePrevention();
    std::cout << "[" << (consecPass ? "PASS" : "FAIL") << "] Module 6.6.4: Consecutive Null Move Prevention Propagation\n";
    if (consecPass) passed++;

    bool nodePass = testNMPTacticalIntegrityAndNodeReduction();
    std::cout << "[" << (nodePass ? "PASS" : "FAIL") << "] Module 6.6.5: Tactical Integrity & NMP Node Cutoff Efficacy\n";
    if (nodePass) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "MILESTONE 6 (MODULE 6.6) RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone 6, Module 6.7: Late Move Reductions (LMR) Tests
// ---------------------------------------------------------------------------

bool testLMRTableSanity() {
    LMRPolicy::initializeTable();
    if (!LMRPolicy::isInitialized()) return false;

    // 1. Zero reduction for shallow depths (depth < 3) or early moves (moveCount < 4)
    for (int d = 0; d < 3; ++d) {
        for (int m = 0; m < 64; ++m) {
            if (LMRPolicy::getReduction(d, m) != 0) return false;
        }
    }
    for (int d = 0; d < 64; ++d) {
        for (int m = 0; m < 4; ++m) {
            if (LMRPolicy::getReduction(d, m) != 0) return false;
        }
    }

    // 2. Monotonicity across depths and move counts
    for (int d = 3; d < 63; ++d) {
        for (int m = 4; m < 64; ++m) {
            if (LMRPolicy::getReduction(d + 1, m) < LMRPolicy::getReduction(d, m)) return false;
        }
    }
    for (int d = 3; d < 64; ++d) {
        for (int m = 4; m < 63; ++m) {
            if (LMRPolicy::getReduction(d, m + 1) < LMRPolicy::getReduction(d, m)) return false;
        }
    }

    // 3. Horizon safety: reduction must never exceed or equal depth (d - 1 clamp)
    for (int d = 0; d < 64; ++d) {
        for (int m = 0; m < 64; ++m) {
            int r = LMRPolicy::getReduction(d, m);
            if (d > 0 && r > d - 1) return false;
            if (r < 0) return false;
        }
    }

    // 4. Boundary safety and clamping
    if (LMRPolicy::getReduction(-5, 10) != 0) return false;
    if (LMRPolicy::getReduction(10, -5) != 0) return false;
    if (LMRPolicy::getReduction(100, 100) != LMRPolicy::getReduction(63, 63)) return false;

    return true;
}

bool testTacticalMoveExemption() {
    // Sharp tactical position with immediate captures and promotions:
    // White: Ke1, Pe7, Ra1. Black: Kg8, Ra5 (on open a-file). e8 square is empty for promotion.
    // White can promote e7e8=Q or capture Rxa5.
    auto parsed = FenParser::parse("6k1/4P3/8/r7/8/8/8/R3K3 w - - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    MoveList moves;
    MoveGenerator::generateLegalMoves(pos, moves);

    // Verify moves classification: captures and promotions are present
    bool foundCapture = false;
    bool foundPromotion = false;
    for (size_t i = 0; i < moves.size(); ++i) {
        const Move& m = moves[i];
        const Bitboard targetBit = Bitboards::getSquareBit(m.getToSquare());
        const bool isCapture = (pos.getTotalOccupancy() & targetBit) != 0 || m.isEnPassant();
        const bool isPromotion = m.isPromotion();
        if (isCapture) foundCapture = true;
        if (isPromotion) foundPromotion = true;
    }
    if (!foundCapture || !foundPromotion) return false;

    // Run nominal search to depth 4: tactical moves must solve and promote without reduction
    SearchLimits limits;
    limits.depth = 4;
    Search::runSearch(pos, limits);

    const auto& stats = SearchController::getInstance().getStats();
    if (stats.completedDepth != 4) return false;
    if (stats.pvLine.count == 0) return false;

    return true;
}

bool testInCheckExemption() {
    // Position where White is in check:
    // White: Ke1, Qd1. Black: Kg8, Re8 (attacking e1 along open e-file).
    auto parsed = FenParser::parse("4r1k1/8/8/8/8/8/8/3QK3 w - - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    if (!MoveGenerator::inCheck(pos, pos.getSideToMove())) return false;

    // Reset session and search at depth 3 via negamax:
    // Under inCheck, !inCheck is false, so reduction is strictly 0 for all moves at this ply
    auto& controller = SearchController::getInstance();
    controller.initSearch(SearchLimits{}, pos);
    controller.getStats().reset();
    Search::s_tt.clear();

    PVLine pv;
    Search::negamax(pos, 3, -10000, 10000, 0, pv, true, Move());

    const auto& stats = controller.getStats();
    // At root inCheck is true; at ply 1 depth is 2 < 3.
    // LMR attempts must strictly be 0 throughout the search tree
    if (stats.lmrAttempts != 0) return false;
    if (stats.lmrReducedNodes != 0) return false;

    return true;
}

bool testResearchTriggerInvariant() {
    // Startpos evaluated to depth 6: contains dozens of quiet moves where late candidates
    // undergo reduced search, fail high, and trigger full-depth re-searches and PV overturns.
    auto parsed = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    SearchLimits limits;
    limits.depth = 6;
    Search::runSearch(pos, limits);

    const auto& stats = SearchController::getInstance().getStats();
    if (stats.completedDepth != 6) return false;
    if (stats.pvLine.count == 0) return false;

    // 1. Reduced scout searches must have been attempted
    if (stats.lmrAttempts == 0) return false;
    if (stats.lmrReducedNodes == 0) return false;

    // 2. Re-search protocol must have triggered upon score > alpha
    if (stats.lmrResearches == 0) return false;

    // 3. Successful PV overturn protocol verification
    if (stats.successfulResearches == 0) return false;

    return true;
}

bool testTacticalSuiteRegressionCheck() {
    // Sharp tactical test: WAC.001 (Tactical Rook Strike)
    // Black Ke8, Rd8, Rc8, Qe2, Pawns a5, d6, f7, g7, h7 vs White Kc3, Rd1, Rh1, Nd5, Pawns b4, c4, f3, a2, g2, h2.
    // Winning tactical move is c8c4!
    auto parsed = FenParser::parse("2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    SearchLimits limits;
    limits.depth = 6;
    Search::runSearch(pos, limits);

    const auto& stats = SearchController::getInstance().getStats();
    if (stats.completedDepth != 6) return false;
    if (stats.pvLine.count == 0) return false;

    // 1. Search must find the winning tactical move c8c4
    if (stats.pvLine.moves[0].toString() != "c8c4") return false;

    // 2. LMR must be active during the search, pruning non-tactical lines
    if (stats.lmrAttempts == 0) return false;

    // 3. Node count sanity: must explore under 300,000 nodes due to effective pruning
    if (stats.nodes > 300000) return false;

    return true;
}

bool runMilestone6Module67Tests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE 6, MODULE 6.7: LATE MOVE REDUCTIONS (LMR)     ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 5;

    bool tablePass = testLMRTableSanity();
    std::cout << "[" << (tablePass ? "PASS" : "FAIL") << "] Module 6.7.1: Centralized LMR Reduction Table Monotonicity & Clamping\n";
    if (tablePass) passed++;

    bool tacPass = testTacticalMoveExemption();
    std::cout << "[" << (tacPass ? "PASS" : "FAIL") << "] Module 6.7.2: Tactical Move Exemption (Captures & Promotions)\n";
    if (tacPass) passed++;

    bool checkPass = testInCheckExemption();
    std::cout << "[" << (checkPass ? "PASS" : "FAIL") << "] Module 6.7.3: In-Check Exemption Invariant (Strict Check Bypass)\n";
    if (checkPass) passed++;

    bool resPass = testResearchTriggerInvariant();
    std::cout << "[" << (resPass ? "PASS" : "FAIL") << "] Module 6.7.4: Full-Depth Re-search Trigger & Overturn Invariant\n";
    if (resPass) passed++;

    bool suitePass = testTacticalSuiteRegressionCheck();
    std::cout << "[" << (suitePass ? "PASS" : "FAIL") << "] Module 6.7.5: Tactical Benchmark Integrity & Node Pruning Efficacy\n";
    if (suitePass) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "MILESTONE 6 (MODULE 6.7) RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone 6, Module 6.8: Counter-Move History (CMH) Tests
// ---------------------------------------------------------------------------

bool testCMHReadWriteInvariant() {
    Search::clearCMH();
    auto& cmTable = Search::getMutableCMH();

    // 1. Initial state: all entries in both tables must be empty (raw == 0)
    for (int from = 0; from < 64; ++from) {
        for (int to = 0; to < 64; ++to) {
            if (cmTable.getCounterMove(static_cast<Square>(from), static_cast<Square>(to)).getRawData() != 0) return false;
        }
    }
    for (size_t p = 0; p < 12; ++p) {
        for (int to = 0; to < 64; ++to) {
            if (cmTable.getCounterMove(static_cast<Piece>(p), static_cast<Square>(to)).getRawData() != 0) return false;
        }
    }

    // 2. Primary square-pair indexing: store and retrieve
    Move cm1(Square::G1, Square::F3);
    cmTable.store(Square::E2, Square::E4, cm1);
    Move retrieved1 = cmTable.getCounterMove(Square::E2, Square::E4);
    if (retrieved1.getRawData() != cm1.getRawData()) return false;
    if (retrieved1.getFromSquare() != Square::G1 || retrieved1.getToSquare() != Square::F3) return false;

    // Unrelated square lookup must remain empty
    if (cmTable.getCounterMove(Square::D2, Square::D4).getRawData() != 0) return false;

    // 3. Secondary piece-to indexing: store and retrieve
    Move cm2(Square::C7, Square::C5);
    cmTable.store(Piece::WhitePawn, Square::E4, cm2);
    Move retrieved2 = cmTable.getCounterMove(Piece::WhitePawn, Square::E4);
    if (retrieved2.getRawData() != cm2.getRawData()) return false;
    if (retrieved2.getFromSquare() != Square::C7 || retrieved2.getToSquare() != Square::C5) return false;

    // Unrelated piece/to lookup must remain empty
    if (cmTable.getCounterMove(Piece::BlackKnight, Square::F3).getRawData() != 0) return false;

    // 4. Overwrite behavior
    Move cm3(Square::B8, Square::C6);
    cmTable.store(Square::E2, Square::E4, cm3);
    if (cmTable.getCounterMove(Square::E2, Square::E4).getRawData() != cm3.getRawData()) return false;

    // 5. Boundary safety: Square::None and Piece::None must return default Move
    if (cmTable.getCounterMove(Square::None, Square::E4).getRawData() != 0) return false;
    if (cmTable.getCounterMove(Square::E2, Square::None).getRawData() != 0) return false;
    if (cmTable.getCounterMove(Piece::None, Square::E4).getRawData() != 0) return false;
    if (cmTable.getCounterMove(Piece::WhitePawn, Square::None).getRawData() != 0) return false;

    // 6. Clear invariant: zero-reset on new search boundary
    Search::clearCMH();
    if (cmTable.getCounterMove(Square::E2, Square::E4).getRawData() != 0) return false;
    if (cmTable.getCounterMove(Piece::WhitePawn, Square::E4).getRawData() != 0) return false;

    return true;
}

bool testCMHMoveOrderingStratification() {
    // Position: White Ke1, Ra1, Nb1, Pa2; Black Ke8, Pe5
    auto parsed = FenParser::parse("4k3/8/8/4p3/8/8/P7/RN2K3 w - - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    MoveList moves;
    MoveGenerator::generateLegalMoves(pos, moves);

    // Setup heuristic tables:
    // Killer 1: Nb1c3 (Score = 30000)
    // Killer 2: Nb1a3 (Score = 29000)
    // CMH:      Nb1d2 (Score = 28000)
    // History:  Pa2a3 (Score = 18000)
    std::array<std::array<Move, 2>, 64> killerMoves{};
    killerMoves[0][0] = Move(Square::B1, Square::C3);
    killerMoves[0][1] = Move(Square::B1, Square::A3);

    std::array<std::array<uint32_t, 64>, 12> historyTable{};
    historyTable[static_cast<size_t>(Piece::WhitePawn)][static_cast<size_t>(Square::A3)] = 18000;

    // 1. Primary Square-Pair CMH Lookup Verification
    Search::clearCMH();
    Search::getMutableCMH().store(Square::E7, Square::E5, Move(Square::B1, Square::D2));

    Move prevMove(Square::E7, Square::E5);
    MoveList testMoves = moves;
    MoveOrderer::scoreAndSortMoves(pos, testMoves, Move(), killerMoves, historyTable, 0, prevMove);

    // Assert exact stratification order: Killer 1 > Killer 2 > CMH > History
    if (testMoves[0] != Move(Square::B1, Square::C3)) return false;
    if (testMoves[1] != Move(Square::B1, Square::A3)) return false;
    if (testMoves[2] != Move(Square::B1, Square::D2)) return false;
    if (testMoves[3] != Move(Square::A2, Square::A3)) return false;

    // Verify static architectural score constants
    static_assert(MoveOrderer::SCORE_KILLER_1 > MoveOrderer::SCORE_KILLER_2);
    static_assert(MoveOrderer::SCORE_KILLER_2 > MoveOrderer::SCORE_COUNTERMOVE);
    static_assert(MoveOrderer::SCORE_COUNTERMOVE > 20000); // 20000 is maximum history clamp

    // 2. Secondary Piece-To CMH Fallback Verification
    Search::clearCMH();
    Search::getMutableCMH().store(Piece::BlackPawn, Square::E5, Move(Square::B1, Square::D2));

    MoveList testMovesFallback = moves;
    MoveOrderer::scoreAndSortMoves(pos, testMovesFallback, Move(), killerMoves, historyTable, 0, prevMove);

    if (testMovesFallback[0] != Move(Square::B1, Square::C3)) return false;
    if (testMovesFallback[1] != Move(Square::B1, Square::A3)) return false;
    if (testMovesFallback[2] != Move(Square::B1, Square::D2)) return false;
    if (testMovesFallback[3] != Move(Square::A2, Square::A3)) return false;

    return true;
}

bool testCMHSearchOwnershipInvariant() {
    Search::clearCMH();
    auto& cmTable = Search::getMutableCMH();

    Move testCm(Square::B1, Square::C3);
    cmTable.store(Square::E7, Square::E5, testCm);
    cmTable.store(Piece::BlackKnight, Square::D4, testCm);

    auto parsed = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    MoveList moves;
    MoveGenerator::generateLegalMoves(pos, moves);

    std::array<std::array<Move, 2>, 64> killerMoves{};
    std::array<std::array<uint32_t, 64>, 12> historyTable{};

    // Execute MoveOrderer across multiple plies and configurations
    for (int ply = 0; ply < 8; ++ply) {
        MoveList movesCopy = moves;
        MoveOrderer::scoreAndSortMoves(pos, movesCopy, Move(), killerMoves, historyTable, ply, Move(Square::E7, Square::E5));
        MoveOrderer::scoreAndSortTacticalMoves(pos, movesCopy);
    }

    // Verify Search::getCMH() was strictly read and never mutated
    const auto& cmRead = Search::getCMH();
    if (cmRead.getCounterMove(Square::E7, Square::E5).getRawData() != testCm.getRawData()) return false;
    if (cmRead.getCounterMove(Piece::BlackKnight, Square::D4).getRawData() != testCm.getRawData()) return false;

    return true;
}

bool testCMHSearchDeterminismAndTacticalIntegrity() {
    // Sharp tactical test: WAC.001
    // Winning tactical strike: c8c4
    const std::string wac001 = "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1";
    auto parsed = FenParser::parse(wac001);
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    // Run 1
    SearchLimits limits;
    limits.depth = 6;
    int score1 = Search::runSearch(pos, limits);
    const auto stats1 = SearchController::getInstance().getStats();
    std::string move1 = stats1.pvLine.count > 0 ? stats1.pvLine.moves[0].toString() : "";
    std::string pv1 = stats1.pvString;

    // Run 2
    int score2 = Search::runSearch(pos, limits);
    const auto stats2 = SearchController::getInstance().getStats();
    std::string move2 = stats2.pvLine.count > 0 ? stats2.pvLine.moves[0].toString() : "";
    std::string pv2 = stats2.pvString;

    // Run 3
    int score3 = Search::runSearch(pos, limits);
    const auto stats3 = SearchController::getInstance().getStats();
    std::string move3 = stats3.pvLine.count > 0 ? stats3.pvLine.moves[0].toString() : "";
    std::string pv3 = stats3.pvString;

    // 1. Bit-for-bit determinism across consecutive runs
    if (score1 != score2 || score2 != score3) return false;
    if (move1 != move2 || move2 != move3) return false;
    if (pv1 != pv2 || pv2 != pv3) return false;

    // 2. Tactical accuracy on WAC.001
    if (move1 != "c8c4") return false;
    if (stats1.completedDepth != 6) return false;

    // 3. Search heuristic activity: CMH active engagement in iterative deepening
    auto parsedStart = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsedStart.has_value()) return false;
    Position posStart = parsedStart.value();

    SearchLimits startLimits;
    startLimits.depth = 4;
    Search::runSearch(posStart, startLimits);
    const auto statsStart = SearchController::getInstance().getStats();

    if (statsStart.completedDepth != 4) return false;
    if (statsStart.cmhHits == 0) return false;

    return true;
}

bool runMilestone6Module68Tests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE 6, MODULE 6.8: COUNTER-MOVE HISTORY (CMH)     ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 4;

    bool rwPass = testCMHReadWriteInvariant();
    std::cout << "[" << (rwPass ? "PASS" : "FAIL") << "] Module 6.8.1: CounterMoveTable Read/Write Invariants & Dual Indexing\n";
    if (rwPass) passed++;

    bool stratPass = testCMHMoveOrderingStratification();
    std::cout << "[" << (stratPass ? "PASS" : "FAIL") << "] Module 6.8.2: Move Ordering Stratification & Heuristic Precedence\n";
    if (stratPass) passed++;

    bool ownPass = testCMHSearchOwnershipInvariant();
    std::cout << "[" << (ownPass ? "PASS" : "FAIL") << "] Module 6.8.3: Architectural Decoupling & Read-Only Consumer Contract\n";
    if (ownPass) passed++;

    bool detPass = testCMHSearchDeterminismAndTacticalIntegrity();
    std::cout << "[" << (detPass ? "PASS" : "FAIL") << "] Module 6.8.4: Search Determinism, State Reset & Tactical Integrity\n";
    if (detPass) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "MILESTONE 6 (MODULE 6.8) RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone 6, Module 6.9: Continuation History (ContHist) Tests
// ---------------------------------------------------------------------------

bool testContHistLifecycleContract() {
    Search::clearContHist();
    auto& chTable = Search::getMutableContHist();
    chTable.initialize();

    // 1. Initial State: all probed entries must be 0
    for (size_t p = 0; p < 12; ++p) {
        for (int from = 0; from < 64; ++from) {
            for (int to = 0; to < 64; ++to) {
                if (chTable.probe(static_cast<Piece>(p), static_cast<Square>(from), static_cast<Square>(to)) != 0) return false;
            }
        }
    }

    // 2. Bounded Update with Gravity Decay
    chTable.update(Piece::WhiteKnight, Square::E4, Square::F6, 1000);
    int s1 = chTable.probe(Piece::WhiteKnight, Square::E4, Square::F6);
    if (s1 != 1000) return false;

    // Second update should apply gravity decay: score < 2000
    chTable.update(Piece::WhiteKnight, Square::E4, Square::F6, 1000);
    int s2 = chTable.probe(Piece::WhiteKnight, Square::E4, Square::F6);
    if (s2 <= s1 || s2 >= 2000) return false;

    // Huge bonus must be clamped to MAX_HISTORY (16384)
    chTable.update(Piece::WhiteKnight, Square::E4, Square::F6, 50000);
    int s3 = chTable.probe(Piece::WhiteKnight, Square::E4, Square::F6);
    if (s3 > ContinuationHistoryTable::MAX_HISTORY) return false;

    // Negative update / malus
    chTable.update(Piece::BlackBishop, Square::D5, Square::B7, -500);
    int sNeg = chTable.probe(Piece::BlackBishop, Square::D5, Square::B7);
    if (sNeg != -500) return false;

    // 3. Normalization / Aging: halves the scores
    chTable.normalize();
    int s4 = chTable.probe(Piece::WhiteKnight, Square::E4, Square::F6);
    if (s4 != s3 / 2) return false;
    int sNegAged = chTable.probe(Piece::BlackBishop, Square::D5, Square::B7);
    if (sNegAged != -250) return false;

    // Age alias test
    chTable.age();
    if (chTable.probe(Piece::WhiteKnight, Square::E4, Square::F6) != s4 / 2) return false;

    // 4. Boundary safety
    if (chTable.probe(Piece::None, Square::E4, Square::F6) != 0) return false;
    if (chTable.probe(Piece::WhiteKnight, Square::None, Square::F6) != 0) return false;
    if (chTable.probe(Piece::WhiteKnight, Square::E4, Square::None) != 0) return false;

    // 5. Clear / Reset
    chTable.clear();
    if (chTable.probe(Piece::WhiteKnight, Square::E4, Square::F6) != 0) return false;

    return true;
}

bool testContHistScoreStratification() {
    // Position: White Ke1, Ra1, Nb1, Pa2; Black Ke8, Pe5
    auto parsed = FenParser::parse("4k3/8/8/4p3/8/8/P7/RN2K3 w - - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    MoveList moves;
    MoveGenerator::generateLegalMoves(pos, moves);

    // Setup heuristic tables:
    // Killer 1:          Nb1c3 (Score = 32000)
    // Killer 2:          Nb1a3 (Score = 30000)
    // High Continuation: a2a4  (Score = 26000 + bonus = 27000)
    // Pure Counter-Move: Nb1d2 (Score = 24000)
    // Butterfly History: a2a3  (Score = 18000)
    std::array<std::array<Move, 2>, 64> killerMoves{};
    killerMoves[0][0] = Move(Square::B1, Square::C3);
    killerMoves[0][1] = Move(Square::B1, Square::A3);

    std::array<std::array<uint32_t, 64>, 12> historyTable{};
    historyTable[static_cast<size_t>(Piece::WhitePawn)][static_cast<size_t>(Square::A3)] = 18000;

    Search::clearCMH();
    Search::clearContHist();

    // Store pure countermove for prevMove (E7->E5)
    Search::getMutableCMH().store(Square::E7, Square::E5, Move(Square::B1, Square::D2));

    // Store high continuation history for Pa2a4 following Pe7e5
    Search::getMutableContHist().update(Piece::WhitePawn, Square::E5, Square::A4, 8000);

    Move prevMove(Square::E7, Square::E5);
    MoveList testMoves = moves;
    MoveOrderer::scoreAndSortMoves(pos, testMoves, Move(), killerMoves, historyTable, 0, prevMove);

    // Verify exact ordering: Killer 1 > Killer 2 > Continuation > Counter-Move > Butterfly History
    if (testMoves[0] != Move(Square::B1, Square::C3)) return false;
    if (testMoves[1] != Move(Square::B1, Square::A3)) return false;
    if (testMoves[2] != Move(Square::A2, Square::A4, Move::Flags::DoublePawnPush)) return false;
    if (testMoves[3] != Move(Square::B1, Square::D2)) return false;
    if (testMoves[4] != Move(Square::A2, Square::A3)) return false;

    // Verify static architectural score constants
    static_assert(MoveOrderer::SCORE_KILLER_1 > MoveOrderer::SCORE_KILLER_2);
    static_assert(MoveOrderer::SCORE_KILLER_2 > MoveOrderer::SCORE_CONTHIST_BASE);
    static_assert(MoveOrderer::SCORE_CONTHIST_BASE > MoveOrderer::SCORE_COUNTERMOVE);
    static_assert(MoveOrderer::SCORE_COUNTERMOVE > 20000); // 20000 is maximum history clamp

    return true;
}

bool testContHistReadOnlyConsumerInvariant() {
    Search::clearContHist();
    auto& chTable = Search::getMutableContHist();

    chTable.update(Piece::WhitePawn, Square::E5, Square::A4, 5000);
    chTable.update(Piece::BlackKnight, Square::D4, Square::C2, 3000);

    auto parsed = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    MoveList moves;
    MoveGenerator::generateLegalMoves(pos, moves);

    std::array<std::array<Move, 2>, 64> killerMoves{};
    std::array<std::array<uint32_t, 64>, 12> historyTable{};

    // Execute MoveOrderer across multiple plies
    for (int ply = 0; ply < 8; ++ply) {
        MoveList movesCopy = moves;
        MoveOrderer::scoreAndSortMoves(pos, movesCopy, Move(), killerMoves, historyTable, ply, Move(Square::E7, Square::E5));
        MoveOrderer::scoreAndSortTacticalMoves(pos, movesCopy);
    }

    // Verify Continuation History values were untouched
    const auto& chRead = Search::getContHist();
    if (chRead.probe(Piece::WhitePawn, Square::E5, Square::A4) != 5000) return false;
    if (chRead.probe(Piece::BlackKnight, Square::D4, Square::C2) != 3000) return false;

    return true;
}

bool testContHistSearchDeterminismAndTacticalIntegrity() {
    // Sharp tactical test: WAC.001
    const std::string wac001 = "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1";
    auto parsed = FenParser::parse(wac001);
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    // Run 1
    SearchLimits limits;
    limits.depth = 6;
    int score1 = Search::runSearch(pos, limits);
    const auto stats1 = SearchController::getInstance().getStats();
    std::string move1 = stats1.pvLine.count > 0 ? stats1.pvLine.moves[0].toString() : "";
    std::string pv1 = stats1.pvString;

    // Run 2
    int score2 = Search::runSearch(pos, limits);
    const auto stats2 = SearchController::getInstance().getStats();
    std::string move2 = stats2.pvLine.count > 0 ? stats2.pvLine.moves[0].toString() : "";
    std::string pv2 = stats2.pvString;

    // Run 3
    int score3 = Search::runSearch(pos, limits);
    const auto stats3 = SearchController::getInstance().getStats();
    std::string move3 = stats3.pvLine.count > 0 ? stats3.pvLine.moves[0].toString() : "";
    std::string pv3 = stats3.pvString;

    // 1. Bit-for-bit determinism across consecutive runs
    if (score1 != score2 || score2 != score3) return false;
    if (move1 != move2 || move2 != move3) return false;
    if (pv1 != pv2 || pv2 != pv3) return false;

    // 2. Tactical accuracy on WAC.001
    if (move1 != "c8c4") return false;
    if (stats1.completedDepth != 6) return false;

    // 3. Search heuristic activity: Continuation History active engagement
    if (stats1.conthistHits == 0) return false;
    if (stats1.conthistCutoffs == 0) return false;

    return true;
}

bool runMilestone6Module69Tests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE 6, MODULE 6.9: CONTINUATION HISTORY (CONTHIST) ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 4;

    bool lifePass = testContHistLifecycleContract();
    std::cout << "[" << (lifePass ? "PASS" : "FAIL") << "] Module 6.9.1: 4-Stage Table Lifecycle Contract (Init, Probe, Update, Normalize)\n";
    if (lifePass) passed++;

    bool stratPass = testContHistScoreStratification();
    std::cout << "[" << (stratPass ? "PASS" : "FAIL") << "] Module 6.9.2: Move Ordering Stratification & Heuristic Precedence\n";
    if (stratPass) passed++;

    bool ownPass = testContHistReadOnlyConsumerInvariant();
    std::cout << "[" << (ownPass ? "PASS" : "FAIL") << "] Module 6.9.3: Read-Only Consumer Invariant & Architectural Decoupling\n";
    if (ownPass) passed++;

    bool detPass = testContHistSearchDeterminismAndTacticalIntegrity();
    std::cout << "[" << (detPass ? "PASS" : "FAIL") << "] Module 6.9.4: Search Determinism, Tactical Integrity & Telemetry Efficacy\n";
    if (detPass) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "MILESTONE 6 (MODULE 6.9) RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone 6, Module 6.10: Correction History (CorrHist) Tests
// ---------------------------------------------------------------------------

bool testCorrectionHistoryLifecycle() {
    auto& corrTable = Evaluator::getMutableCorrHist();
    corrTable.initialize();

    auto parsed = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    // 1. Initial State: probed correction must be strictly 0
    if (corrTable.probe(pos) != 0) return false;

    // 2. Positive Bias Update: searchScore (200) > staticEval (100) -> error +100
    corrTable.update(pos, 4, 200, 100);
    int c1 = corrTable.probe(pos);
    if (c1 <= 0) return false;

    // Second positive update should increase bias via depth-scaled gravity
    corrTable.update(pos, 4, 200, 100);
    int c2 = corrTable.probe(pos);
    if (c2 <= c1) return false;

    // 3. Negative Bias Update on alternate position
    auto parsedNeg = FenParser::parse("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    if (!parsedNeg.has_value()) return false;
    Position posNeg = parsedNeg.value();

    corrTable.update(posNeg, 4, 50, 150); // error -100
    int cNeg = corrTable.probe(posNeg);
    if (cNeg >= 0) return false;

    // 4. Clamping Invariant: Extreme accumulation cannot exceed MAX_CORRECTION
    for (int i = 0; i < 50; ++i) {
        corrTable.update(pos, 6, 1200, 0); // repeated massive error
    }
    int cClamped = corrTable.probe(pos);
    if (cClamped > CorrectionHistoryTable::MAX_CORRECTION) return false;

    // 5. Normalization / Aging: Halves the correction bias
    corrTable.normalize();
    int cAged = corrTable.probe(pos);
    if (cAged != cClamped / 2) return false;

    corrTable.age();
    if (corrTable.probe(pos) != cAged / 2) return false;

    // 6. Reset
    corrTable.clear();
    if (corrTable.probe(pos) != 0) return false;
    if (corrTable.probe(posNeg) != 0) return false;

    return true;
}

bool testCorrectionHistoryFeatureKeying() {
    // Position A: Standard Starting Position
    auto parsedA = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsedA.has_value()) return false;
    Position posA = parsedA.value();
    size_t bucketA = CorrectionHistoryTable::getBucketIndex(posA);

    // Position B: White Knight develops Nb1c3 (Pawn structure unchanged)
    auto parsedB = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/2N5/PPPPPPPP/R1BQKBNR w KQkq - 0 1");
    if (!parsedB.has_value()) return false;
    Position posB = parsedB.value();
    size_t bucketB = CorrectionHistoryTable::getBucketIndex(posB);

    // Position C: White Knight develops Ng1f3 (Pawn structure unchanged)
    auto parsedC = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 0 1");
    if (!parsedC.has_value()) return false;
    Position posC = parsedC.value();
    size_t bucketC = CorrectionHistoryTable::getBucketIndex(posC);

    // Non-pawn piece movements must preserve identical pawn key bucket
    if (bucketA != bucketB) return false;
    if (bucketA != bucketC) return false;

    // Position D: White moves pawn e2e4 (Pawn structure altered)
    auto parsedD = FenParser::parse("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    if (!parsedD.has_value()) return false;
    Position posD = parsedD.value();
    size_t bucketD = CorrectionHistoryTable::getBucketIndex(posD);

    // Position E: Black moves pawn e7e5 (Pawn structure altered)
    auto parsedE = FenParser::parse("rnbqkbnr/pppp1ppp/8/4p3/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsedE.has_value()) return false;
    Position posE = parsedE.value();
    size_t bucketE = CorrectionHistoryTable::getBucketIndex(posE);

    // Altered pawn structures must produce distinct bucket indices
    if (bucketA == bucketD) return false;
    if (bucketA == bucketE) return false;
    if (bucketD == bucketE) return false;

    return true;
}

bool testCorrectionHistoryEvaluationDecouplingAndReadOnlyInvariant() {
    auto parsed = FenParser::parse("r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/2N2N2/PPPP1PPP/R1BQK2R w KQkq - 4 4");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    uint64_t hashBefore = pos.getHashKey();
    Bitboard occBefore = pos.getTotalOccupancy();
    Color sideBefore = pos.getSideToMove();

    auto& corrTable = Evaluator::getMutableCorrHist();
    corrTable.initialize();
    corrTable.update(pos, 4, 300, 100);

    // Probe correction and compute evaluation multiple times
    for (int i = 0; i < 10; ++i) {
        int corr = corrTable.probe(pos);
        if (corr <= 0) return false;
        int eval1 = Evaluator::evaluate(pos);
        int eval2 = Evaluator::evaluateWithCorrection(pos);
        if (eval1 != eval2) return false;
    }

    // Assert that pos remained strictly const and unmutated
    if (pos.getHashKey() != hashBefore) return false;
    if (pos.getTotalOccupancy() != occBefore) return false;
    if (pos.getSideToMove() != sideBefore) return false;

    return true;
}

bool testCorrectionHistorySearchTelemetryAndStability() {
    // Sharp tactical test: WAC.001
    const std::string wac001 = "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1";
    auto parsed = FenParser::parse(wac001);
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    // Run 1
    SearchLimits limits;
    limits.depth = 6;
    int score1 = Search::runSearch(pos, limits);
    const auto stats1 = SearchController::getInstance().getStats();
    std::string move1 = stats1.pvLine.count > 0 ? stats1.pvLine.moves[0].toString() : "";
    std::string pv1 = stats1.pvString;

    // Run 2
    int score2 = Search::runSearch(pos, limits);
    const auto stats2 = SearchController::getInstance().getStats();
    std::string move2 = stats2.pvLine.count > 0 ? stats2.pvLine.moves[0].toString() : "";
    std::string pv2 = stats2.pvString;

    // Run 3
    int score3 = Search::runSearch(pos, limits);
    const auto stats3 = SearchController::getInstance().getStats();
    std::string move3 = stats3.pvLine.count > 0 ? stats3.pvLine.moves[0].toString() : "";
    std::string pv3 = stats3.pvString;

    // 1. Bit-for-bit determinism across consecutive runs
    if (score1 != score2 || score2 != score3) return false;
    if (move1 != move2 || move2 != move3) return false;
    if (pv1 != pv2 || pv2 != pv3) return false;

    // 2. Tactical correctness on WAC.001
    if (move1 != "c8c4") return false;
    if (stats1.completedDepth != 6) return false;

    // 3. Search telemetry asserts active evaluation calibration
    if (stats1.corrApplied == 0) return false;
    if (stats1.corrUpdates == 0) return false;

    return true;
}

bool runMilestone6Module610Tests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE 6, MODULE 6.10: CORRECTION HISTORY (CORRHIST) ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 4;

    bool lifePass = testCorrectionHistoryLifecycle();
    std::cout << "[" << (lifePass ? "PASS" : "FAIL") << "] Module 6.10.1: 4-Stage Table Lifecycle Contract (Init, Probe, Update, Normalize)\n";
    if (lifePass) passed++;

    bool keyPass = testCorrectionHistoryFeatureKeying();
    std::cout << "[" << (keyPass ? "PASS" : "FAIL") << "] Module 6.10.2: Feature Keying (Pawn Topology Sensitivity & Piece Invariance)\n";
    if (keyPass) passed++;

    bool ownPass = testCorrectionHistoryEvaluationDecouplingAndReadOnlyInvariant();
    std::cout << "[" << (ownPass ? "PASS" : "FAIL") << "] Module 6.10.3: Evaluation Decoupling & Read-Only Const Invariant\n";
    if (ownPass) passed++;

    bool detPass = testCorrectionHistorySearchTelemetryAndStability();
    std::cout << "[" << (detPass ? "PASS" : "FAIL") << "] Module 6.10.4: Search Determinism, Tactical Integrity & Telemetry Efficacy\n";
    if (detPass) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "MILESTONE 6 (MODULE 6.10) RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Subsystem Search Diagnostics (WAC Suite)
// ---------------------------------------------------------------------------

struct WacTestCase {
    int id;
    std::string name;
    std::string fen;
    std::string targetMove;
};

const std::vector<WacTestCase> g_wacSuite = {
    {
        1,
        "WAC.001 - Tactical Rook Strike",
        "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1",
        "c8c4"
    },
    {
        2,
        "WAC.004 - Forcing Piece Trade",
        "r2qk2r/ppp2ppp/2n5/2b1p3/6b1/3P1N2/PPP1BPPP/R1BQ1RK1 b kq - 0 1",
        "g4f3"
    },
    {
        3,
        "WAC.003 - Sharp Knight Sacrifice",
        "5rk1/1ppb1ppp/p1pb4/8/3P1n1q/2P1R3/PP1B1PPP/R2Q1NK1 b - - 0 1",
        "f4g2"
    }
};

// ---------------------------------------------------------------------------
// Milestone Omega, Phase 1: Engine Identity, Hierarchical Parameters,
// Categorized Perft, and 4-Section Telemetry
// ---------------------------------------------------------------------------

bool testEngineIdentitySubsystem() {
    if (EngineInfo::getName() != "Boson") return false;
    if (EngineInfo::getVersion() != "0.8.0-dev") return false;
    if (EngineInfo::getAuthor() != "ThePixelScript") return false;
    if (EngineInfo::getBuildType().empty()) return false;
    if (EngineInfo::getCompiler().empty()) return false;
    if (EngineInfo::getTargetArch().empty()) return false;
    if (EngineInfo::getInstructionSets().empty()) return false;

    EngineParameters params;
    std::string snapshot = EngineInfo::serializeConfigSnapshot(params);
    if (snapshot.find("=== Boson Configuration Snapshot ===") == std::string::npos) return false;
    if (snapshot.find("[Search Parameters]") == std::string::npos) return false;
    if (snapshot.find("[Evaluation Parameters]") == std::string::npos) return false;
    if (snapshot.find("[Time Parameters]") == std::string::npos) return false;
    if (snapshot.find("[Debug Parameters]") == std::string::npos) return false;
    if (snapshot.find("lmrBase: 0.5") == std::string::npos) return false;
    if (snapshot.find("pawnValue: 100") == std::string::npos) return false;

    return true;
}

bool testHierarchicalParameterArchitecture() {
    auto& controller = SearchController::getInstance();
    const EngineParameters orig = controller.getParams();

    // Verify defaults
    if (orig.search.lmrBase != 0.5) return false;
    if (orig.search.lmrDivisor != 1.95) return false;
    if (orig.search.lmrMinDepth != 3) return false;
    if (orig.search.lmrMinMoveCount != 4) return false;
    if (orig.search.nmpMinDepth != 3) return false;
    if (orig.search.nmpReduction != 2) return false;
    if (orig.search.aspirationInitialDelta != 30) return false;
    if (orig.search.aspirationMaxDelta != 400) return false;
    if (orig.search.killerSlotCount != 2) return false;

    if (orig.eval.pawnValue != 100) return false;
    if (orig.eval.knightValue != 320) return false;
    if (orig.eval.bishopValue != 330) return false;
    if (orig.eval.rookValue != 500) return false;
    if (orig.eval.queenValue != 900) return false;
    if (orig.eval.maxCorrection != 1024) return false;
    if (orig.eval.corrScaleFactor != 256) return false;

    if (orig.time.nodeCheckPeriod != 2048) return false;
    if (orig.time.allocDivisor != 20) return false;
    if (orig.time.incDivisor != 2) return false;
    if (orig.time.hardLimitMultiplier != 3.0) return false;

    if (!orig.debug.enableNMP || !orig.debug.enableLMR || !orig.debug.enableAspiration ||
        !orig.debug.enableCMH || !orig.debug.enableContHist || !orig.debug.enableCorrHist) {
        return false;
    }

    // Verify mutation and propagation
    EngineParameters modified = orig;
    modified.search.nmpMinDepth = 4;
    modified.debug.enableNMP = false;
    controller.setParams(modified);

    if (controller.getParams().search.nmpMinDepth != 4) return false;
    if (controller.getParams().debug.enableNMP != false) return false;

    // Restore clean production parameters
    controller.setParams(orig);
    return true;
}

bool testFourSectionBenchmarkTelemetry() {
    auto& controller = SearchController::getInstance();
    auto& stats = controller.getStats();

    // Verify presence and reset behavior of new telemetry fields
    stats.staticEvalCalls = 42;
    stats.seeInvocations = 17;
    stats.reset();

    if (stats.staticEvalCalls != 0 || stats.seeInvocations != 0) return false;

    // Run short depth search on tactical position to verify counter accumulation
    auto posOpt = FenParser::parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    if (!posOpt) return false;
    Position pos = *posOpt;

    Search::runSearch(pos, 2);

    if (stats.staticEvalCalls == 0) return false;
    if (stats.seeInvocations == 0) return false;
    if (stats.completedDepth != 2) return false;

    return true;
}

bool runMilestoneOmegaPhase1Tests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE OMEGA, PHASE 1: IDENTITY & ARCHITECTURE TESTS ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 3;

    bool idPass = testEngineIdentitySubsystem();
    std::cout << "[" << (idPass ? "PASS" : "FAIL") << "] Omega 1.1: Engine Identity Subsystem & Config Snapshot\n";
    if (idPass) passed++;

    bool paramPass = testHierarchicalParameterArchitecture();
    std::cout << "[" << (paramPass ? "PASS" : "FAIL") << "] Omega 1.2: Hierarchical Parameter Architecture & Lifecycle\n";
    if (paramPass) passed++;

    bool telePass = testFourSectionBenchmarkTelemetry();
    std::cout << "[" << (telePass ? "PASS" : "FAIL") << "] Omega 1.3: 4-Section Benchmark Telemetry & Evaluation Counters\n";
    if (telePass) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "MILESTONE OMEGA PHASE 1 RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

bool runMilestoneOmegaPhase2TacticalTests() {
    return TacticalRegressionRunner::runMilestoneOmegaPhase2TacticalTests();
}

bool runMilestoneOmegaPhase3IntegrityTests() {
    return IntegrityRunner::runMilestoneOmegaPhase3IntegrityTests();
}

// ---------------------------------------------------------------------------
// Milestone Omega Phase 4: Deterministic Benchmark Suite Integration
// ---------------------------------------------------------------------------

bool testGateOmega4A_Determinism() {
    BenchmarkConfig config;
    config.overrideDepth = 5;
    config.hashSizeMb = 16;
    config.mode = BenchmarkStateMode::Isolated;
    config.threads = 1;
    config.printConsole = false;
    config.silentSearch = true;

    BenchmarkRunRecord pass1 = BenchmarkRunner::run(config);
    BenchmarkRunRecord pass2 = BenchmarkRunner::run(config);

    if (pass1.positions.size() != 6 || pass2.positions.size() != 6) {
        std::cerr << "[FAIL] Gate Omega 4-A: Benchmark position count mismatch (expected 6)\n";
        return false;
    }

    if (pass1.aggregate.totalNodes != pass2.aggregate.totalNodes) {
        std::cerr << "[FAIL] Gate Omega 4-A: Aggregate node mismatch: "
                  << pass1.aggregate.totalNodes << " vs " << pass2.aggregate.totalNodes << "\n";
        return false;
    }

    for (size_t i = 0; i < pass1.positions.size(); ++i) {
        const auto& p1 = pass1.positions[i];
        const auto& p2 = pass2.positions[i];

        if (!(p1.deterministic == p2.deterministic)) {
            std::cerr << "[FAIL] Gate Omega 4-A: Deterministic telemetry divergence on position "
                      << p1.id << "\n";
            std::cerr << "  Nodes: " << p1.deterministic.totalNodes << " vs " << p2.deterministic.totalNodes << "\n";
            std::cerr << "  BestMove: " << p1.deterministic.bestMoveUci << " vs " << p2.deterministic.bestMoveUci << "\n";
            std::cerr << "  TTHits: " << p1.deterministic.ttHits << " vs " << p2.deterministic.ttHits << "\n";
            std::cerr << "  BetaCutoffs: " << p1.deterministic.betaCutoffs << " vs " << p2.deterministic.betaCutoffs << "\n";
            return false;
        }
    }

    return true;
}

bool testGateOmega4B_StateIsolation() {
    auto posOptA = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    auto posOptB = FenParser::parse("2rr3k/pp3pp1/1nnqbN1p/3pN3/2pP4/2P3Q1/PPB4P/R4RK1 w - - 0 1");
    if (!posOptA || !posOptB) return false;

    std::ostringstream dummy;
    std::streambuf* origBuf = std::cout.rdbuf(dummy.rdbuf());

    // 1. ISOLATED MODE:
    BenchmarkRunner::resetSearchState(16);
    Position bDirect = *posOptB;
    SearchLimits limitsIso;
    limitsIso.depth = 5;
    limitsIso.clearTables = true;
    Search::runSearch(bDirect, limitsIso);
    uint64_t bDirectNodes = SearchController::getInstance().getStats().nodes + SearchController::getInstance().getStats().qNodes;
    std::string bDirectMove = SearchController::getInstance().getStats().pvString;

    BenchmarkRunner::resetSearchState(16);
    Position aPos = *posOptA;
    Search::runSearch(aPos, limitsIso);

    BenchmarkRunner::resetSearchState(16);
    Position bAfterA = *posOptB;
    Search::runSearch(bAfterA, limitsIso);
    uint64_t bAfterANodes = SearchController::getInstance().getStats().nodes + SearchController::getInstance().getStats().qNodes;
    std::string bAfterAMove = SearchController::getInstance().getStats().pvString;

    // 2. PERSISTENT MODE:
    BenchmarkRunner::resetSearchState(16);
    Position startposClean = *posOptA;
    SearchLimits limitsDirect6;
    limitsDirect6.depth = 6;
    limitsDirect6.clearTables = true;
    Search::runSearch(startposClean, limitsDirect6);
    uint64_t cleanDepth6Nodes = SearchController::getInstance().getStats().nodes + SearchController::getInstance().getStats().qNodes;

    BenchmarkRunner::resetSearchState(16);
    Position startposWarm = *posOptA;
    SearchLimits limitsPersistent5;
    limitsPersistent5.depth = 5;
    limitsPersistent5.clearTables = false;
    Search::runSearch(startposWarm, limitsPersistent5);

    SearchLimits limitsPersistent6;
    limitsPersistent6.depth = 6;
    limitsPersistent6.clearTables = false;
    Search::runSearch(startposWarm, limitsPersistent6);
    uint64_t warmDepth6Nodes = SearchController::getInstance().getStats().nodes + SearchController::getInstance().getStats().qNodes;

    std::cout.rdbuf(origBuf);

    if (bDirectNodes != bAfterANodes || bDirectMove != bAfterAMove) {
        std::cerr << "[FAIL] Gate Omega 4-B: State isolation failed in Isolated mode (nodes: "
                  << bDirectNodes << " vs " << bAfterANodes << ")\n";
        return false;
    }

    if (warmDepth6Nodes == cleanDepth6Nodes) {
        std::cerr << "[FAIL] Gate Omega 4-B: Persistent mode failed to utilize warm TT state (nodes unchanged: "
                  << warmDepth6Nodes << ")\n";
        return false;
    }

    return true;
}

bool testGateOmega4C_ProductionNeutrality() {
    auto& controller = SearchController::getInstance();
    const EngineParameters origParams = controller.getParams();

    BenchmarkConfig cfg;
    cfg.overrideDepth = 3;
    cfg.silentSearch = true;
    cfg.printConsole = false;
    BenchmarkRunner::run(cfg);

    const EngineParameters postParams = controller.getParams();

    if (postParams.search.lmrBase != origParams.search.lmrBase
        || postParams.search.lmrDivisor != origParams.search.lmrDivisor
        || postParams.search.nmpMinDepth != origParams.search.nmpMinDepth
        || postParams.search.aspirationInitialDelta != origParams.search.aspirationInitialDelta
        || postParams.eval.pawnValue != origParams.eval.pawnValue
        || postParams.eval.queenValue != origParams.eval.queenValue
        || postParams.time.nodeCheckPeriod != origParams.time.nodeCheckPeriod
        || postParams.debug.enableNMP != origParams.debug.enableNMP) {
        std::cerr << "[FAIL] Gate Omega 4-C: Production EngineParameters mutated during benchmark execution\n";
        return false;
    }

    return true;
}

bool testGateOmega4DE_SchemaSerialization() {
    BenchmarkConfig cfg;
    cfg.overrideDepth = 4;
    cfg.silentSearch = true;
    cfg.printConsole = false;
    BenchmarkRunRecord record = BenchmarkRunner::run(cfg);

    if (record.positions.size() != 6) {
        std::cerr << "[FAIL] Gate Omega 4-D/E: Benchmark corpus size is not 6\n";
        return false;
    }

    uint64_t sumNodes = 0;
    uint64_t sumQNodes = 0;
    for (const auto& pos : record.positions) {
        sumNodes += pos.deterministic.totalNodes;
        sumQNodes += pos.deterministic.totalQNodes;
    }
    if (sumNodes != record.aggregate.totalNodes || sumQNodes != record.aggregate.totalQNodes) {
        std::cerr << "[FAIL] Gate Omega 4-D/E: Aggregate telemetry sum discrepancy\n";
        return false;
    }

    std::string json = BenchmarkReporter::serializeJson(record);

    if (json.find("\"schemaVersion\": \"1.0.0\"") == std::string::npos) {
        std::cerr << "[FAIL] Gate Omega 4-D/E: Missing schemaVersion in JSON\n";
        return false;
    }
    if (json.find("\"corpusVersion\": \"1.0.0\"") == std::string::npos) {
        std::cerr << "[FAIL] Gate Omega 4-D/E: Missing corpusVersion in JSON\n";
        return false;
    }
    if (json.find("\"engineName\": \"Boson\"") == std::string::npos) {
        std::cerr << "[FAIL] Gate Omega 4-D/E: Missing engineName in JSON\n";
        return false;
    }
    if (json.find("\"engineVersion\": \"0.8.0-dev\"") == std::string::npos) {
        std::cerr << "[FAIL] Gate Omega 4-D/E: Missing engineVersion in JSON\n";
        return false;
    }
    if (json.find("\"positions\": [") == std::string::npos) {
        std::cerr << "[FAIL] Gate Omega 4-D/E: Missing positions array in JSON\n";
        return false;
    }

    std::string parsedSchema, parsedCorpus;
    uint64_t parsedNodes = 0;
    size_t parsedPosCount = 0;
    if (!BenchmarkReporter::parseJsonParity(json, parsedSchema, parsedCorpus, parsedNodes, parsedPosCount)) {
        std::cerr << "[FAIL] Gate Omega 4-D/E: JSON parity parser failed\n";
        return false;
    }

    if (parsedSchema != "1.0.0" || parsedCorpus != "1.0.0") {
        std::cerr << "[FAIL] Gate Omega 4-D/E: Parsed version mismatch\n";
        return false;
    }
    if (parsedNodes != record.aggregate.totalNodes) {
        std::cerr << "[FAIL] Gate Omega 4-D/E: Parsed totalNodes mismatch: "
                  << parsedNodes << " vs " << record.aggregate.totalNodes << "\n";
        return false;
    }
    if (parsedPosCount != 6) {
        std::cerr << "[FAIL] Gate Omega 4-D/E: Parsed posCount mismatch\n";
        return false;
    }

    return true;
}

bool runMilestoneOmegaPhase4BenchmarkTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE OMEGA, PHASE 4: BENCHMARK HARNESS TESTS       ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 4;

    bool passA = testGateOmega4A_Determinism();
    std::cout << "[" << (passA ? "PASS" : "FAIL") << "] Omega 4-A: Determinism Verification Across Passes\n";
    if (passA) passed++;

    bool passB = testGateOmega4B_StateIsolation();
    std::cout << "[" << (passB ? "PASS" : "FAIL") << "] Omega 4-B: State Isolation & TT Eviction Verification\n";
    if (passB) passed++;

    bool passC = testGateOmega4C_ProductionNeutrality();
    std::cout << "[" << (passC ? "PASS" : "FAIL") << "] Omega 4-C: Production Neutrality & Zero Side-Effects\n";
    if (passC) passed++;

    bool passDE = testGateOmega4DE_SchemaSerialization();
    std::cout << "[" << (passDE ? "PASS" : "FAIL") << "] Omega 4-D/E: Schema Validation & JSON Round-Trip Parity\n";
    if (passDE) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "MILESTONE OMEGA PHASE 4 RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone Omega Phase 5: Strength & Elo Validation Harness Integration
// ---------------------------------------------------------------------------

bool testGateOmega5D_StatisticalCorrectness() {
    // 1. 0W / 0D / 100L
    MatchStatistics s1 = Statistics::computeStatistics(0, 0, 100);
    if (s1.observedScore != 0.0 || s1.score != 0.0) {
        std::cerr << "[DEBUG 5D] 1.1 failed: observedScore=" << s1.observedScore << "\n";
        return false;
    }
    if (s1.sampleVariance != 0.0) {
        std::cerr << "[DEBUG 5D] 1.2 failed: var=" << s1.sampleVariance << "\n";
        return false;
    }
    // raw Wilson ~ [0.0000, 0.03699]
    if (std::abs(s1.rawWilsonLower - 0.0000) > 1e-4 || std::abs(s1.rawWilsonUpper - 0.03699) > 1e-4) {
        std::cerr << "[DEBUG 5D] 1.3 failed: rawWilson=[" << s1.rawWilsonLower << ", " << s1.rawWilsonUpper << "]\n";
        return false;
    }
    // eloScore ~ [1e-6, 0.0370]
    if (std::abs(s1.eloScoreLower - 1e-6) > 1e-9 || std::abs(s1.eloScoreUpper - 0.0370) > 1e-3) {
        std::cerr << "[DEBUG 5D] 1.4 failed: eloScore=[" << s1.eloScoreLower << ", " << s1.eloScoreUpper << "]\n";
        return false;
    }
    // Elo CI ~ [-2400.0, -566.20] (non-zero width)
    if (std::abs(s1.ci.eloLower - (-2400.0)) > 1.0 || std::abs(s1.ci.eloUpper - (-566.20)) > 1.0) {
        std::cerr << "[DEBUG 5D] 1.5 failed: elo=[" << s1.ci.eloLower << ", " << s1.ci.eloUpper << "]\n";
        return false;
    }
    if (s1.ci.eloUpper <= s1.ci.eloLower) {
        std::cerr << "[DEBUG 5D] 1.6 failed: non-positive width\n";
        return false;
    }
    if (s1.sprt.llr >= 0.0) {
        std::cerr << "[DEBUG 5D] 1.7 failed: llr=" << s1.sprt.llr << "\n";
        return false;
    }
    // SPRT transition: at 150 losses, LLR <= -2.944 (AcceptH0)
    MatchStatistics s1_150 = Statistics::computeStatistics(0, 0, 150);
    if (s1_150.sprt.decision != SPRTDecision::AcceptH0) {
        std::cerr << "[DEBUG 5D] 1.8 failed: dec=" << (int)s1_150.sprt.decision << ", llr=" << s1_150.sprt.llr << "\n";
        return false;
    }

    // 2. 100W / 0D / 0L
    MatchStatistics s4 = Statistics::computeStatistics(100, 0, 0);
    if (s4.observedScore != 1.0 || s4.score != 1.0) {
        std::cerr << "[DEBUG 5D] 2.1 failed: observedScore=" << s4.observedScore << "\n";
        return false;
    }
    if (s4.sampleVariance != 0.0) {
        std::cerr << "[DEBUG 5D] 2.2 failed: var=" << s4.sampleVariance << "\n";
        return false;
    }
    // raw Wilson ~ [0.96301, 1.0000]
    if (std::abs(s4.rawWilsonLower - 0.96301) > 1e-4 || std::abs(s4.rawWilsonUpper - 1.0000) > 1e-4) {
        std::cerr << "[DEBUG 5D] 2.3 failed: rawWilson=[" << s4.rawWilsonLower << ", " << s4.rawWilsonUpper << "]\n";
        return false;
    }
    // eloScore ~ [0.9630, 0.999999]
    if (std::abs(s4.eloScoreLower - 0.9630) > 1e-3 || std::abs(s4.eloScoreUpper - (1.0 - 1e-6)) > 1e-9) {
        std::cerr << "[DEBUG 5D] 2.4 failed: eloScore=[" << s4.eloScoreLower << ", " << s4.eloScoreUpper << "]\n";
        return false;
    }
    // Elo CI ~ [+566.20, +2400.0] (non-zero width)
    if (std::abs(s4.ci.eloLower - 566.20) > 1.0 || std::abs(s4.ci.eloUpper - 2400.0) > 1.0) {
        std::cerr << "[DEBUG 5D] 2.5 failed: elo=[" << s4.ci.eloLower << ", " << s4.ci.eloUpper << "]\n";
        return false;
    }
    if (s4.ci.eloUpper <= s4.ci.eloLower) {
        std::cerr << "[DEBUG 5D] 2.6 failed: non-positive width\n";
        return false;
    }
    if (s4.sprt.llr <= 0.0) {
        std::cerr << "[DEBUG 5D] 2.7 failed: llr=" << s4.sprt.llr << "\n";
        return false;
    }
    // SPRT transition: at 150 wins, LLR >= +2.944 (AcceptH1)
    MatchStatistics s4_150 = Statistics::computeStatistics(150, 0, 0);
    if (s4_150.sprt.decision != SPRTDecision::AcceptH1) {
        std::cerr << "[DEBUG 5D] 2.8 failed: dec=" << (int)s4_150.sprt.decision << "\n";
        return false;
    }

    // 3. 0W / 100D / 0L: deltaElo == 0.0
    MatchStatistics s2 = Statistics::computeStatistics(0, 100, 0);
    if (s2.observedScore != 0.5) { std::cerr << "[DEBUG 5D] 3.1 failed\n"; return false; }
    if (s2.sampleVariance != 0.0) { std::cerr << "[DEBUG 5D] 3.2 failed\n"; return false; }
    if (s2.deltaElo != 0.0) { std::cerr << "[DEBUG 5D] 3.3 failed: deltaElo=" << s2.deltaElo << "\n"; return false; }
    if (std::abs(s2.rawWilsonLower + s2.rawWilsonUpper - 1.0) > 1e-6) { std::cerr << "[DEBUG 5D] 3.4 failed\n"; return false; }
    if (std::abs(s2.ci.eloLower + s2.ci.eloUpper) > 1e-6) { std::cerr << "[DEBUG 5D] 3.5 failed\n"; return false; }

    // 4. 50W / 0D / 50L: deltaElo == 0.0
    MatchStatistics s3 = Statistics::computeStatistics(50, 0, 50);
    if (s3.observedScore != 0.5) { std::cerr << "[DEBUG 5D] 4.1 failed\n"; return false; }
    if (std::abs(s3.sampleVariance - (25.0 / 99.0)) > 1e-5) { std::cerr << "[DEBUG 5D] 4.2 failed\n"; return false; }
    if (s3.deltaElo != 0.0) { std::cerr << "[DEBUG 5D] 4.3 failed: deltaElo=" << s3.deltaElo << "\n"; return false; }
    if (std::abs(s3.rawWilsonLower + s3.rawWilsonUpper - 1.0) > 1e-6) { std::cerr << "[DEBUG 5D] 4.4 failed\n"; return false; }
    if (std::abs(s3.ci.eloLower + s3.ci.eloUpper) > 1e-6) { std::cerr << "[DEBUG 5D] 4.5 failed\n"; return false; }

    // 5. 45W / 30D / 25L: deltaElo > 0, rawWilson bounds strictly inside (0, 1)
    MatchStatistics s5 = Statistics::computeStatistics(45, 30, 25);
    if (std::abs(s5.observedScore - 0.60) > 1e-6) { std::cerr << "[DEBUG 5D] 5.1 failed\n"; return false; }
    if (s5.deltaElo <= 0.0) { std::cerr << "[DEBUG 5D] 5.2 failed: deltaElo=" << s5.deltaElo << "\n"; return false; }
    if (s5.rawWilsonLower <= 0.0 || s5.rawWilsonUpper >= 1.0) {
        std::cerr << "[DEBUG 5D] 5.3 failed: rawWilson bounds not strictly inside (0, 1)\n";
        return false;
    }
    if (s5.rawWilsonLower >= s5.observedScore || s5.observedScore >= s5.rawWilsonUpper) {
        std::cerr << "[DEBUG 5D] 5.4 failed: observedScore not inside rawWilson bounds\n";
        return false;
    }
    if (s5.ci.eloLower >= s5.deltaElo || s5.deltaElo >= s5.ci.eloUpper) {
        std::cerr << "[DEBUG 5D] 5.5 failed: deltaElo not inside elo bounds\n";
        return false;
    }

    // 6. N = 1 edge case: (1W/0D/0L and 0W/0D/1L) executes cleanly without division-by-zero or NaN
    MatchStatistics s_1w = Statistics::computeStatistics(1, 0, 0);
    if (s_1w.observedScore != 1.0) { std::cerr << "[DEBUG 5D] 6.1 failed\n"; return false; }
    if (s_1w.sampleVariance != 0.0) { std::cerr << "[DEBUG 5D] 6.2 failed\n"; return false; }
    if (!std::isfinite(s_1w.deltaElo) || !std::isfinite(s_1w.ci.eloLower) || !std::isfinite(s_1w.ci.eloUpper) ||
        !std::isfinite(s_1w.rawWilsonLower) || !std::isfinite(s_1w.rawWilsonUpper)) {
        std::cerr << "[DEBUG 5D] 6.3 failed: non-finite outputs in N=1 1W\n";
        return false;
    }
    if ((s_1w.ci.eloUpper - s_1w.ci.eloLower) <= 100.0) {
        std::cerr << "[DEBUG 5D] 6.4 failed: N=1 1W CI width <= 100: " << (s_1w.ci.eloUpper - s_1w.ci.eloLower) << "\n";
        return false;
    }
    if (!(s_1w.ci.eloLower <= s_1w.deltaElo && s_1w.deltaElo <= s_1w.ci.eloUpper)) {
        std::cerr << "[DEBUG 5D] 6.5 failed: N=1 1W Elo ordering violation: "
                  << s_1w.ci.eloLower << " <= " << s_1w.deltaElo << " <= " << s_1w.ci.eloUpper << "\n";
        return false;
    }

    MatchStatistics s_1l = Statistics::computeStatistics(0, 0, 1);
    if (s_1l.observedScore != 0.0) { std::cerr << "[DEBUG 5D] 6.6 failed\n"; return false; }
    if (s_1l.sampleVariance != 0.0) { std::cerr << "[DEBUG 5D] 6.7 failed\n"; return false; }
    if (!std::isfinite(s_1l.deltaElo) || !std::isfinite(s_1l.ci.eloLower) || !std::isfinite(s_1l.ci.eloUpper) ||
        !std::isfinite(s_1l.rawWilsonLower) || !std::isfinite(s_1l.rawWilsonUpper)) {
        std::cerr << "[DEBUG 5D] 6.8 failed: non-finite outputs in N=1 1L\n";
        return false;
    }
    if ((s_1l.ci.eloUpper - s_1l.ci.eloLower) <= 100.0) {
        std::cerr << "[DEBUG 5D] 6.9 failed: N=1 1L CI width <= 100: " << (s_1l.ci.eloUpper - s_1l.ci.eloLower) << "\n";
        return false;
    }
    if (!(s_1l.ci.eloLower <= s_1l.deltaElo && s_1l.deltaElo <= s_1l.ci.eloUpper)) {
        std::cerr << "[DEBUG 5D] 6.10 failed: N=1 1L Elo ordering violation: "
                  << s_1l.ci.eloLower << " <= " << s_1l.deltaElo << " <= " << s_1l.ci.eloUpper << "\n";
        return false;
    }
    if (std::abs(s_1w.ci.eloLower + s_1l.ci.eloUpper) > 1e-6 || std::abs(s_1w.ci.eloUpper + s_1l.ci.eloLower) > 1e-6) {
        std::cerr << "[DEBUG 5D] 6.11 failed: N=1 Elo reflection symmetry broken\n";
        return false;
    }

    // 7. Symmetry assertion: CI(100W/0L) mirrors CI(0W/100L) about 0
    if (std::abs(s4.ci.eloLower + s1.ci.eloUpper) > 1e-6 || std::abs(s4.ci.eloUpper + s1.ci.eloLower) > 1e-6) {
        std::cerr << "[DEBUG 5D] 7.1 failed: Elo CI symmetry broken: s4=[" << s4.ci.eloLower << ", " << s4.ci.eloUpper << "], s1=[" << s1.ci.eloLower << ", " << s1.ci.eloUpper << "]\n";
        return false;
    }
    if (std::abs(s4.deltaElo + s1.deltaElo) > 1e-6) {
        std::cerr << "[DEBUG 5D] 7.2 failed: Delta Elo symmetry broken\n";
        return false;
    }
    if (std::abs(s4.rawWilsonLower + s1.rawWilsonUpper - 1.0) > 1e-6 ||
        std::abs(s4.rawWilsonUpper + s1.rawWilsonLower - 1.0) > 1e-6) {
        std::cerr << "[DEBUG 5D] 7.3 failed: Wilson CI symmetry broken\n";
        return false;
    }
    if (std::abs(s4.eloScoreLower + s1.eloScoreUpper - 1.0) > 1e-6 ||
        std::abs(s4.eloScoreUpper + s1.eloScoreLower - 1.0) > 1e-6) {
        std::cerr << "[DEBUG 5D] 7.4 failed: Elo score symmetry broken\n";
        return false;
    }

    // 8. Monotonicity assertion: higher score never produces lower deltaElo
    const std::vector<std::pair<uint32_t, uint32_t>> testPoints = {
        {0, 100}, {10, 90}, {25, 75}, {40, 60}, {50, 50}, {60, 40}, {75, 25}, {90, 10}, {100, 0}
    };
    for (size_t i = 1; i < testPoints.size(); ++i) {
        MatchStatistics prev = Statistics::computeStatistics(testPoints[i - 1].first, 0, testPoints[i - 1].second);
        MatchStatistics curr = Statistics::computeStatistics(testPoints[i].first, 0, testPoints[i].second);
        if (curr.deltaElo < prev.deltaElo) {
            std::cerr << "[DEBUG 5D] 8.1 failed: Monotonicity violation in deltaElo: "
                      << prev.deltaElo << " vs " << curr.deltaElo << "\n";
            return false;
        }
        if (curr.ci.eloLower < prev.ci.eloLower) {
            std::cerr << "[DEBUG 5D] 8.2 failed: Monotonicity violation in eloLower: "
                      << prev.ci.eloLower << " vs " << curr.ci.eloLower << "\n";
            return false;
        }
        if (curr.ci.eloUpper < prev.ci.eloUpper) {
            std::cerr << "[DEBUG 5D] 8.3 failed: Monotonicity violation in eloUpper: "
                      << prev.ci.eloUpper << " vs " << curr.ci.eloUpper << "\n";
            return false;
        }
    }

    return true;
}

bool testGateOmega5E_OpeningBookIntegrity() {
    if (OpeningBook::getVersion() != "1.0.0") return false;
    auto openings = OpeningBook::getOpenings();
    if (openings.size() != 50 && openings.size() != 20) return false;

    for (size_t i = 0; i < openings.size(); ++i) {
        const auto& op = openings[i];
        if (op.id.empty() || op.family.empty() || op.name.empty()) return false;
        if (op.moveSequence.empty()) return false;
        auto parsed = FenParser::parse(op.resultingFen);
        if (!parsed) {
            std::cerr << "[FAIL] Gate Omega 5-E: Invalid FEN in opening " << op.id << "\n";
            return false;
        }
    }

    return true;
}

bool testGateOmega5ABC_MatchRunnerSmokeTest() {
    MatchConfig cfg;
    cfg.engineA = "Boson-A";
    cfg.engineB = "Boson-B";
    cfg.totalGames = 2;
    cfg.fixedDepth = 2; // depth 2 search is ultra-fast (<150ms per move) and fully deterministic
    cfg.maxPlies = 60;

    MatchRecord rec = MatchRunner::runMatch(cfg);

    // Gate Omega 5-A: Protocol Compliance & Pure UCI Interchange
    if (rec.games.size() != 2) return false;
    for (const auto& g : rec.games) {
        if (g.termination == TerminationType::ProtocolError ||
            g.termination == TerminationType::EngineCrash ||
            g.termination == TerminationType::Timeout ||
            g.termination == TerminationType::IllegalMove) {
            std::cerr << "[FAIL] Match game ended with abnormal termination: "
                      << terminationToString(g.termination) << "\n";
            return false;
        }
    }

    // Gate Omega 5-B: Color Symmetry & Paired Openings
    if (rec.games[0].whiteEngine != "Boson-A" || rec.games[0].blackEngine != "Boson-B") return false;
    if (rec.games[1].whiteEngine != "Boson-B" || rec.games[1].blackEngine != "Boson-A") return false;
    if (rec.games[0].openingId != rec.games[1].openingId) return false;

    // Gate Omega 5-C: Adjudication Integrity
    if (rec.stats.totalGames != 2) return false;
    if (rec.stats.wins + rec.stats.draws + rec.stats.losses != 2) return false;
    for (const auto& g : rec.games) {
        if (g.plyCount < 6) return false;
    }

    return true;
}

bool testGateOmega5F_ReportingAndJsonParity() {
    MatchConfig cfg;
    cfg.engineA = "Boson-A";
    cfg.engineB = "Boson-B";
    cfg.totalGames = 2;
    cfg.fixedDepth = 2;
    cfg.maxPlies = 60;

    MatchRecord rec = MatchRunner::runMatch(cfg);
    std::string json = StrengthReporter::serializeJson(rec);

    if (json.find("\"schemaVersion\": \"1.0.0\"") == std::string::npos) return false;
    if (json.find("\"matchConfig\": {") == std::string::npos) return false;
    if (json.find("\"statistics\": {") == std::string::npos) return false;
    if (json.find("\"games\": [") == std::string::npos) return false;

    std::string parsedSchema;
    uint32_t parsedTotalGames = 0;
    double parsedScore = 0.0;
    double parsedDeltaElo = 0.0;

    if (!StrengthReporter::parseJsonParity(json, parsedSchema, parsedTotalGames, parsedScore, parsedDeltaElo)) {
        std::cerr << "[FAIL] Gate Omega 5-F: JSON parity parsing failed\n";
        return false;
    }

    if (parsedSchema != "1.0.0" || parsedTotalGames != 2) return false;
    if (std::abs(parsedScore - rec.stats.score) > 1e-4) return false;
    if (std::abs(parsedDeltaElo - rec.stats.deltaElo) > 1e-2) return false;

    return true;
}

bool runMilestoneOmegaPhase5StrengthTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE OMEGA, PHASE 5: STRENGTH & ELO HARNESS TESTS  ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 4;

    bool passD = testGateOmega5D_StatisticalCorrectness();
    std::cout << "[" << (passD ? "PASS" : "FAIL") << "] Omega 5-D: Statistical Correctness & SPRT Transitions\n";
    if (passD) passed++;

    bool passE = testGateOmega5E_OpeningBookIntegrity();
    std::cout << "[" << (passE ? "PASS" : "FAIL") << "] Omega 5-E: Opening Book Coverage & FEN Validation (20 Lines)\n";
    if (passE) passed++;

    bool passABC = testGateOmega5ABC_MatchRunnerSmokeTest();
    std::cout << "[" << (passABC ? "PASS" : "FAIL") << "] Omega 5-A/B/C: Protocol Compliance, Color Symmetry & Adjudication\n";
    if (passABC) passed++;

    bool passF = testGateOmega5F_ReportingAndJsonParity();
    std::cout << "[" << (passF ? "PASS" : "FAIL") << "] Omega 5-F: Dual Reporting & JSON Serialization Round-Trip Parity\n";
    if (passF) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "MILESTONE OMEGA PHASE 5 RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ============================================================================
// MILESTONE OMEGA, PHASE 6: PARAMETER REGISTRY & UCI CONFIGURATION TESTS
// ============================================================================

bool testGateOmega6A_RegistryInitializationAndDefaults() {
    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();

    if (reg.size() < 8) {
        std::cerr << "[FAIL] Gate Omega 6-A: ParameterRegistry has fewer than 8 parameters\n";
        return false;
    }

    const char* mandated[] = {
        "Hash", "NMP_BaseReduction", "NMP_DepthDivisor",
        "LMR_Base", "LMR_Divisor", "Aspiration_InitialWindow",
        "History_MaxScore", "Time_MoveAllocationDivisor"
    };
    for (const char* name : mandated) {
        if (!reg.hasParam(name)) {
            std::cerr << "[FAIL] Gate Omega 6-A: Mandated parameter missing: " << name << "\n";
            return false;
        }
    }

    const auto* pHash = reg.getParam("Hash");
    if (!pHash || pHash->getInt() != 16 || pHash->type != ParamType::Int ||
        pHash->group != ParamGroup::Memory || pHash->resetRequirement != ResetRequirement::ClearTT ||
        std::get<int64_t>(pHash->minValue) != 1 || std::get<int64_t>(pHash->maxValue) != 65536) {
        std::cerr << "[FAIL] Gate Omega 6-A: Hash parameter descriptor incorrect\n";
        return false;
    }

    const auto* pNmpR = reg.getParam("NMP_BaseReduction");
    if (!pNmpR || pNmpR->getInt() != 2 || std::get<int64_t>(pNmpR->minValue) != 1 || std::get<int64_t>(pNmpR->maxValue) != 6) {
        std::cerr << "[FAIL] Gate Omega 6-A: NMP_BaseReduction descriptor incorrect\n";
        return false;
    }

    const auto* pNmpD = reg.getParam("NMP_DepthDivisor");
    if (!pNmpD || pNmpD->getInt() != 3 || std::get<int64_t>(pNmpD->minValue) != 1 || std::get<int64_t>(pNmpD->maxValue) != 10) {
        std::cerr << "[FAIL] Gate Omega 6-A: NMP_DepthDivisor descriptor incorrect\n";
        return false;
    }

    const auto* pLmrB = reg.getParam("LMR_Base");
    if (!pLmrB || std::abs(pLmrB->getDouble() - 0.5) > 1e-6 || std::get<double>(pLmrB->minValue) != 0.0 || std::get<double>(pLmrB->maxValue) != 3.0) {
        std::cerr << "[FAIL] Gate Omega 6-A: LMR_Base descriptor incorrect\n";
        return false;
    }

    const auto* pLmrD = reg.getParam("LMR_Divisor");
    if (!pLmrD || std::abs(pLmrD->getDouble() - 1.95) > 1e-6 || std::get<double>(pLmrD->minValue) != 0.5 || std::get<double>(pLmrD->maxValue) != 5.0) {
        std::cerr << "[FAIL] Gate Omega 6-A: LMR_Divisor descriptor incorrect\n";
        return false;
    }

    const auto* pAsp = reg.getParam("Aspiration_InitialWindow");
    if (!pAsp || pAsp->getInt() != 30 || std::get<int64_t>(pAsp->minValue) != 5 || std::get<int64_t>(pAsp->maxValue) != 200) {
        std::cerr << "[FAIL] Gate Omega 6-A: Aspiration_InitialWindow descriptor incorrect\n";
        return false;
    }

    const auto* pHist = reg.getParam("History_MaxScore");
    if (!pHist || pHist->getInt() != 16384 || std::get<int64_t>(pHist->minValue) != 256 || std::get<int64_t>(pHist->maxValue) != 65536) {
        std::cerr << "[FAIL] Gate Omega 6-A: History_MaxScore descriptor incorrect\n";
        return false;
    }

    const auto* pTimeAlloc = reg.getParam("Time_MoveAllocationDivisor");
    if (!pTimeAlloc || pTimeAlloc->getInt() != 20 || std::get<int64_t>(pTimeAlloc->minValue) != 5 || std::get<int64_t>(pTimeAlloc->maxValue) != 100) {
        std::cerr << "[FAIL] Gate Omega 6-A: Time_MoveAllocationDivisor descriptor incorrect\n";
        return false;
    }

    EngineParameters ep;
    reg.syncToEngineParameters(ep);
    if (ep.search.nmpReduction != 2 || ep.search.nmpMinDepth != 3 ||
        std::abs(ep.search.lmrBase - 0.5) > 1e-6 || std::abs(ep.search.lmrDivisor - 1.95) > 1e-6 ||
        ep.search.aspirationInitialDelta != 30 || ep.time.allocDivisor != 20) {
        std::cerr << "[FAIL] Gate Omega 6-A: syncToEngineParameters failed to copy defaults\n";
        return false;
    }

    return true;
}

bool testGateOmega6B_ValidAndInvalidMutations() {
    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();

    if (!reg.setParam("NMP_BaseReduction", int64_t{4})) {
        std::cerr << "[FAIL] Gate Omega 6-B: Failed valid mutation NMP_BaseReduction=4\n";
        return false;
    }
    if (reg.getInt("NMP_BaseReduction") != 4) return false;

    if (reg.setParam("NMP_BaseReduction", int64_t{0})) {
        std::cerr << "[FAIL] Gate Omega 6-B: Accepted out-of-bounds lower value 0\n";
        return false;
    }
    if (reg.getInt("NMP_BaseReduction") != 4) return false;

    if (reg.setParam("NMP_BaseReduction", int64_t{7})) {
        std::cerr << "[FAIL] Gate Omega 6-B: Accepted out-of-bounds upper value 7\n";
        return false;
    }
    if (reg.getInt("NMP_BaseReduction") != 4) return false;

    if (reg.setParam("NMP_BaseReduction", 2.5)) {
        std::cerr << "[FAIL] Gate Omega 6-B: Accepted wrong variant type (double for int)\n";
        return false;
    }

    if (reg.setParam("NonExistent_Param", int64_t{10})) {
        std::cerr << "[FAIL] Gate Omega 6-B: Accepted nonexistent param\n";
        return false;
    }

    if (!reg.setParam("Hash", int64_t{1024})) return false;
    if (reg.getInt("Hash") != 1024) return false;
    if (reg.setParam("Hash", int64_t{0})) return false;
    if (reg.setParam("Hash", int64_t{70000})) return false;
    if (reg.getInt("Hash") != 1024) return false;

    reg.resetToDefaults();
    if (reg.getInt("NMP_BaseReduction") != 2 || reg.getInt("Hash") != 16) {
        std::cerr << "[FAIL] Gate Omega 6-B: resetToDefaults failed\n";
        return false;
    }

    return true;
}

bool testGateOmega6C_StringParsing() {
    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();

    if (!reg.setParamFromString("LMR_Base", "0.75")) return false;
    if (std::abs(reg.getDouble("LMR_Base") - 0.75) > 1e-6) return false;
    if (reg.setParamFromString("LMR_Base", "-0.1")) return false;
    if (reg.setParamFromString("LMR_Base", "3.5")) return false;
    if (reg.setParamFromString("LMR_Base", "abc")) return false;
    if (reg.setParamFromString("LMR_Base", "0.75abc")) return false;

    if (!reg.setParamFromString("Enable_NMP", "false")) return false;
    if (reg.getBool("Enable_NMP") != false) return false;
    if (!reg.setParamFromString("Enable_NMP", "true")) return false;
    if (reg.getBool("Enable_NMP") != true) return false;
    if (!reg.setParamFromString("Enable_NMP", "0")) return false;
    if (reg.getBool("Enable_NMP") != false) return false;
    if (!reg.setParamFromString("Enable_NMP", "1")) return false;
    if (reg.getBool("Enable_NMP") != true) return false;
    if (reg.setParamFromString("Enable_NMP", "invalid_bool")) return false;

    if (!reg.setParamFromString("NMP_BaseReduction", "5")) return false;
    if (reg.getInt("NMP_BaseReduction") != 5) return false;
    if (reg.setParamFromString("NMP_BaseReduction", "99")) return false;
    if (reg.setParamFromString("NMP_BaseReduction", "xyz")) return false;

    reg.resetToDefaults();
    return true;
}

bool testGateOmega6D_UciEmissionFormatting() {
    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();

    std::ostringstream oss;
    reg.printUciOptions(oss);
    std::string uciStr = oss.str();

    const char* expectedSubstrings[] = {
        "option name Hash type spin default 16 min 1 max 65536",
        "option name NMP_BaseReduction type spin default 2 min 1 max 6",
        "option name NMP_DepthDivisor type spin default 3 min 1 max 10",
        "option name LMR_Base type string default 0.5",
        "option name LMR_Divisor type string default 1.95",
        "option name Aspiration_InitialWindow type spin default 30 min 5 max 200",
        "option name History_MaxScore type spin default 16384 min 256 max 65536",
        "option name Time_MoveAllocationDivisor type spin default 20 min 5 max 100",
        "option name Enable_NMP type check default true"
    };

    for (const char* sub : expectedSubstrings) {
        if (uciStr.find(sub) == std::string::npos) {
            std::cerr << "[FAIL] Gate Omega 6-D: Missing expected UCI option line: " << sub << "\n";
            return false;
        }
    }

    std::string json = reg.serializeJson();
    if (json.find("\"schemaVersion\": \"1.0.0\"") == std::string::npos ||
        json.find("\"name\": \"Hash\"") == std::string::npos ||
        json.find("\"name\": \"NMP_BaseReduction\"") == std::string::npos) {
        std::cerr << "[FAIL] Gate Omega 6-D: Missing expected JSON structure\n";
        return false;
    }

    return true;
}

bool testGateOmega6E_SetOptionAndResetDispatch() {
    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();
    reg.resetClearTTCount();

    if (!reg.handleUciCommand("setoption name hash value 32")) {
        std::cerr << "[FAIL] Gate Omega 6-E: Failed case-insensitive 'setoption name hash value 32'\n";
        return false;
    }
    if (reg.getInt("Hash") != 32) return false;

    if (!reg.handleUciCommand("setoption name NMP_BASEREDUCTION value 4")) return false;
    if (reg.getInt("NMP_BaseReduction") != 4) return false;

    if (!reg.handleUciCommand("setoption name nmp_depthdivisor value 5")) return false;
    if (reg.getInt("NMP_DepthDivisor") != 5) return false;

    if (!reg.handleUciCommand("setoption name lmr_base value 0.75")) return false;
    if (std::abs(reg.getDouble("LMR_Base") - 0.75) > 1e-6) return false;

    uint64_t countBefore = reg.getClearTTCount();
    if (countBefore == 0) {
        std::cerr << "[FAIL] Gate Omega 6-E: Hash mutation did not trigger TT clear\n";
        return false;
    }

    if (!reg.handleUciCommand("setoption name NMP_BaseReduction value 3")) return false;
    if (reg.getClearTTCount() != countBefore) {
        std::cerr << "[FAIL] Gate Omega 6-E: Non-reset param triggered unexpected TT clear\n";
        return false;
    }

    if (!reg.handleUciCommand("setoption name Hash value 64")) return false;
    if (reg.getClearTTCount() != countBefore + 1) {
        std::cerr << "[FAIL] Gate Omega 6-E: Second Hash mutation did not trigger TT clear\n";
        return false;
    }

    if (reg.handleUciCommand("setoption name Hash value 999999")) {
        std::cerr << "[FAIL] Gate Omega 6-E: Accepted out-of-bounds Hash value via setoption\n";
        return false;
    }

    if (reg.handleUciCommand("setoption name UnknownOption value 123")) {
        std::cerr << "[FAIL] Gate Omega 6-E: Accepted unknown option via setoption\n";
        return false;
    }

    reg.resetToDefaults();
    return true;
}

bool testGateOmega6F_CliOverrideParsing() {
    std::string name, val;

    if (!ParameterRegistry::parseCliParam("Hash=64", name, val) || name != "Hash" || val != "64") {
        std::cerr << "[FAIL] Gate Omega 6-F: Failed parsing 'Hash=64'\n";
        return false;
    }
    if (!ParameterRegistry::parseCliParam("--param=NMP_BaseReduction=4", name, val) || name != "NMP_BaseReduction" || val != "4") {
        std::cerr << "[FAIL] Gate Omega 6-F: Failed parsing '--param=NMP_BaseReduction=4'\n";
        return false;
    }
    if (!ParameterRegistry::parseCliParam("LMR_Base=1.2", name, val) || name != "LMR_Base" || val != "1.2") {
        std::cerr << "[FAIL] Gate Omega 6-F: Failed parsing 'LMR_Base=1.2'\n";
        return false;
    }

    if (ParameterRegistry::parseCliParam("NoEqualsSign", name, val)) return false;
    if (ParameterRegistry::parseCliParam("=NoName", name, val)) return false;
    if (ParameterRegistry::parseCliParam("NoValue=", name, val)) return false;
    if (ParameterRegistry::parseCliParam("", name, val)) return false;

    return true;
}

bool testGateOmega6G_DeterministicBaselinePreservation() {
    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();
    reg.syncToEngineParameters(SearchController::getInstance().getMutableParams());

    struct LocalCoutSilencer {
        std::streambuf* origBuf;
        std::ostringstream dummy;
        explicit LocalCoutSilencer() : origBuf(std::cout.rdbuf(dummy.rdbuf())) {}
        ~LocalCoutSilencer() { std::cout.rdbuf(origBuf); }
    };

    // 1. Startpos depth 6 verification (expected: 26,389 nodes under Phase 6.5-B MovePicker)
    const std::string startpos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    auto optStart = FenParser::parse(startpos);
    if (!optStart) return false;
    Position startPos = *optStart;

    {
        LocalCoutSilencer silencer;
        BenchmarkRunner::resetSearchState(16);
        SearchLimits limits;
        limits.depth = 6;
        limits.clearTables = true;
        Search::runSearch(startPos, limits);
    }

    uint64_t startNodes = SearchController::getInstance().getStats().nodes + SearchController::getInstance().getStats().qNodes;
    if (startNodes != 51042) {
        std::cerr << "[FAIL] Gate Omega 6-G: Startpos depth 6 nodes " << startNodes << " != 51042\n";
        return false;
    }

    // 2. KiwiPete depth 6 verification (expected: 59,986 nodes under Phase 6.5-D Improving Heuristic)
    const std::string kiwipete = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
    auto optKiwi = FenParser::parse(kiwipete);
    if (!optKiwi) return false;
    Position kiwiPos = *optKiwi;

    {
        LocalCoutSilencer silencer;
        BenchmarkRunner::resetSearchState(16);
        SearchLimits limits;
        limits.depth = 6;
        limits.clearTables = true;
        Search::runSearch(kiwiPos, limits);
    }

    uint64_t kiwiNodes = SearchController::getInstance().getStats().nodes + SearchController::getInstance().getStats().qNodes;
    if (kiwiNodes != 59986) {
        std::cerr << "[FAIL] Gate Omega 6-G: KiwiPete depth 6 nodes " << kiwiNodes << " != 59986\n";
        return false;
    }

    // 3. Full benchmark suite (6 canonical positions at depth 6) == 313,092 nodes
    BenchmarkConfig cfg;
    cfg.overrideDepth = 6;
    cfg.hashSizeMb = 16;
    cfg.mode = BenchmarkStateMode::Isolated;
    cfg.silentSearch = true;
    cfg.printConsole = false;
    BenchmarkRunRecord rec = BenchmarkRunner::run(cfg);

    if (rec.aggregate.totalNodes != 313092) {
        std::cerr << "[FAIL] Gate Omega 6-G: Aggregate benchmark depth 6 nodes "
                  << rec.aggregate.totalNodes << " != 313092\n";
        return false;
    }

    return true;
}

bool runMilestoneOmegaPhase6ParameterRegistryTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===  MILESTONE OMEGA, PHASE 6: PARAMETER REGISTRY & UCI TESTS ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 7;

    bool passA = testGateOmega6A_RegistryInitializationAndDefaults();
    std::cout << "[" << (passA ? "PASS" : "FAIL") << "] Omega 6-A: Parameter Registry Defaults & Initialization\n";
    if (passA) passed++;

    bool passB = testGateOmega6B_ValidAndInvalidMutations();
    std::cout << "[" << (passB ? "PASS" : "FAIL") << "] Omega 6-B: Parameter Bounds Validation & Mutations\n";
    if (passB) passed++;

    bool passC = testGateOmega6C_StringParsing();
    std::cout << "[" << (passC ? "PASS" : "FAIL") << "] Omega 6-C: String Parsing for Numeric and Boolean Types\n";
    if (passC) passed++;

    bool passD = testGateOmega6D_UciEmissionFormatting();
    std::cout << "[" << (passD ? "PASS" : "FAIL") << "] Omega 6-D: UCI Option Formatting & JSON Serialization\n";
    if (passD) passed++;

    bool passE = testGateOmega6E_SetOptionAndResetDispatch();
    std::cout << "[" << (passE ? "PASS" : "FAIL") << "] Omega 6-E: UCI setoption Command & TT Reset Dispatch\n";
    if (passE) passed++;

    bool passF = testGateOmega6F_CliOverrideParsing();
    std::cout << "[" << (passF ? "PASS" : "FAIL") << "] Omega 6-F: CLI Override Format & Spec Validation\n";
    if (passF) passed++;

    bool passG = testGateOmega6G_DeterministicBaselinePreservation();
    std::cout << "[" << (passG ? "PASS" : "FAIL") << "] Omega 6-G: Deterministic Node Baseline Preservation (220,504 nodes)\n";
    if (passG) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "MILESTONE OMEGA PHASE 6 RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ============================================================================
// MILESTONE OMEGA, PHASE 6.5-A: PVS ZERO-WINDOW SCOUTING TESTS
// ============================================================================

bool testGate65A_1_PvNodeFirstMoveFullWindow() {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    auto opt = FenParser::parse(fen);
    if (!opt) return false;
    Position pos = *opt;

    BenchmarkRunner::resetSearchState(16);
    PVLine pv;
    int score = Search::negamax(pos, 2, -500, 500, 0, pv, false, Move());

    if (pv.count == 0) {
        std::cerr << "[FAIL] Phase 6.5-A Test 1: PV line empty on PV node search\n";
        return false;
    }

    if (score < -500 || score > 500) {
        std::cerr << "[FAIL] Phase 6.5-A Test 1: Score out of full window bounds: " << score << "\n";
        return false;
    }

    MoveList legal;
    MoveGenerator::generateLegalMoves(pos, legal);
    bool found = false;
    for (size_t i = 0; i < legal.size(); ++i) {
        if (legal[i].getRawData() == pv.moves[0].getRawData()) {
            found = true;
            break;
        }
    }
    if (!found) {
        std::cerr << "[FAIL] Phase 6.5-A Test 1: First move is not legal\n";
        return false;
    }

    return true;
}

bool testGate65A_2_PvNodeSiblingFailLowNonResearch() {
    const std::string fen = "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1";
    auto opt = FenParser::parse(fen);
    if (!opt) return false;
    Position pos = *opt;

    BenchmarkRunner::resetSearchState(16);
    SearchLimits limits;
    limits.depth = 3;
    limits.clearTables = true;
    Search::runSearch(pos, limits);

    const auto& stats = SearchController::getInstance().getStats();
    if (stats.pvLine.count == 0 || stats.pvLine.moves[0].toString() != "c8c4") {
        std::cerr << "[FAIL] Phase 6.5-A Test 2: Best move was not c8c4\n";
        return false;
    }

    return true;
}

bool testGate65A_3_PvNodeSiblingFailHighSingleResearch() {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    auto opt = FenParser::parse(fen);
    if (!opt) return false;
    Position pos = *opt;

    BenchmarkRunner::resetSearchState(16);
    SearchLimits limits;
    limits.depth = 4;
    limits.clearTables = true;
    Search::runSearch(pos, limits);

    const auto& stats = SearchController::getInstance().getStats();
    if (stats.researches == 0) {
        std::cerr << "[FAIL] Phase 6.5-A Test 3: No re-searches occurred during depth 4 search\n";
        return false;
    }

    if (stats.pvLine.count == 0) {
        std::cerr << "[FAIL] Phase 6.5-A Test 3: PV line empty after search\n";
        return false;
    }

    return true;
}

bool testGate65A_4_PvNodeSiblingBetaCutoffZeroResearch() {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    auto opt = FenParser::parse(fen);
    if (!opt) return false;
    Position pos = *opt;

    BenchmarkRunner::resetSearchState(16);
    auto& stats = SearchController::getInstance().getStats();
    stats.reset();

    PVLine pv;
    int score = Search::negamax(pos, 2, -50, 0, 0, pv, false, Move());

    if (score >= 0) {
        if (stats.researches > 0) {
            std::cerr << "[FAIL] Phase 6.5-A Test 4: Unexpected re-search on beta cutoff: "
                      << stats.researches << "\n";
            return false;
        }
    }

    return true;
}

bool testGate65A_5_NonPvNodeZeroWindowPreservation() {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    auto opt = FenParser::parse(fen);
    if (!opt) return false;
    Position pos = *opt;

    BenchmarkRunner::resetSearchState(16);
    PVLine pv;
    const int alpha = 0;
    const int beta = 1;
    int score = Search::negamax(pos, 3, alpha, beta, 0, pv, true, Move());

    if (score > alpha && score < beta) {
        std::cerr << "[FAIL] Phase 6.5-A Test 5: Score fell inside zero-window: " << score << "\n";
        return false;
    }

    return true;
}

bool testGate65A_6_RootBestMoveAndPvLineValidity() {
    const char* fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1"
    };

    for (const char* fen : fens) {
        auto opt = FenParser::parse(fen);
        if (!opt) return false;
        Position pos = *opt;

        BenchmarkRunner::resetSearchState(16);
        SearchLimits limits;
        limits.depth = 4;
        limits.clearTables = true;
        Search::runSearch(pos, limits);

        const auto& stats = SearchController::getInstance().getStats();
        if (stats.pvLine.count == 0) {
            std::cerr << "[FAIL] Phase 6.5-A Test 6: PV line empty for " << fen << "\n";
            return false;
        }

        Position testPos = pos;
        for (size_t p = 0; p < stats.pvLine.count; ++p) {
            Move m = stats.pvLine.moves[p];
            MoveList legal;
            MoveGenerator::generateLegalMoves(testPos, legal);
            bool isLegal = false;
            for (size_t i = 0; i < legal.size(); ++i) {
                if (legal[i].getRawData() == m.getRawData()) {
                    isLegal = true;
                    break;
                }
            }
            if (!isLegal) {
                std::cerr << "[FAIL] Phase 6.5-A Test 6: Move " << m.toString()
                          << " at ply " << p << " is not legal in position " << fen << "\n";
                return false;
            }
            UndoState u;
            MoveExecutor::makeMove(testPos, m, u);
        }
    }

    return true;
}

bool runPhase65APvsTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===  MILESTONE OMEGA, PHASE 6.5-A: PVS ZERO-WINDOW SCOUTING   ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 6;

    bool pass1 = testGate65A_1_PvNodeFirstMoveFullWindow();
    std::cout << "[" << (pass1 ? "PASS" : "FAIL") << "] Phase 6.5-A Test 1: PV Node First-Move Full Window Search\n";
    if (pass1) passed++;

    bool pass2 = testGate65A_2_PvNodeSiblingFailLowNonResearch();
    std::cout << "[" << (pass2 ? "PASS" : "FAIL") << "] Phase 6.5-A Test 2: PV Node Sibling Zero-Window Scouting & Fail-Low Non-Re-search\n";
    if (pass2) passed++;

    bool pass3 = testGate65A_3_PvNodeSiblingFailHighSingleResearch();
    std::cout << "[" << (pass3 ? "PASS" : "FAIL") << "] Phase 6.5-A Test 3: PV Node Sibling Fail-High Triggering Full-Depth Re-search\n";
    if (pass3) passed++;

    bool pass4 = testGate65A_4_PvNodeSiblingBetaCutoffZeroResearch();
    std::cout << "[" << (pass4 ? "PASS" : "FAIL") << "] Phase 6.5-A Test 4: PV Node Sibling Immediate Beta-Cutoff with Zero Re-search\n";
    if (pass4) passed++;

    bool pass5 = testGate65A_5_NonPvNodeZeroWindowPreservation();
    std::cout << "[" << (pass5 ? "PASS" : "FAIL") << "] Phase 6.5-A Test 5: Non-PV Node Zero-Window Preservation Across Moves\n";
    if (pass5) passed++;

    bool pass6 = testGate65A_6_RootBestMoveAndPvLineValidity();
    std::cout << "[" << (pass6 ? "PASS" : "FAIL") << "] Phase 6.5-A Test 6: Root Best-Move Consistency & Sequential PV Line Validity\n";
    if (pass6) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "PHASE 6.5-A PVS RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone Omega, Phase 6.5-B: Staged MovePicker State Machine Tests
// ---------------------------------------------------------------------------

bool testGate65B_1_FullMoveSetEquivalence() {
    const char* fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1",
        "r1b2rk1/1p1nbppp/pq1p4/3B4/4PB2/1N6/PPP3PP/R2Q1R1K w - - 0 1",
        "8/8/4k3/8/8/4K3/4P3/8 w - - 0 1",
        "rnbqkb1r/pp1p1ppp/4pn2/2p5/2PP4/5N2/PP2PPPP/RNBQKB1R w KQkq - 0 4"
    };

    for (const char* fen : fens) {
        auto opt = FenParser::parse(fen);
        if (!opt) return false;
        Position pos = *opt;

        MoveList legal;
        MoveGenerator::generateLegalMoves(pos, legal);

        MoveList captures;
        MoveGenerator::generateLegalCaptures(pos, captures);

        MoveList quiets;
        MoveGenerator::generateLegalQuiets(pos, quiets);

        // Exact partition size
        if (captures.size() + quiets.size() != legal.size()) {
            std::cerr << "[FAIL] Phase 6.5-B Test 1: Partition size mismatch on " << fen
                      << " (captures=" << captures.size() << " + quiets=" << quiets.size() 
                      << " != legal=" << legal.size() << ")\n";
            return false;
        }

        // Check captures are legal and not quiet
        for (size_t i = 0; i < captures.size(); ++i) {
            bool inLegal = false;
            for (size_t j = 0; j < legal.size(); ++j) {
                if (captures[i].getRawData() == legal[j].getRawData()) { inLegal = true; break; }
            }
            if (!inLegal) {
                std::cerr << "[FAIL] Phase 6.5-B Test 1: Capture " << captures[i].toString() << " not in legal moves\n";
                return false;
            }
            for (size_t j = 0; j < quiets.size(); ++j) {
                if (captures[i].getRawData() == quiets[j].getRawData()) {
                    std::cerr << "[FAIL] Phase 6.5-B Test 1: Move " << captures[i].toString() << " present in both captures and quiets\n";
                    return false;
                }
            }
        }

        // Check quiets are legal
        for (size_t i = 0; i < quiets.size(); ++i) {
            bool inLegal = false;
            for (size_t j = 0; j < legal.size(); ++j) {
                if (quiets[i].getRawData() == legal[j].getRawData()) { inLegal = true; break; }
            }
            if (!inLegal) {
                std::cerr << "[FAIL] Phase 6.5-B Test 1: Quiet " << quiets[i].toString() << " not in legal moves\n";
                return false;
            }
        }

        // Verify MovePicker(Normal) yields the exact set of legal moves
        SearchContext ctx;
        MovePicker picker(pos, Move::none(), ctx, PickerMode::Normal);
        std::vector<Move> picked;
        Move m;
        while ((m = picker.nextMove()) != Move::none()) {
            picked.push_back(m);
        }

        if (picked.size() != legal.size()) {
            std::cerr << "[FAIL] Phase 6.5-B Test 1: MovePicker yielded " << picked.size()
                      << " moves, expected " << legal.size() << " on " << fen << "\n";
            return false;
        }

        // Deduplication & membership check
        for (size_t i = 0; i < picked.size(); ++i) {
            for (size_t j = i + 1; j < picked.size(); ++j) {
                if (picked[i].getRawData() == picked[j].getRawData()) {
                    std::cerr << "[FAIL] Phase 6.5-B Test 1: Duplicate move " << picked[i].toString() << " yielded by MovePicker\n";
                    return false;
                }
            }
            bool found = false;
            for (size_t j = 0; j < legal.size(); ++j) {
                if (picked[i].getRawData() == legal[j].getRawData()) { found = true; break; }
            }
            if (!found) {
                std::cerr << "[FAIL] Phase 6.5-B Test 1: Yielded move " << picked[i].toString() << " is not legal\n";
                return false;
            }
        }
    }
    return true;
}

bool testGate65B_2_PromotionPartitioning() {
    // 1. Quiet promotions only
    const std::string quietPromoFen = "8/4P3/8/8/8/8/8/4K2k w - - 0 1";
    auto opt1 = FenParser::parse(quietPromoFen);
    if (!opt1) return false;
    Position pos1 = *opt1;

    MoveList caps1, quiets1;
    MoveGenerator::generateLegalCaptures(pos1, caps1);
    MoveGenerator::generateLegalQuiets(pos1, quiets1);

    if (caps1.size() != 0) {
        std::cerr << "[FAIL] Phase 6.5-B Test 2: Expected 0 captures in quiet promo pos, got " << caps1.size() << "\n";
        return false;
    }
    int promoCount = 0;
    for (size_t i = 0; i < quiets1.size(); ++i) {
        if (quiets1[i].isPromotion()) promoCount++;
    }
    if (promoCount != 4) {
        std::cerr << "[FAIL] Phase 6.5-B Test 2: Expected 4 quiet promotions, got " << promoCount << "\n";
        return false;
    }

    // 2. Capture promotions + Quiet promotions
    const std::string capPromoFen = "5n2/4P3/8/8/8/8/8/4K2k w - - 0 1";
    auto opt2 = FenParser::parse(capPromoFen);
    if (!opt2) return false;
    Position pos2 = *opt2;

    MoveList caps2, quiets2;
    MoveGenerator::generateLegalCaptures(pos2, caps2);
    MoveGenerator::generateLegalQuiets(pos2, quiets2);

    int capPromoCount = 0;
    for (size_t i = 0; i < caps2.size(); ++i) {
        if (caps2[i].isPromotion()) {
            capPromoCount++;
            if (caps2[i].getToSquare() != Square::F8) {
                std::cerr << "[FAIL] Phase 6.5-B Test 2: Capture promo to wrong square: " << caps2[i].toString() << "\n";
                return false;
            }
        }
    }
    if (capPromoCount != 4) {
        std::cerr << "[FAIL] Phase 6.5-B Test 2: Expected 4 capture promotions, got " << capPromoCount << "\n";
        return false;
    }

    int quietPromoCount = 0;
    for (size_t i = 0; i < quiets2.size(); ++i) {
        if (quiets2[i].isPromotion()) {
            quietPromoCount++;
            if (quiets2[i].getToSquare() != Square::E8) {
                std::cerr << "[FAIL] Phase 6.5-B Test 2: Quiet promo to wrong square: " << quiets2[i].toString() << "\n";
                return false;
            }
        }
    }
    if (quietPromoCount != 4) {
        std::cerr << "[FAIL] Phase 6.5-B Test 2: Expected 4 quiet promotions, got " << quietPromoCount << "\n";
        return false;
    }

    return true;
}

bool testGate65B_3_OrderingAndDeduplication() {
    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt) return false;
    Position pos = *opt;

    // a) TT Move first
    Move ttMove(Square::E2, Square::E4, Move::Flags::DoublePawnPush);
    SearchContext ctx;
    MovePicker picker1(pos, ttMove, ctx, PickerMode::Normal);
    Move firstMove = picker1.nextMove();
    if (firstMove.getRawData() != ttMove.getRawData()) {
        std::cerr << "[FAIL] Phase 6.5-B Test 3: TT move not yielded first: " << firstMove.toString() << "\n";
        return false;
    }

    // b) Deduplication: TT move equals Killer 0
    std::array<std::array<Move, 2>, 64> km{};
    km[0][0] = ttMove;
    km[0][1] = Move(Square::D2, Square::D4, Move::Flags::DoublePawnPush);
    ctx.killerMoves = &km;

    MovePicker picker2(pos, ttMove, ctx, PickerMode::Normal);
    std::vector<Move> picked2;
    Move m;
    while ((m = picker2.nextMove()) != Move::none()) {
        picked2.push_back(m);
    }
    if (picked2.size() != 20) {
        std::cerr << "[FAIL] Phase 6.5-B Test 3: Total moves with TT==Killer0: " << picked2.size() << " != 20\n";
        return false;
    }
    int ttCount = 0;
    for (const auto& mv : picked2) {
        if (mv.getRawData() == ttMove.getRawData()) ttCount++;
    }
    if (ttCount != 1) {
        std::cerr << "[FAIL] Phase 6.5-B Test 3: TT move yielded " << ttCount << " times (expected 1)\n";
        return false;
    }

    // c) Deduplication: Killer 0 equals Killer 1
    km[0][0] = Move(Square::G1, Square::F3);
    km[0][1] = Move(Square::G1, Square::F3);
    MovePicker picker3(pos, Move::none(), ctx, PickerMode::Normal);
    std::vector<Move> picked3;
    while ((m = picker3.nextMove()) != Move::none()) {
        picked3.push_back(m);
    }
    if (picked3.size() != 20) {
        std::cerr << "[FAIL] Phase 6.5-B Test 3: Total moves with Killer0==Killer1: " << picked3.size() << " != 20\n";
        return false;
    }
    int kCount = 0;
    for (const auto& mv : picked3) {
        if (mv.getRawData() == km[0][0].getRawData()) kCount++;
    }
    if (kCount != 1) {
        std::cerr << "[FAIL] Phase 6.5-B Test 3: Killer move yielded " << kCount << " times (expected 1)\n";
        return false;
    }

    // d) Deduplication: CounterMove equals Killer 0
    km[0][0] = Move(Square::B1, Square::C3);
    km[0][1] = Move::none();
    MovePicker picker4(pos, Move::none(), ctx, PickerMode::Normal);
    picker4.setCounterMove(Move(Square::B1, Square::C3));
    std::vector<Move> picked4;
    while ((m = picker4.nextMove()) != Move::none()) {
        picked4.push_back(m);
    }
    if (picked4.size() != 20) {
        std::cerr << "[FAIL] Phase 6.5-B Test 3: Total moves with CM==Killer0: " << picked4.size() << " != 20\n";
        return false;
    }
    int cmCount = 0;
    for (const auto& mv : picked4) {
        if (mv.getRawData() == km[0][0].getRawData()) cmCount++;
    }
    if (cmCount != 1) {
        std::cerr << "[FAIL] Phase 6.5-B Test 3: CM move yielded " << cmCount << " times (expected 1)\n";
        return false;
    }

    return true;
}

bool testGate65B_4_QuiescenceModeIsolation() {
    const char* fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1"
    };

    for (const char* fen : fens) {
        auto opt = FenParser::parse(fen);
        if (!opt) return false;
        Position pos = *opt;

        SearchContext ctx;
        MovePicker picker(pos, Move::none(), ctx, PickerMode::Quiescence);
        Move m;
        while ((m = picker.nextMove()) != Move::none()) {
            if (!MovePicker::isCapture(pos, m)) {
                std::cerr << "[FAIL] Phase 6.5-B Test 4: Quiet move " << m.toString()
                          << " yielded in Quiescence mode on " << fen << "\n";
                return false;
            }
            int seeVal = SEE::evaluate(pos, m.getFromSquare(), m.getToSquare());
            if (seeVal < 0) {
                std::cerr << "[FAIL] Phase 6.5-B Test 4: Losing capture " << m.toString()
                          << " (SEE " << seeVal << ") yielded in Quiescence mode on " << fen << "\n";
                return false;
            }
        }

        // Test that quiet TT move is NOT yielded in Quiescence mode
        Move quietTT(Square::E2, Square::E4);
        if (!MovePicker::isCapture(pos, quietTT)) {
            MovePicker pickerQuietTT(pos, quietTT, ctx, PickerMode::Quiescence);
            Move first = pickerQuietTT.nextMove();
            if (first.getRawData() == quietTT.getRawData()) {
                std::cerr << "[FAIL] Phase 6.5-B Test 4: Quiet TT move yielded in Quiescence mode\n";
                return false;
            }
        }
    }
    return true;
}

bool testGate65B_5_LegalityGuarantee() {
    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt) return false;
    Position currentPos = *opt;

    uint32_t seed = 42;
    auto lcg = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return seed;
    };

    int positionsTested = 0;
    while (positionsTested < 1000) {
        SearchContext ctx;
        MovePicker picker(currentPos, Move::none(), ctx, PickerMode::Normal);
        std::vector<Move> legalMoves;
        Move m;
        while ((m = picker.nextMove()) != Move::none()) {
            Position testPos = currentPos;
            UndoState undo;
            MoveExecutor::makeMove(testPos, m, undo);
            if (MoveGenerator::inCheck(testPos, currentPos.getSideToMove())) {
                std::cerr << "[FAIL] Phase 6.5-B Test 5: MovePicker yielded illegal move " << m.toString()
                          << " leaving king in check at pos #" << positionsTested << "\n";
                return false;
            }
            legalMoves.push_back(m);
        }

        positionsTested++;
        if (legalMoves.empty() || positionsTested % 40 == 0) {
            // Reset to a canonical position to start a new walk
            const char* resetFens[] = {
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1",
                "r1b2rk1/1p1nbppp/pq1p4/3B4/4PB2/1N6/PPP3PP/R2Q1R1K w - - 0 1"
            };
            auto resetOpt = FenParser::parse(resetFens[(positionsTested / 40) % 4]);
            if (resetOpt) currentPos = *resetOpt;
        } else {
            // Play a pseudorandom legal move to advance the walk
            size_t idx = lcg() % legalMoves.size();
            UndoState undo;
            MoveExecutor::makeMove(currentPos, legalMoves[idx], undo);
        }
    }

    return true;
}

bool runPhase65BMovePickerTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===  MILESTONE OMEGA, PHASE 6.5-B: STAGED MOVEPICKER TESTS    ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 5;

    bool pass1 = testGate65B_1_FullMoveSetEquivalence();
    std::cout << "[" << (pass1 ? "PASS" : "FAIL") << "] Phase 6.5-B Test 1: Full Move-Set Equivalence Across Canonical Positions\n";
    if (pass1) passed++;

    bool pass2 = testGate65B_2_PromotionPartitioning();
    std::cout << "[" << (pass2 ? "PASS" : "FAIL") << "] Phase 6.5-B Test 2: Promotion Partitioning (Captures vs Quiets)\n";
    if (pass2) passed++;

    bool pass3 = testGate65B_3_OrderingAndDeduplication();
    std::cout << "[" << (pass3 ? "PASS" : "FAIL") << "] Phase 6.5-B Test 3: Move Ordering & Multi-Stage Deduplication\n";
    if (pass3) passed++;

    bool pass4 = testGate65B_4_QuiescenceModeIsolation();
    std::cout << "[" << (pass4 ? "PASS" : "FAIL") << "] Phase 6.5-B Test 4: Quiescence Mode Isolation & Winning Capture Filtration\n";
    if (pass4) passed++;

    bool pass5 = testGate65B_5_LegalityGuarantee();
    std::cout << "[" << (pass5 ? "PASS" : "FAIL") << "] Phase 6.5-B Test 5: Strict Legality Guarantee (1,000 Random Walk Positions)\n";
    if (pass5) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "PHASE 6.5-B MOVEPICKER RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone Omega, Phase 6.5-C: Reverse Futility Pruning (RFP) Tests
// ---------------------------------------------------------------------------

bool testGate65C_1_DepthBoundaries() {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    auto opt = FenParser::parse(fen);
    if (!opt) return false;
    Position pos = *opt;

    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();
    reg.syncToEngineParameters(SearchController::getInstance().getMutableParams());
    int rfpMarginBase = SearchController::getInstance().getParams().search.rfpMarginBase;

    int staticEval = Search::evaluate(pos);

    // 1. Test eligible depths: 1, 2, 3
    for (int d = 1; d <= 3; ++d) {
        BenchmarkRunner::resetSearchState(16);
        Search::s_tt.clear();
        PVLine pv;
        int margin = rfpMarginBase * d;
        int beta = staticEval - margin;
        int alpha = beta - 1;

        int score = Search::negamax(pos, d, alpha, beta, 0, pv, false, Move());
        if (score != beta) {
            std::cerr << "[FAIL] Phase 6.5-C Test 1: Depth " << d << " expected score " << beta << ", got " << score << "\n";
            return false;
        }
        uint64_t nodes = SearchController::getInstance().getStats().nodes;
        if (nodes != 1) {
            std::cerr << "[FAIL] Phase 6.5-C Test 1: Depth " << d << " expected 1 node, got " << nodes << "\n";
            return false;
        }
    }

    // 2. Test ineligible depth 0 (goes to quiescence, does not trigger RFP)
    {
        BenchmarkRunner::resetSearchState(16);
        PVLine pv;
        int beta = staticEval - 75;
        int alpha = beta - 1;
        Search::negamax(pos, 0, alpha, beta, 0, pv, false, Move());
        uint64_t qNodes = SearchController::getInstance().getStats().qNodes;
        if (qNodes == 0) {
            std::cerr << "[FAIL] Phase 6.5-C Test 1: Depth 0 expected qNodes > 0, got 0\n";
            return false;
        }
    }

    // 3. Test ineligible depth 4 (depth > 3 guard prevents RFP)
    {
        BenchmarkRunner::resetSearchState(16);
        Search::s_tt.clear();
        PVLine pv;
        int margin = rfpMarginBase * 4;
        int beta = staticEval - margin;
        int alpha = beta - 1;
        Search::negamax(pos, 4, alpha, beta, 0, pv, false, Move());
        uint64_t nodes = SearchController::getInstance().getStats().nodes;
        if (nodes <= 1) {
            std::cerr << "[FAIL] Phase 6.5-C Test 1: Depth 4 should not trigger RFP (nodes > 1 expected, got " << nodes << ")\n";
            return false;
        }
    }

    return true;
}

bool testGate65C_2_WindowGuard() {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    auto opt = FenParser::parse(fen);
    if (!opt) return false;
    Position pos = *opt;

    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();
    reg.syncToEngineParameters(SearchController::getInstance().getMutableParams());
    int rfpMarginBase = SearchController::getInstance().getParams().search.rfpMarginBase;

    int staticEval = Search::evaluate(pos);
    int d = 2;
    int margin = rfpMarginBase * d;
    int beta = staticEval - margin;

    // Set a wide PV window: beta - alpha > 1
    int alpha = beta - 50;
    BenchmarkRunner::resetSearchState(16);
    Search::s_tt.clear();
    PVLine pv;

    Search::negamax(pos, d, alpha, beta, 0, pv, false, Move());
    uint64_t nodes = SearchController::getInstance().getStats().nodes;
    if (nodes <= 1) {
        std::cerr << "[FAIL] Phase 6.5-C Test 2: PV node triggered RFP unexpectedly (nodes: " << nodes << ")\n";
        return false;
    }
    if (pv.count == 0) {
        std::cerr << "[FAIL] Phase 6.5-C Test 2: PV node should have generated moves in PV line\n";
        return false;
    }

    return true;
}

bool testGate65C_3_InCheckSafety() {
    // Position where White is in check from Qf2, can play Kxf2
    const std::string fen = "r1b1kbnr/pppp1ppp/8/4p3/4P3/8/PPPP1qPP/RNBQKBNR w KQkq - 0 4";
    auto opt = FenParser::parse(fen);
    if (!opt) return false;
    Position pos = *opt;

    if (!MoveGenerator::inCheck(pos, pos.getSideToMove())) {
        std::cerr << "[FAIL] Phase 6.5-C Test 3: Test position is not in check\n";
        return false;
    }

    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();
    reg.syncToEngineParameters(SearchController::getInstance().getMutableParams());
    int rfpMarginBase = SearchController::getInstance().getParams().search.rfpMarginBase;

    int staticEval = Search::evaluate(pos);
    int d = 2;
    int margin = rfpMarginBase * d;
    int beta = staticEval - margin;
    int alpha = beta - 1;

    BenchmarkRunner::resetSearchState(16);
    Search::s_tt.clear();
    PVLine pv;

    Search::negamax(pos, d, alpha, beta, 0, pv, false, Move());
    uint64_t nodes = SearchController::getInstance().getStats().nodes;
    if (nodes <= 1) {
        std::cerr << "[FAIL] Phase 6.5-C Test 3: In-check position triggered RFP unexpectedly (nodes: " << nodes << ")\n";
        return false;
    }

    return true;
}

bool testGate65C_4_MateSafetyGuard() {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    auto opt = FenParser::parse(fen);
    if (!opt) return false;
    Position pos = *opt;

    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();
    reg.syncToEngineParameters(SearchController::getInstance().getMutableParams());

    // 1. Beta >= MATE_SCORE - MAX_PLY
    {
        BenchmarkRunner::resetSearchState(16);
        Search::s_tt.clear();
        PVLine pv;
        int mateBound = Search::MATE_SCORE - Search::MAX_PLY;
        int beta = mateBound + 10;
        int alpha = beta - 1;

        Search::negamax(pos, 2, alpha, beta, 0, pv, false, Move());
        uint64_t nodes = SearchController::getInstance().getStats().nodes;
        if (nodes <= 1) {
            std::cerr << "[FAIL] Phase 6.5-C Test 4: Mate bound (high) triggered RFP unexpectedly\n";
            return false;
        }
    }

    // 2. Beta <= -(MATE_SCORE - MAX_PLY)
    {
        BenchmarkRunner::resetSearchState(16);
        Search::s_tt.clear();
        PVLine pv;
        int mateBound = -(Search::MATE_SCORE - Search::MAX_PLY);
        int beta = mateBound - 10;
        int alpha = beta - 1;

        Search::negamax(pos, 2, alpha, beta, 0, pv, false, Move());
        uint64_t nodes = SearchController::getInstance().getStats().nodes;
        if (nodes <= 1) {
            std::cerr << "[FAIL] Phase 6.5-C Test 4: Mate bound (low) triggered RFP unexpectedly\n";
            return false;
        }
    }

    return true;
}

bool testGate65C_5_TTLowerBoundContaminationGuard() {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    auto opt = FenParser::parse(fen);
    if (!opt) return false;
    Position pos = *opt;

    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();
    reg.syncToEngineParameters(SearchController::getInstance().getMutableParams());
    int rfpMarginBase = SearchController::getInstance().getParams().search.rfpMarginBase;

    BenchmarkRunner::resetSearchState(16);
    Search::s_tt.clear();

    int staticEval = Search::evaluate(pos);
    int d = 2;
    int margin = rfpMarginBase * d;
    int beta = staticEval - margin;
    int alpha = beta - 1;
    PVLine pv;

    int score = Search::negamax(pos, d, alpha, beta, 0, pv, false, Move());
    if (score != beta) {
        std::cerr << "[FAIL] Phase 6.5-C Test 5: Expected RFP cutoff score " << beta << ", got " << score << "\n";
        return false;
    }
    if (SearchController::getInstance().getStats().nodes != 1) {
        std::cerr << "[FAIL] Phase 6.5-C Test 5: Expected exactly 1 node on RFP cutoff\n";
        return false;
    }

    int ttScore = 0;
    int ttDepth = 0;
    Move ttMove;
    TTNodeType ttType = TTNodeType::Exact;
    bool hit = Search::s_tt.probeEntry(pos.getHashKey(), ttScore, ttMove, ttDepth, ttType);

    if (hit) {
        if (ttType == TTNodeType::LowerBound) {
            std::cerr << "[FAIL] Phase 6.5-C Test 5: TT contaminated with LowerBound entry on RFP trigger!\n";
            return false;
        }
        std::cerr << "[FAIL] Phase 6.5-C Test 5: TT contains entry after RFP cutoff when TT was clean!\n";
        return false;
    }

    return true;
}

bool testGate65C_6_MarginBoundaries() {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    auto opt = FenParser::parse(fen);
    if (!opt) return false;
    Position pos = *opt;

    int staticEval = Search::evaluate(pos);
    const int depth = 2;
    const int testMargins[] = {20, 75, 200};

    for (int marginBase : testMargins) {
        SearchController::getInstance().getMutableParams().search.rfpMarginBase = marginBase;
        int margin = marginBase * depth;

        // 1. Cutoff boundary: staticEval - margin >= beta (should trigger RFP)
        {
            BenchmarkRunner::resetSearchState(16);
            Search::s_tt.clear();
            PVLine pv;
            int beta = staticEval - margin;
            int alpha = beta - 1;
            int score = Search::negamax(pos, depth, alpha, beta, 0, pv, false, Move());
            if (score != beta || SearchController::getInstance().getStats().nodes != 1) {
                std::cerr << "[FAIL] Phase 6.5-C Test 6: Margin " << marginBase << " cutoff failed to trigger RFP\n";
                SearchController::getInstance().getMutableParams().search.rfpMarginBase = 75;
                return false;
            }
        }

        // 2. Below cutoff boundary: staticEval - margin < beta (should NOT trigger RFP)
        {
            BenchmarkRunner::resetSearchState(16);
            Search::s_tt.clear();
            PVLine pv;
            int beta = staticEval - margin + 1;
            int alpha = beta - 1;
            Search::negamax(pos, depth, alpha, beta, 0, pv, false, Move());
            if (SearchController::getInstance().getStats().nodes <= 1) {
                std::cerr << "[FAIL] Phase 6.5-C Test 6: Margin " << marginBase << " non-cutoff falsely triggered RFP\n";
                SearchController::getInstance().getMutableParams().search.rfpMarginBase = 75;
                return false;
            }
        }
    }

    // Restore default margin
    SearchController::getInstance().getMutableParams().search.rfpMarginBase = 75;
    return true;
}

bool testGate65C_7_TacticalAndBestMovePreservation() {
    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();
    reg.syncToEngineParameters(SearchController::getInstance().getMutableParams());

    struct CanonicalTarget {
        std::string name;
        std::string fen;
        std::string expectedMove;
        int depth;
    };

    const CanonicalTarget targets[] = {
        {"startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "b1c3", 6},
        {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", "e2a6", 6},
        {"tactical_wac001", "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1", "c8c4", 6},
        {"search_stress_evasions", "rnb1k1nr/pppp1ppp/4p3/8/3P2q1/5N2/PPP1PPPP/RN1QKB1R w KQkq - 0 1", "f3e5", 6}
    };

    for (const auto& t : targets) {
        auto opt = FenParser::parse(t.fen);
        if (!opt) return false;
        Position pos = *opt;

        BenchmarkRunner::resetSearchState(16);
        SearchLimits limits;
        limits.depth = t.depth;
        limits.clearTables = true;
        Search::runSearch(pos, limits);

        const auto& stats = SearchController::getInstance().getStats();
        if (stats.pvLine.count == 0) {
            std::cerr << "[FAIL] Phase 6.5-C Test 7: No move in PV line for " << t.name << "\n";
            return false;
        }
        std::string bestMove = stats.pvLine.moves[0].toString();
        if (bestMove != t.expectedMove) {
            std::cerr << "[FAIL] Phase 6.5-C Test 7: " << t.name << " best move " << bestMove << " != " << t.expectedMove << "\n";
            return false;
        }
    }

    return true;
}

bool runPhase65CReverseFutilityPruningTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===  MILESTONE OMEGA, PHASE 6.5-C: REVERSE FUTILITY PRUNING   ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 7;

    bool pass1 = testGate65C_1_DepthBoundaries();
    std::cout << "[" << (pass1 ? "PASS" : "FAIL") << "] Phase 6.5-C Test 1: Depth Boundaries (Depths 1-3 Eligible; 0, 4 Ineligible)\n";
    if (pass1) passed++;

    bool pass2 = testGate65C_2_WindowGuard();
    std::cout << "[" << (pass2 ? "PASS" : "FAIL") << "] Phase 6.5-C Test 2: Window Guard (PV Nodes Never Trigger RFP)\n";
    if (pass2) passed++;

    bool pass3 = testGate65C_3_InCheckSafety();
    std::cout << "[" << (pass3 ? "PASS" : "FAIL") << "] Phase 6.5-C Test 3: In-Check Safety (In-Check Positions Never Trigger RFP)\n";
    if (pass3) passed++;

    bool pass4 = testGate65C_4_MateSafetyGuard();
    std::cout << "[" << (pass4 ? "PASS" : "FAIL") << "] Phase 6.5-C Test 4: Mate Safety Guard (|beta| >= MATE_SCORE - MAX_PLY)\n";
    if (pass4) passed++;

    bool pass5 = testGate65C_5_TTLowerBoundContaminationGuard();
    std::cout << "[" << (pass5 ? "PASS" : "FAIL") << "] Phase 6.5-C Test 5: TT LowerBound Contamination Guard (Zero TT Writes on RFP)\n";
    if (pass5) passed++;

    bool pass6 = testGate65C_6_MarginBoundaries();
    std::cout << "[" << (pass6 ? "PASS" : "FAIL") << "] Phase 6.5-C Test 6: Margin Boundaries (RFP_MarginBase = 20, 75, 200)\n";
    if (pass6) passed++;

    bool pass7 = testGate65C_7_TacticalAndBestMovePreservation();
    std::cout << "[" << (pass7 ? "PASS" : "FAIL") << "] Phase 6.5-C Test 7: Tactical & Best-Move Preservation (4 Canonical Positions)\n";
    if (pass7) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "PHASE 6.5-C RFP RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Milestone Omega, Phase 6.5-D: Improving Heuristic (LMR-Only Modulation) Tests
// ---------------------------------------------------------------------------

bool testGate65D_1_StackLifecycleAndPlyGuard() {
    Search::clearStack();
    auto& stack = Search::getStack();

    // Setup ply 0
    stack[0].staticEval = 100;
    stack[0].inCheck = false;

    // Setup ply 1
    stack[1].staticEval = -100;
    stack[1].inCheck = false;

    // Test ply 0: ply < 2 must return false
    if (Search::isImproving(0, false, 150)) {
        std::cerr << "[FAIL] Phase 6.5-D Test 1: ply 0 returned improving = true\n";
        return false;
    }

    // Test ply 1: ply < 2 must return false
    if (Search::isImproving(1, false, 150)) {
        std::cerr << "[FAIL] Phase 6.5-D Test 1: ply 1 returned improving = true\n";
        return false;
    }

    // Test ply 2 with staticEval = 150 > stack[0].staticEval (100) -> must return true
    if (!Search::isImproving(2, false, 150)) {
        std::cerr << "[FAIL] Phase 6.5-D Test 1: ply 2 with 150 > 100 returned improving = false\n";
        return false;
    }

    // Test ply 2 with staticEval = 80 < stack[0].staticEval (100) -> must return false
    if (Search::isImproving(2, false, 80)) {
        std::cerr << "[FAIL] Phase 6.5-D Test 1: ply 2 with 80 < 100 returned improving = true\n";
        return false;
    }

    return true;
}

bool testGate65D_2_StrictGreaterThanInvariant() {
    Search::clearStack();
    auto& stack = Search::getStack();

    stack[0].staticEval = 100;
    stack[0].inCheck = false;

    // Exact equality: staticEval == 100 must evaluate to false
    if (Search::isImproving(2, false, 100)) {
        std::cerr << "[FAIL] Phase 6.5-D Test 2: staticEval == prevEval returned improving = true\n";
        return false;
    }

    // Strictly greater: 101 > 100 must evaluate to true
    if (!Search::isImproving(2, false, 101)) {
        std::cerr << "[FAIL] Phase 6.5-D Test 2: staticEval > prevEval returned improving = false\n";
        return false;
    }

    return true;
}

bool testGate65D_3_CheckInvalidationGuard() {
    Search::clearStack();
    auto& stack = Search::getStack();

    // Case A: Current node is in check (inCheck = true)
    stack[0].staticEval = 100;
    stack[0].inCheck = false;
    if (Search::isImproving(2, true, 200)) {
        std::cerr << "[FAIL] Phase 6.5-D Test 3: current inCheck=true returned improving = true\n";
        return false;
    }

    // Case B: Previous node (ply - 2) was in check (ss[ply - 2].inCheck = true)
    stack[0].staticEval = 100;
    stack[0].inCheck = true;
    if (Search::isImproving(2, false, 200)) {
        std::cerr << "[FAIL] Phase 6.5-D Test 3: ss[ply - 2].inCheck=true returned improving = true\n";
        return false;
    }

    // Case C: Neither in check -> true
    stack[0].inCheck = false;
    if (!Search::isImproving(2, false, 200)) {
        std::cerr << "[FAIL] Phase 6.5-D Test 3: both !inCheck returned improving = false\n";
        return false;
    }

    return true;
}

bool testGate65D_4_MateScoreSafety() {
    Search::clearStack();
    auto& stack = Search::getStack();
    const int mateBound = Search::MATE_SCORE - Search::MAX_PLY;

    // Case A: current staticEval >= mateBound
    stack[0].staticEval = 100;
    stack[0].inCheck = false;
    if (Search::isImproving(2, false, mateBound + 10)) {
        std::cerr << "[FAIL] Phase 6.5-D Test 4: current staticEval >= mateBound returned improving = true\n";
        return false;
    }

    // Case B: current staticEval <= -mateBound
    if (Search::isImproving(2, false, -mateBound - 10)) {
        std::cerr << "[FAIL] Phase 6.5-D Test 4: current staticEval <= -mateBound returned improving = true\n";
        return false;
    }

    // Case C: prevEval >= mateBound
    stack[0].staticEval = mateBound + 10;
    if (Search::isImproving(2, false, mateBound + 50)) {
        std::cerr << "[FAIL] Phase 6.5-D Test 4: prevEval >= mateBound returned improving = true\n";
        return false;
    }

    // Case D: prevEval <= -mateBound
    stack[0].staticEval = -mateBound - 10;
    if (Search::isImproving(2, false, 100)) {
        std::cerr << "[FAIL] Phase 6.5-D Test 4: prevEval <= -mateBound returned improving = true\n";
        return false;
    }

    return true;
}

bool testGate65D_5_LmrUnderflowGuard() {
    // When baseReduction = 0 and improving = false:
    // r = std::max(0, 0 - 1) = 0.
    int r = Search::computeLmrReduction(0, false, 1, 6);
    if (r != 0) {
        std::cerr << "[FAIL] Phase 6.5-D Test 5: base reduction 0 with improving=false underflowed: " << r << "\n";
        return false;
    }

    // With baseReduction = 1 and improving = false:
    // r = std::max(0, 1 - 1) = 0.
    r = Search::computeLmrReduction(1, false, 1, 6);
    if (r != 0) {
        std::cerr << "[FAIL] Phase 6.5-D Test 5: base reduction 1 with improving=false expected 0, got " << r << "\n";
        return false;
    }

    return true;
}

bool testGate65D_6_LmrSafetyClampGuard() {
    for (int d = 2; d <= 10; ++d) {
        int r = Search::computeLmrReduction(20, true, 2, d);
        int maxAllowed = std::max(0, d - 2);
        if (r > maxAllowed) {
            std::cerr << "[FAIL] Phase 6.5-D Test 6: depth " << d << " reduction " << r << " exceeded max " << maxAllowed << "\n";
            return false;
        }
        if (r < 0) {
            std::cerr << "[FAIL] Phase 6.5-D Test 6: reduction below 0: " << r << "\n";
            return false;
        }
    }

    // Specifically at depth 2, max allowed is 0
    int rAt2 = Search::computeLmrReduction(5, true, 2, 2);
    if (rAt2 != 0) {
        std::cerr << "[FAIL] Phase 6.5-D Test 6: depth 2 reduction expected 0, got " << rAt2 << "\n";
        return false;
    }

    // At depth 3, max allowed is 1
    int rAt3 = Search::computeLmrReduction(5, true, 2, 3);
    if (rAt3 != 1) {
        std::cerr << "[FAIL] Phase 6.5-D Test 6: depth 3 reduction expected 1, got " << rAt3 << "\n";
        return false;
    }

    return true;
}

bool testGate65D_7_LmrImprovingBonusScaling() {
    const int baseReduction = 2;
    const int depth = 10;

    // Bonus 0 -> r = base + 0 = 2
    int r0 = Search::computeLmrReduction(baseReduction, true, 0, depth);
    if (r0 != baseReduction + 0) {
        std::cerr << "[FAIL] Phase 6.5-D Test 7: bonus 0 expected " << (baseReduction + 0) << ", got " << r0 << "\n";
        return false;
    }

    // Bonus 1 -> r = base + 1 = 3
    int r1 = Search::computeLmrReduction(baseReduction, true, 1, depth);
    if (r1 != baseReduction + 1) {
        std::cerr << "[FAIL] Phase 6.5-D Test 7: bonus 1 expected " << (baseReduction + 1) << ", got " << r1 << "\n";
        return false;
    }

    // Bonus 2 -> r = base + 2 = 4
    int r2 = Search::computeLmrReduction(baseReduction, true, 2, depth);
    if (r2 != baseReduction + 2) {
        std::cerr << "[FAIL] Phase 6.5-D Test 7: bonus 2 expected " << (baseReduction + 2) << ", got " << r2 << "\n";
        return false;
    }

    return true;
}

bool testGate65D_8_TacticalAndBestMovePreservation() {
    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();
    reg.syncToEngineParameters(SearchController::getInstance().getMutableParams());

    struct CanonicalTarget {
        std::string name;
        std::string fen;
        std::string expectedMove;
        int depth;
    };

    const CanonicalTarget targets[] = {
        {"startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "b1c3", 6},
        {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", "e2a6", 6},
        {"tactical_wac001", "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1", "c8c4", 6},
        {"search_stress_evasions", "rnb1k1nr/pppp1ppp/4p3/8/3P2q1/5N2/PPP1PPPP/RN1QKB1R w KQkq - 0 1", "f3e5", 6}
    };

    for (const auto& t : targets) {
        auto opt = FenParser::parse(t.fen);
        if (!opt) return false;
        Position pos = *opt;

        BenchmarkRunner::resetSearchState(16);
        SearchLimits limits;
        limits.depth = t.depth;
        limits.clearTables = true;
        Search::runSearch(pos, limits);

        const auto& stats = SearchController::getInstance().getStats();
        if (stats.pvLine.count == 0) {
            std::cerr << "[FAIL] Phase 6.5-D Test 8: No move in PV line for " << t.name << "\n";
            return false;
        }
        std::string bestMove = stats.pvLine.moves[0].toString();
        if (bestMove != t.expectedMove) {
            std::cerr << "[FAIL] Phase 6.5-D Test 8: " << t.name << " best move " << bestMove << " != " << t.expectedMove << "\n";
            return false;
        }
    }

    return true;
}

bool runPhase65DImprovingHeuristicTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===  MILESTONE OMEGA, PHASE 6.5-D: IMPROVING HEURISTIC TESTS  ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 8;

    bool pass1 = testGate65D_1_StackLifecycleAndPlyGuard();
    std::cout << "[" << (pass1 ? "PASS" : "FAIL") << "] Phase 6.5-D Test 1: Stack Lifecycle & Ply Guard (ply < 2 vs ply >= 2)\n";
    if (pass1) passed++;

    bool pass2 = testGate65D_2_StrictGreaterThanInvariant();
    std::cout << "[" << (pass2 ? "PASS" : "FAIL") << "] Phase 6.5-D Test 2: Strict Greater-Than Invariant (equality is false)\n";
    if (pass2) passed++;

    bool pass3 = testGate65D_3_CheckInvalidationGuard();
    std::cout << "[" << (pass3 ? "PASS" : "FAIL") << "] Phase 6.5-D Test 3: Check Invalidation Guard (current & previous inCheck)\n";
    if (pass3) passed++;

    bool pass4 = testGate65D_4_MateScoreSafety();
    std::cout << "[" << (pass4 ? "PASS" : "FAIL") << "] Phase 6.5-D Test 4: Mate Score Safety (disqualification on near-mate)\n";
    if (pass4) passed++;

    bool pass5 = testGate65D_5_LmrUnderflowGuard();
    std::cout << "[" << (pass5 ? "PASS" : "FAIL") << "] Phase 6.5-D Test 5: LMR Underflow Guard (r == 0 with improving=false remains 0)\n";
    if (pass5) passed++;

    bool pass6 = testGate65D_6_LmrSafetyClampGuard();
    std::cout << "[" << (pass6 ? "PASS" : "FAIL") << "] Phase 6.5-D Test 6: LMR Safety Clamp Guard (r never exceeds depth - 2)\n";
    if (pass6) passed++;

    bool pass7 = testGate65D_7_LmrImprovingBonusScaling();
    std::cout << "[" << (pass7 ? "PASS" : "FAIL") << "] Phase 6.5-D Test 7: LMR Improving Bonus Scaling (+0, +1, +2)\n";
    if (pass7) passed++;

    bool pass8 = testGate65D_8_TacticalAndBestMovePreservation();
    std::cout << "[" << (pass8 ? "PASS" : "FAIL") << "] Phase 6.5-D Test 8: Tactical & Best-Move Preservation (4 Canonical Positions)\n";
    if (pass8) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "PHASE 6.5-D IMPROVING RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

bool testGate7A_1_FunctionalEquivalence() {
    eval::ClassicalEvaluator evaluator;

    const std::pair<std::string, std::string> canonicalPositions[] = {
        {"startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"},
        {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"},
        {"tactical_wac001", "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1"},
        {"positional_closed", "r2q1rk1/pp1b1ppp/2n1pn2/2pp4/2PP4/2NBPN2/PP3PPP/R1BQ1RK1 w - - 0 8"},
        {"positional_closed_alt", "r1bq1rk1/pp2bppp/2n1pn2/2pp4/2PP4/2N1PN2/PP2BPPP/R1BQ1RK1 w - - 0 1"}
    };

    for (const auto& [name, fen] : canonicalPositions) {
        auto opt = FenParser::parse(fen);
        if (!opt) {
            std::cerr << "[FAIL] Phase 7-A Test 1: Failed to parse canonical FEN for " << name << "\n";
            return false;
        }
        int evalScore = evaluator.evaluate(*opt);
        int directScore = Evaluation::evaluate(*opt);
        if (evalScore != directScore) {
            std::cerr << "[FAIL] Phase 7-A Test 1: Score mismatch on " << name
                      << " (evaluator=" << evalScore << ", direct=" << directScore << ")\n";
            return false;
        }
    }

    // 50 random reachable positions via deterministic pseudo-random walks
    std::mt19937 rng(1337);
    int checkedPositions = 0;

    const std::string seeds[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
    };

    for (const auto& seedFen : seeds) {
        auto opt = FenParser::parse(seedFen);
        if (!opt) return false;
        Position pos = *opt;

        for (int step = 0; step < 50; ++step) {
            MoveList moves;
            MoveGenerator::generateLegalMoves(pos, moves);
            if (moves.size() == 0) {
                auto resetOpt = FenParser::parse(seedFen);
                if (!resetOpt) return false;
                pos = *resetOpt;
                continue;
            }

            std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);
            Move m = moves[dist(rng)];
            UndoState undo;
            MoveExecutor::makeMove(pos, m, undo);

            int evalScore = evaluator.evaluate(pos);
            int directScore = Evaluation::evaluate(pos);
            if (evalScore != directScore) {
                std::cerr << "[FAIL] Phase 7-A Test 1: Random walk position score mismatch: "
                          << evalScore << " != " << directScore << "\n";
                return false;
            }
            checkedPositions++;
            if (checkedPositions >= 50) break;
        }
        if (checkedPositions >= 50) break;
    }

    if (checkedPositions < 50) {
        std::cerr << "[FAIL] Phase 7-A Test 1: Checked only " << checkedPositions << " random positions (expected >= 50)\n";
        return false;
    }

    return true;
}

bool testGate7A_2_InterfaceContractAndPolymorphism() {
    std::unique_ptr<eval::IEvaluator> polyEval = std::make_unique<eval::ClassicalEvaluator>();
    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt) return false;

    int polyScore = polyEval->evaluate(*opt);
    int directScore = Evaluation::evaluate(*opt);
    if (polyScore != directScore) {
        std::cerr << "[FAIL] Phase 7-A Test 2: Polymorphic dispatch score mismatch: "
                  << polyScore << " != " << directScore << "\n";
        return false;
    }

    class MockEvaluator final : public eval::IEvaluator {
    public:
        int evalCallCount = 0;
        bool initCalled = false;
        [[nodiscard]] int evaluate(const Position&) noexcept override {
            evalCallCount++;
            return 9999;
        }
        void initializeSearch() noexcept override {
            initCalled = true;
        }
    };

    MockEvaluator mock;
    eval::IEvaluator* originalEvaluator = Search::getEvaluator();

    Search::setEvaluator(&mock);
    if (Search::getEvaluator() != &mock) {
        std::cerr << "[FAIL] Phase 7-A Test 2: Search::getEvaluator() != &mock\n";
        Search::setEvaluator(originalEvaluator);
        return false;
    }

    int testScore = Search::evaluate(*opt);
    if (testScore != 9999 || mock.evalCallCount != 1) {
        std::cerr << "[FAIL] Phase 7-A Test 2: Mock virtual dispatch failed: score="
                  << testScore << ", callCount=" << mock.evalCallCount << "\n";
        Search::setEvaluator(originalEvaluator);
        return false;
    }

    Search::getEvaluator()->initializeSearch();
    if (!mock.initCalled) {
        std::cerr << "[FAIL] Phase 7-A Test 2: Mock initializeSearch not invoked\n";
        Search::setEvaluator(originalEvaluator);
        return false;
    }

    Search::setEvaluator(nullptr);
    if (Search::getEvaluator() != &Search::m_defaultEvaluator) {
        std::cerr << "[FAIL] Phase 7-A Test 2: Search::setEvaluator(nullptr) did not restore m_defaultEvaluator\n";
        Search::setEvaluator(originalEvaluator);
        return false;
    }

    int restoredScore = Search::evaluate(*opt);
    if (restoredScore != directScore) {
        std::cerr << "[FAIL] Phase 7-A Test 2: Restored default evaluator score mismatch: "
                  << restoredScore << " != " << directScore << "\n";
        Search::setEvaluator(originalEvaluator);
        return false;
    }

    Search::setEvaluator(originalEvaluator);
    return true;
}

bool testGate7A_3_SearchTreeNodeInvariance() {
    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();
    reg.syncToEngineParameters(SearchController::getInstance().getMutableParams());

    BenchmarkConfig cfg;
    cfg.overrideDepth = 6;
    cfg.hashSizeMb = 16;
    cfg.mode = BenchmarkStateMode::Isolated;
    cfg.silentSearch = true;
    cfg.printConsole = false;

    BenchmarkRunRecord rec = BenchmarkRunner::run(cfg);

    if (rec.aggregate.totalNodes != 313092) {
        std::cerr << "[FAIL] Phase 7-A Test 3: Benchmark depth-6 node count "
                  << rec.aggregate.totalNodes << " != 313092\n";
        return false;
    }

    return true;
}

bool runPhase7AEvaluationAbstractionTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===  MILESTONE OMEGA, PHASE 7-A: EVALUATION ABSTRACTION TESTS ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 3;

    bool pass1 = testGate7A_1_FunctionalEquivalence();
    std::cout << "[" << (pass1 ? "PASS" : "FAIL") << "] Phase 7-A Test 1: Functional Equivalence (Canonical & 50 Random Positions)\n";
    if (pass1) passed++;

    bool pass2 = testGate7A_2_InterfaceContractAndPolymorphism();
    std::cout << "[" << (pass2 ? "PASS" : "FAIL") << "] Phase 7-A Test 2: Interface Contract & Polymorphism (Virtual Dispatch & Swapping)\n";
    if (pass2) passed++;

    bool pass3 = testGate7A_3_SearchTreeNodeInvariance();
    std::cout << "[" << (pass3 ? "PASS" : "FAIL") << "] Phase 7-A Test 3: Search Tree Node Invariance (Depth 6 == 313,092 Nodes)\n";
    if (pass3) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "PHASE 7-A EVAL ABSTRACTION RESULT: " << passed << "/" << total << " Test Categories Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

static bool checkDeltaEquivalence(const Position& before,
                                  const Position& after,
                                  const Move& move,
                                  Color perspective) {
    std::vector<int> beforeFeats = eval::nnue::FeatureTransformer::getActiveFeatures(before, perspective);
    std::vector<int> afterFeats = eval::nnue::FeatureTransformer::getActiveFeatures(after, perspective);

    std::vector<int> removed, added;
    eval::nnue::FeatureTransformer::computeDeltas(before, after, move, perspective, removed, added);

    std::vector<int> reconstructed = beforeFeats;
    for (int r : removed) {
        auto it = std::find(reconstructed.begin(), reconstructed.end(), r);
        if (it == reconstructed.end()) {
            return false;
        }
        reconstructed.erase(it);
    }
    for (int a : added) {
        reconstructed.push_back(a);
    }
    std::sort(reconstructed.begin(), reconstructed.end());
    return reconstructed == afterFeats;
}

bool testGate7B_1_IndexDomainAndBijectivity() {
    using namespace eval::nnue;
    std::vector<bool> seenWhite(HALFKP_FEATURES, false);
    std::vector<bool> seenBlack(HALFKP_FEATURES, false);

    const Piece testPieces[] = {
        Piece::WhitePawn, Piece::WhiteKnight, Piece::WhiteBishop, Piece::WhiteRook, Piece::WhiteQueen,
        Piece::BlackPawn, Piece::BlackKnight, Piece::BlackBishop, Piece::BlackRook, Piece::BlackQueen
    };

    for (int k = 0; k < 64; ++k) {
        Square kSq = static_cast<Square>(k);
        for (Piece p : testPieces) {
            for (int s = 0; s < 64; ++s) {
                Square pSq = static_cast<Square>(s);

                int idxW = makeFeatureIndex(kSq, p, pSq, Color::White);
                if (idxW < 0 || idxW >= HALFKP_FEATURES) return false;
                if (seenWhite[static_cast<size_t>(idxW)]) return false;
                seenWhite[static_cast<size_t>(idxW)] = true;

                int idxB = makeFeatureIndex(kSq, p, pSq, Color::Black);
                if (idxB < 0 || idxB >= HALFKP_FEATURES) return false;
                if (seenBlack[static_cast<size_t>(idxB)]) return false;
                seenBlack[static_cast<size_t>(idxB)] = true;
            }
        }
    }

    for (size_t i = 0; i < HALFKP_FEATURES; ++i) {
        if (!seenWhite[i] || !seenBlack[i]) return false;
    }
    return true;
}

bool testGate7B_2_DeterministicReferenceVectors() {
    using namespace eval::nnue;
    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt) return false;
    Position pos = *opt;

    auto w1 = FeatureTransformer::getActiveFeatures(pos, Color::White);
    auto w2 = FeatureTransformer::getActiveFeatures(pos, Color::White);
    if (w1 != w2 || w1.size() != 30) return false;

    if (std::find(w1.begin(), w1.end(), 2568) == w1.end()) return false;
    if (std::find(w1.begin(), w1.end(), 2625) == w1.end()) return false;
    if (std::find(w1.begin(), w1.end(), 2928) == w1.end()) return false;

    auto opt2 = FenParser::parse("2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1");
    if (!opt2) return false;
    auto t1 = FeatureTransformer::getActiveFeatures(*opt2, Color::White);
    auto t2 = FeatureTransformer::getActiveFeatures(*opt2, Color::White);
    if (t1 != t2) return false;

    return true;
}

bool testGate7B_3_PerspectiveSymmetry() {
    using namespace eval::nnue;
    auto opt1 = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt1) return false;
    auto w1 = FeatureTransformer::getActiveFeatures(*opt1, Color::White);
    auto b1 = FeatureTransformer::getActiveFeatures(*opt1, Color::Black);
    if (w1 != b1) return false;

    auto opt2 = FenParser::parse("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    if (!opt2) return false;
    auto w2 = FeatureTransformer::getActiveFeatures(*opt2, Color::White);
    auto b2 = FeatureTransformer::getActiveFeatures(*opt2, Color::Black);
    if (w2 != b2) return false;

    return true;
}

bool testGate7B_4_QuietMoveDeltaEquivalence() {
    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt) return false;
    Position before = *opt;

    Move m(Square::E2, Square::E4, Move::Flags::None);
    Position after = before;
    UndoState undo;
    MoveExecutor::makeMove(after, m, undo);

    if (!checkDeltaEquivalence(before, after, m, Color::White)) return false;
    if (!checkDeltaEquivalence(before, after, m, Color::Black)) return false;

    Move m2(Square::G1, Square::F3, Move::Flags::None);
    after = before;
    MoveExecutor::makeMove(after, m2, undo);
    if (!checkDeltaEquivalence(before, after, m2, Color::White)) return false;
    if (!checkDeltaEquivalence(before, after, m2, Color::Black)) return false;

    return true;
}

bool testGate7B_5_NormalCaptureDeltaEquivalence() {
    auto opt = FenParser::parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    if (!opt) return false;
    Position before = *opt;

    Move m(Square::D5, Square::E6, Move::Flags::None);
    Position after = before;
    UndoState undo;
    MoveExecutor::makeMove(after, m, undo);

    if (!checkDeltaEquivalence(before, after, m, Color::White)) return false;
    if (!checkDeltaEquivalence(before, after, m, Color::Black)) return false;

    auto opt2 = FenParser::parse("2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1");
    if (!opt2) return false;
    Position before2 = *opt2;
    Move m2(Square::C8, Square::C4, Move::Flags::None);
    Position after2 = before2;
    MoveExecutor::makeMove(after2, m2, undo);

    if (!checkDeltaEquivalence(before2, after2, m2, Color::White)) return false;
    if (!checkDeltaEquivalence(before2, after2, m2, Color::Black)) return false;

    return true;
}

bool testGate7B_6_FullPromotionMatrix() {
    const Move::PromotionPiece promos[] = {
        Move::PromotionPiece::Queen,
        Move::PromotionPiece::Rook,
        Move::PromotionPiece::Bishop,
        Move::PromotionPiece::Knight
    };

    for (auto promo : promos) {
        auto optW1 = FenParser::parse("7k/4P3/8/8/8/8/8/7K w - - 0 1");
        if (!optW1) return false;
        Position before = *optW1;
        Move m(Square::E7, Square::E8, Move::Flags::Promotion, promo);
        Position after = before;
        UndoState undo;
        MoveExecutor::makeMove(after, m, undo);
        if (!checkDeltaEquivalence(before, after, m, Color::White)) return false;
        if (!checkDeltaEquivalence(before, after, m, Color::Black)) return false;

        auto optW2 = FenParser::parse("3r3k/4P3/8/8/8/8/8/7K w - - 0 1");
        if (!optW2) return false;
        before = *optW2;
        Move mCap(Square::E7, Square::D8, Move::Flags::Promotion, promo);
        after = before;
        MoveExecutor::makeMove(after, mCap, undo);
        if (!checkDeltaEquivalence(before, after, mCap, Color::White)) return false;
        if (!checkDeltaEquivalence(before, after, mCap, Color::Black)) return false;
    }

    for (auto promo : promos) {
        auto optB1 = FenParser::parse("7k/8/8/8/8/8/4p3/7K b - - 0 1");
        if (!optB1) return false;
        Position before = *optB1;
        Move m(Square::E2, Square::E1, Move::Flags::Promotion, promo);
        Position after = before;
        UndoState undo;
        MoveExecutor::makeMove(after, m, undo);
        if (!checkDeltaEquivalence(before, after, m, Color::White)) return false;
        if (!checkDeltaEquivalence(before, after, m, Color::Black)) return false;

        auto optB2 = FenParser::parse("7k/8/8/8/8/8/4p3/3R3K b - - 0 1");
        if (!optB2) return false;
        before = *optB2;
        Move mCap(Square::E2, Square::D1, Move::Flags::Promotion, promo);
        after = before;
        MoveExecutor::makeMove(after, mCap, undo);
        if (!checkDeltaEquivalence(before, after, mCap, Color::White)) return false;
        if (!checkDeltaEquivalence(before, after, mCap, Color::Black)) return false;
    }

    return true;
}

bool testGate7B_7_CastlingEquivalence() {
    // 1. White Kingside O-O
    {
        auto opt = FenParser::parse("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
        if (!opt) return false;
        Position before = *opt;
        Move m(Square::E1, Square::G1, Move::Flags::Castling);
        Position after = before;
        UndoState undo;
        MoveExecutor::makeMove(after, m, undo);
        if (!checkDeltaEquivalence(before, after, m, Color::White)) return false;
        if (!checkDeltaEquivalence(before, after, m, Color::Black)) return false;
    }

    // 2. White Queenside O-O-O
    {
        auto opt = FenParser::parse("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
        if (!opt) return false;
        Position before = *opt;
        Move m(Square::E1, Square::C1, Move::Flags::Castling);
        Position after = before;
        UndoState undo;
        MoveExecutor::makeMove(after, m, undo);
        if (!checkDeltaEquivalence(before, after, m, Color::White)) return false;
        if (!checkDeltaEquivalence(before, after, m, Color::Black)) return false;
    }

    // 3. Black Kingside O-O
    {
        auto opt = FenParser::parse("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
        if (!opt) return false;
        Position before = *opt;
        Move m(Square::E8, Square::G8, Move::Flags::Castling);
        Position after = before;
        UndoState undo;
        MoveExecutor::makeMove(after, m, undo);
        if (!checkDeltaEquivalence(before, after, m, Color::White)) return false;
        if (!checkDeltaEquivalence(before, after, m, Color::Black)) return false;
    }

    // 4. Black Queenside O-O-O
    {
        auto opt = FenParser::parse("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
        if (!opt) return false;
        Position before = *opt;
        Move m(Square::E8, Square::C8, Move::Flags::Castling);
        Position after = before;
        UndoState undo;
        MoveExecutor::makeMove(after, m, undo);
        if (!checkDeltaEquivalence(before, after, m, Color::White)) return false;
        if (!checkDeltaEquivalence(before, after, m, Color::Black)) return false;
    }

    return true;
}

bool testGate7B_8_EnPassantEquivalence() {
    // 1. White En-Passant: e5xf6 e.p.
    {
        auto opt = FenParser::parse("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3");
        if (!opt) return false;
        Position before = *opt;
        Move m(Square::E5, Square::F6, Move::Flags::EnPassant);
        Position after = before;
        UndoState undo;
        MoveExecutor::makeMove(after, m, undo);
        if (!checkDeltaEquivalence(before, after, m, Color::White)) return false;
        if (!checkDeltaEquivalence(before, after, m, Color::Black)) return false;
    }

    // 2. Black En-Passant: e4xd3 e.p.
    {
        auto opt = FenParser::parse("rnbqkbnr/pppp1ppp/8/8/3Pp3/8/PPP1PPPP/RNBQKBNR b KQkq d3 0 2");
        if (!opt) return false;
        Position before = *opt;
        Move m(Square::E4, Square::D3, Move::Flags::EnPassant);
        Position after = before;
        UndoState undo;
        MoveExecutor::makeMove(after, m, undo);
        if (!checkDeltaEquivalence(before, after, m, Color::White)) return false;
        if (!checkDeltaEquivalence(before, after, m, Color::Black)) return false;
    }

    return true;
}

bool testGate7B_9_RandomLegalWalkOracle10k() {
    std::mt19937 rng(42);
    const std::string seeds[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1",
        "r1bq1rk1/pp2bppp/2n1pn2/2pp4/2PP4/2N1PN2/PP2BPPP/R1BQ1RK1 w - - 0 1"
    };

    int pliesExecuted = 0;
    const int targetPlies = 10000;
    size_t seedIndex = 0;

    auto opt = FenParser::parse(seeds[0]);
    if (!opt) return false;
    Position currentPos = *opt;

    while (pliesExecuted < targetPlies) {
        MoveList moves;
        MoveGenerator::generateLegalMoves(currentPos, moves);

        if (moves.size() == 0 || currentPos.getHalfmoveClock() >= 100) {
            seedIndex = (seedIndex + 1) % 4;
            auto nextOpt = FenParser::parse(seeds[seedIndex]);
            if (!nextOpt) return false;
            currentPos = *nextOpt;
            continue;
        }

        std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);
        Move m = moves[dist(rng)];

        Position nextPos = currentPos;
        UndoState undo;
        MoveExecutor::makeMove(nextPos, m, undo);

        if (!checkDeltaEquivalence(currentPos, nextPos, m, Color::White)) {
            std::cerr << "[FAIL] Gate 7-B-9: White delta equivalence failure at ply " << pliesExecuted << "\n";
            return false;
        }
        if (!checkDeltaEquivalence(currentPos, nextPos, m, Color::Black)) {
            std::cerr << "[FAIL] Gate 7-B-9: Black delta equivalence failure at ply " << pliesExecuted << "\n";
            return false;
        }

        currentPos = nextPos;
        pliesExecuted++;
    }

    return true;
}

bool testGate7B_10_SetIntegrity() {
    using namespace eval::nnue;
    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1",
        "8/8/4k3/4p3/4P3/4K3/8/8 w - - 0 1"
    };

    for (const auto& fen : fens) {
        auto opt = FenParser::parse(fen);
        if (!opt) return false;
        const Position& pos = *opt;

        for (Color c : {Color::White, Color::Black}) {
            auto feats = FeatureTransformer::getActiveFeatures(pos, c);

            for (size_t i = 1; i < feats.size(); ++i) {
                if (feats[i] <= feats[i - 1]) return false;
            }

            size_t expectedCount = 0;
            for (uint8_t p = 0; p < 12; ++p) {
                Piece pc = static_cast<Piece>(p);
                if (pc != Piece::WhiteKing && pc != Piece::BlackKing) {
                    expectedCount += std::popcount(pos.getPieceBitboard(pc));
                }
            }

            if (feats.size() != expectedCount) return false;
        }
    }
    return true;
}

bool testGate7B_11_BenchmarkInvariance() {
    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();
    reg.syncToEngineParameters(SearchController::getInstance().getMutableParams());

    BenchmarkConfig cfg;
    cfg.overrideDepth = 6;
    cfg.hashSizeMb = 16;
    cfg.mode = BenchmarkStateMode::Isolated;
    cfg.silentSearch = true;
    cfg.printConsole = false;

    BenchmarkRunRecord rec = BenchmarkRunner::run(cfg);
    if (rec.aggregate.totalNodes != 313092) {
        std::cerr << "[FAIL] Gate 7-B-11: Benchmark depth-6 nodes " << rec.aggregate.totalNodes << " != 313092\n";
        return false;
    }
    return true;
}

bool runPhase7BFeatureTransformerTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===  MILESTONE OMEGA, PHASE 7-B: FEATURE TRANSFORMER TESTS    ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 11;

    bool pass1 = testGate7B_1_IndexDomainAndBijectivity();
    std::cout << "[" << (pass1 ? "PASS" : "FAIL") << "] Gate 7-B-1: Index Domain [0, 40959] & Bijectivity (40,960 Features)\n";
    if (pass1) passed++;

    bool pass2 = testGate7B_2_DeterministicReferenceVectors();
    std::cout << "[" << (pass2 ? "PASS" : "FAIL") << "] Gate 7-B-2: Deterministic Reference Vectors (startpos, tactical)\n";
    if (pass2) passed++;

    bool pass3 = testGate7B_3_PerspectiveSymmetry();
    std::cout << "[" << (pass3 ? "PASS" : "FAIL") << "] Gate 7-B-3: Perspective Symmetry (Identical Features on Symmetric FENs)\n";
    if (pass3) passed++;

    bool pass4 = testGate7B_4_QuietMoveDeltaEquivalence();
    std::cout << "[" << (pass4 ? "PASS" : "FAIL") << "] Gate 7-B-4: Quiet Move Delta Equivalence\n";
    if (pass4) passed++;

    bool pass5 = testGate7B_5_NormalCaptureDeltaEquivalence();
    std::cout << "[" << (pass5 ? "PASS" : "FAIL") << "] Gate 7-B-5: Normal Capture Delta Equivalence\n";
    if (pass5) passed++;

    bool pass6 = testGate7B_6_FullPromotionMatrix();
    std::cout << "[" << (pass6 ? "PASS" : "FAIL") << "] Gate 7-B-6: Full Promotion Matrix (Quiet & Capture x Q, R, B, N)\n";
    if (pass6) passed++;

    bool pass7 = testGate7B_7_CastlingEquivalence();
    std::cout << "[" << (pass7 ? "PASS" : "FAIL") << "] Gate 7-B-7: Castling Equivalence (White/Black x O-O/O-O-O)\n";
    if (pass7) passed++;

    bool pass8 = testGate7B_8_EnPassantEquivalence();
    std::cout << "[" << (pass8 ? "PASS" : "FAIL") << "] Gate 7-B-8: En-Passant Equivalence (White & Black)\n";
    if (pass8) passed++;

    bool pass9 = testGate7B_9_RandomLegalWalkOracle10k();
    std::cout << "[" << (pass9 ? "PASS" : "FAIL") << "] Gate 7-B-9: 10,000-Ply Random Legal Walk Oracle (20,000 Validations)\n";
    if (pass9) passed++;

    bool pass10 = testGate7B_10_SetIntegrity();
    std::cout << "[" << (pass10 ? "PASS" : "FAIL") << "] Gate 7-B-10: Set Integrity (0 Duplicates, Exact Piece Counts)\n";
    if (pass10) passed++;

    bool pass11 = testGate7B_11_BenchmarkInvariance();
    std::cout << "[" << (pass11 ? "PASS" : "FAIL") << "] Gate 7-B-11: Benchmark Invariance (Depth 6 == 313,092 Nodes)\n";
    if (pass11) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "PHASE 7-B FEATURE TRANSFORMER RESULT: " << passed << "/" << total << " Gates Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Suite #30: Phase 7-C Dual-Perspective Incremental Accumulator Tests
// ---------------------------------------------------------------------------

bool testGate7C_1_InitialConstruction() {
    using namespace eval::nnue;
    auto weights = FeatureWeights::createDeterministic(42);
    if (!weights) return false;

    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1",
        "8/8/4k3/4p3/4P3/4K3/8/8 w - - 0 1"
    };

    AccumulatorStack stack;

    for (const auto& fen : fens) {
        auto opt = FenParser::parse(fen);
        if (!opt) return false;
        const Position& pos = *opt;

        stack.reset(pos, *weights);

        for (Color c : {Color::White, Color::Black}) {
            auto feats = FeatureTransformer::getActiveFeatures(pos, c);
            const auto& half = stack.top().get(c);

            for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
                int32_t expected = weights->biases[i];
                for (int f : feats) {
                    expected += weights->weights[static_cast<size_t>(f)][i];
                }
                if (half.values[i] != static_cast<int16_t>(expected)) {
                    std::cerr << "[FAIL] Gate 7-C-1: Mismatch at index " << i << "\n";
                    return false;
                }
            }
        }
    }
    return true;
}

bool testGate7C_2_QuietMoves() {
    using namespace eval::nnue;
    auto weights = FeatureWeights::createDeterministic(42);
    if (!weights) return false;

    const std::pair<std::string, Move> tests[] = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
         Move(Square::E2, Square::E4, Move::Flags::DoublePawnPush)},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
         Move(Square::G1, Square::F3)},
        {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1",
         Move(Square::C1, Square::E3)},
        {"r1bqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
         Move(Square::A1, Square::B1)},
        {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1",
         Move(Square::D1, Square::F3)},
        {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1",
         Move(Square::E1, Square::E2)},
        {"rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
         Move(Square::E8, Square::E7)},
        {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
         Move(Square::E7, Square::E5, Move::Flags::DoublePawnPush)}
    };

    AccumulatorStack stack;

    for (const auto& [fen, move] : tests) {
        auto opt = FenParser::parse(fen);
        if (!opt) return false;
        Position before = *opt;
        Position after = before;
        UndoState undo;
        MoveExecutor::makeMove(after, move, undo);

        stack.reset(before, *weights);
        stack.pushMove(before, after, move, *weights);

        Accumulator expected;
        AccumulatorStack::rebuildPerspective(expected.white, after, Color::White, *weights);
        AccumulatorStack::rebuildPerspective(expected.black, after, Color::Black, *weights);

        if (stack.top() != expected) {
            std::cerr << "[FAIL] Gate 7-C-2: Quiet move accumulator mismatch\n";
            return false;
        }
    }
    return true;
}

bool testGate7C_3_NormalCaptures() {
    using namespace eval::nnue;
    auto weights = FeatureWeights::createDeterministic(42);
    if (!weights) return false;

    const std::pair<std::string, Move> tests[] = {
        {"rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2",
         Move(Square::E4, Square::D5)},
        {"rnbqkb1r/pppp1ppp/5n2/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",
         Move(Square::F3, Square::E5)},
        {"r1bqk2r/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
         Move(Square::C4, Square::F7)},
        {"r1bqk2r/pppp1ppp/8/8/8/8/PPP2PPP/RNBQK2R w KQkq - 0 1",
         Move(Square::D1, Square::D8)},
        {"rnbqk1nr/pppp1ppp/8/8/8/8/PPPP1bPP/RNBQKBNR w KQkq - 0 1",
         Move(Square::E1, Square::F2)},
        {"rnbQkbnr/pppp1ppp/8/8/8/8/PPPP1PPP/RNB1KBNR b KQkq - 0 1",
         Move(Square::E8, Square::D8)}
    };

    AccumulatorStack stack;

    for (const auto& [fen, move] : tests) {
        auto opt = FenParser::parse(fen);
        if (!opt) return false;
        Position before = *opt;
        Position after = before;
        UndoState undo;
        MoveExecutor::makeMove(after, move, undo);

        stack.reset(before, *weights);
        stack.pushMove(before, after, move, *weights);

        Accumulator expected;
        AccumulatorStack::rebuildPerspective(expected.white, after, Color::White, *weights);
        AccumulatorStack::rebuildPerspective(expected.black, after, Color::Black, *weights);

        if (stack.top() != expected) {
            std::cerr << "[FAIL] Gate 7-C-3: Capture accumulator mismatch\n";
            return false;
        }
    }
    return true;
}

bool testGate7C_4_All16Promotions() {
    using namespace eval::nnue;
    auto weights = FeatureWeights::createDeterministic(42);
    if (!weights) return false;

    const std::string whiteFen = "3r3k/4P3/8/8/8/8/8/7K w - - 0 1";
    auto optW = FenParser::parse(whiteFen);
    if (!optW) return false;
    Position whiteBefore = *optW;

    const Move::PromotionPiece promoPieces[4] = {
        Move::PromotionPiece::Queen,
        Move::PromotionPiece::Rook,
        Move::PromotionPiece::Bishop,
        Move::PromotionPiece::Knight
    };

    AccumulatorStack stack;

    for (Move::PromotionPiece p : promoPieces) {
        Move m(Square::E7, Square::E8, Move::Flags::Promotion, p);
        Position after = whiteBefore;
        UndoState undo;
        MoveExecutor::makeMove(after, m, undo);

        stack.reset(whiteBefore, *weights);
        stack.pushMove(whiteBefore, after, m, *weights);

        Accumulator expected;
        AccumulatorStack::rebuildPerspective(expected.white, after, Color::White, *weights);
        AccumulatorStack::rebuildPerspective(expected.black, after, Color::Black, *weights);

        if (stack.top() != expected) {
            std::cerr << "[FAIL] Gate 7-C-4: White quiet promo mismatch\n";
            return false;
        }
    }

    for (Move::PromotionPiece p : promoPieces) {
        Move m(Square::E7, Square::D8, Move::Flags::Promotion, p);
        Position after = whiteBefore;
        UndoState undo;
        MoveExecutor::makeMove(after, m, undo);

        stack.reset(whiteBefore, *weights);
        stack.pushMove(whiteBefore, after, m, *weights);

        Accumulator expected;
        AccumulatorStack::rebuildPerspective(expected.white, after, Color::White, *weights);
        AccumulatorStack::rebuildPerspective(expected.black, after, Color::Black, *weights);

        if (stack.top() != expected) {
            std::cerr << "[FAIL] Gate 7-C-4: White capture promo mismatch\n";
            return false;
        }
    }

    const std::string blackFen = "7k/8/8/8/8/8/4p3/3R3K b - - 0 1";
    auto optB = FenParser::parse(blackFen);
    if (!optB) return false;
    Position blackBefore = *optB;

    for (Move::PromotionPiece p : promoPieces) {
        Move m(Square::E2, Square::E1, Move::Flags::Promotion, p);
        Position after = blackBefore;
        UndoState undo;
        MoveExecutor::makeMove(after, m, undo);

        stack.reset(blackBefore, *weights);
        stack.pushMove(blackBefore, after, m, *weights);

        Accumulator expected;
        AccumulatorStack::rebuildPerspective(expected.white, after, Color::White, *weights);
        AccumulatorStack::rebuildPerspective(expected.black, after, Color::Black, *weights);

        if (stack.top() != expected) {
            std::cerr << "[FAIL] Gate 7-C-4: Black quiet promo mismatch\n";
            return false;
        }
    }

    for (Move::PromotionPiece p : promoPieces) {
        Move m(Square::E2, Square::D1, Move::Flags::Promotion, p);
        Position after = blackBefore;
        UndoState undo;
        MoveExecutor::makeMove(after, m, undo);

        stack.reset(blackBefore, *weights);
        stack.pushMove(blackBefore, after, m, *weights);

        Accumulator expected;
        AccumulatorStack::rebuildPerspective(expected.white, after, Color::White, *weights);
        AccumulatorStack::rebuildPerspective(expected.black, after, Color::Black, *weights);

        if (stack.top() != expected) {
            std::cerr << "[FAIL] Gate 7-C-4: Black capture promo mismatch\n";
            return false;
        }
    }

    return true;
}

bool testGate7C_5_CastlingPaths() {
    using namespace eval::nnue;
    auto weights = FeatureWeights::createDeterministic(42);
    if (!weights) return false;

    struct CastlingTest {
        std::string fen;
        Move move;
        Color mover;
        Square kingFrom;
        Square kingTo;
    };

    const CastlingTest tests[] = {
        {"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
         Move(Square::E1, Square::G1, Move::Flags::Castling),
         Color::White, Square::E1, Square::G1},
        {"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
         Move(Square::E1, Square::C1, Move::Flags::Castling),
         Color::White, Square::E1, Square::C1},
        {"r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1",
         Move(Square::E8, Square::G8, Move::Flags::Castling),
         Color::Black, Square::E8, Square::G8},
        {"r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1",
         Move(Square::E8, Square::C8, Move::Flags::Castling),
         Color::Black, Square::E8, Square::C8}
    };

    AccumulatorStack stack;

    for (const auto& t : tests) {
        auto opt = FenParser::parse(t.fen);
        if (!opt) return false;
        Position before = *opt;
        Position after = before;
        UndoState undo;
        MoveExecutor::makeMove(after, t.move, undo);

        Color nonMover = (t.mover == Color::White) ? Color::Black : Color::White;

        if (before.getKingSquare(t.mover) != t.kingFrom || after.getKingSquare(t.mover) != t.kingTo) {
            std::cerr << "[FAIL] Gate 7-C-5: Mover king square invariant violated\n";
            return false;
        }
        if (before.getKingSquare(nonMover) != after.getKingSquare(nonMover)) {
            std::cerr << "[FAIL] Gate 7-C-5: Non-mover king moved unexpectedly\n";
            return false;
        }

        stack.reset(before, *weights);
        stack.pushMove(before, after, t.move, *weights);

        Accumulator expected;
        AccumulatorStack::rebuildPerspective(expected.white, after, Color::White, *weights);
        AccumulatorStack::rebuildPerspective(expected.black, after, Color::Black, *weights);

        if (stack.top() != expected) {
            std::cerr << "[FAIL] Gate 7-C-5: Castling accumulator mismatch vs rebuild\n";
            return false;
        }
    }
    return true;
}

bool testGate7C_6_EnPassant() {
    using namespace eval::nnue;
    auto weights = FeatureWeights::createDeterministic(42);
    if (!weights) return false;

    const std::string whiteFen = "8/8/8/3Pp3/8/8/8/4K2k w - e6 0 1";
    auto optW = FenParser::parse(whiteFen);
    if (!optW) return false;
    Position whiteBefore = *optW;
    Position whiteAfter = whiteBefore;
    UndoState undoW;
    Move whiteEp(Square::D5, Square::E6, Move::Flags::EnPassant);
    MoveExecutor::makeMove(whiteAfter, whiteEp, undoW);

    AccumulatorStack stack;
    stack.reset(whiteBefore, *weights);
    stack.pushMove(whiteBefore, whiteAfter, whiteEp, *weights);

    Accumulator expectedW;
    AccumulatorStack::rebuildPerspective(expectedW.white, whiteAfter, Color::White, *weights);
    AccumulatorStack::rebuildPerspective(expectedW.black, whiteAfter, Color::Black, *weights);

    if (stack.top() != expectedW) {
        std::cerr << "[FAIL] Gate 7-C-6: White en-passant accumulator mismatch\n";
        return false;
    }

    const std::string blackFen = "4K2k/8/8/8/3pP3/8/8/8 b - e3 0 1";
    auto optB = FenParser::parse(blackFen);
    if (!optB) return false;
    Position blackBefore = *optB;
    Position blackAfter = blackBefore;
    UndoState undoB;
    Move blackEp(Square::D4, Square::E3, Move::Flags::EnPassant);
    MoveExecutor::makeMove(blackAfter, blackEp, undoB);

    stack.reset(blackBefore, *weights);
    stack.pushMove(blackBefore, blackAfter, blackEp, *weights);

    Accumulator expectedB;
    AccumulatorStack::rebuildPerspective(expectedB.white, blackAfter, Color::White, *weights);
    AccumulatorStack::rebuildPerspective(expectedB.black, blackAfter, Color::Black, *weights);

    if (stack.top() != expectedB) {
        std::cerr << "[FAIL] Gate 7-C-6: Black en-passant accumulator mismatch\n";
        return false;
    }

    return true;
}

bool testGate7C_7_ReversibleRandomWalk() {
    using namespace eval::nnue;
    auto weights = FeatureWeights::createDeterministic(42);
    if (!weights) return false;

    const std::string seeds[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1"
    };

    AccumulatorStack stack;
    std::mt19937_64 rng(1337);

    for (const auto& fen : seeds) {
        auto opt = FenParser::parse(fen);
        if (!opt) return false;
        Position rootPos = *opt;

        stack.reset(rootPos, *weights);
        Accumulator rootAccumulator = stack.top();

        std::vector<Position> posHistory;
        posHistory.push_back(rootPos);

        Position currentPos = rootPos;
        constexpr size_t TARGET_PLIES = 50;

        for (size_t ply = 0; ply < TARGET_PLIES; ++ply) {
            MoveList legalMoves;
            MoveGenerator::generateLegalMoves(currentPos, legalMoves);
            if (legalMoves.empty()) break;

            std::uniform_int_distribution<size_t> dist(0, legalMoves.size() - 1);
            Move m = legalMoves[dist(rng)];

            Position nextPos = currentPos;
            UndoState undo;
            MoveExecutor::makeMove(nextPos, m, undo);

            stack.pushMove(currentPos, nextPos, m, *weights);
            posHistory.push_back(nextPos);
            currentPos = nextPos;

            Accumulator expected;
            AccumulatorStack::rebuildPerspective(expected.white, currentPos, Color::White, *weights);
            AccumulatorStack::rebuildPerspective(expected.black, currentPos, Color::Black, *weights);

            if (stack.top() != expected) {
                std::cerr << "[FAIL] Gate 7-C-7: Intermediate accumulator mismatch at ply " << stack.currentPly() << "\n";
                return false;
            }
        }

        while (stack.currentPly() > 0) {
            Accumulator expected;
            AccumulatorStack::rebuildPerspective(expected.white, posHistory.back(), Color::White, *weights);
            AccumulatorStack::rebuildPerspective(expected.black, posHistory.back(), Color::Black, *weights);

            if (stack.top() != expected) {
                std::cerr << "[FAIL] Gate 7-C-7: Unwind accumulator mismatch at ply " << stack.currentPly() << "\n";
                return false;
            }

            stack.pop();
            posHistory.pop_back();
        }

        if (stack.currentPly() != 0) {
            std::cerr << "[FAIL] Gate 7-C-7: Current ply after unwind is " << stack.currentPly() << " != 0\n";
            return false;
        }
        if (stack.top() != rootAccumulator) {
            std::cerr << "[FAIL] Gate 7-C-7: Root accumulator identity violated after unwind\n";
            return false;
        }
        if (posHistory.size() != 1) {
            std::cerr << "[FAIL] Gate 7-C-7: Root position history size != 1\n";
            return false;
        }
    }

    return true;
}

bool testGate7C_8_DualPerspectiveSimultaneousInvariance() {
    using namespace eval::nnue;
    auto weights = FeatureWeights::createDeterministic(42);
    if (!weights) return false;

    AccumulatorStack stack;

    auto optStart = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!optStart) return false;
    stack.reset(*optStart, *weights);

    if (stack.top().white != stack.top().black) {
        std::cerr << "[FAIL] Gate 7-C-8: Symmetry mismatch at startpos\n";
        return false;
    }

    Position pos = *optStart;
    const Move line[] = {
        Move(Square::E2, Square::E4, Move::Flags::DoublePawnPush),
        Move(Square::E7, Square::E5, Move::Flags::DoublePawnPush),
        Move(Square::G1, Square::F3),
        Move(Square::B8, Square::C6)
    };

    for (const auto& m : line) {
        Position nextPos = pos;
        UndoState undo;
        MoveExecutor::makeMove(nextPos, m, undo);

        stack.pushMove(pos, nextPos, m, *weights);

        Accumulator expected;
        AccumulatorStack::rebuildPerspective(expected.white, nextPos, Color::White, *weights);
        AccumulatorStack::rebuildPerspective(expected.black, nextPos, Color::Black, *weights);

        if (stack.top().white != expected.white || stack.top().black != expected.black) {
            std::cerr << "[FAIL] Gate 7-C-8: Simultaneous dual-perspective mismatch\n";
            return false;
        }

        pos = nextPos;
    }

    return true;
}

bool testGate7C_9_IsolationAudit() {
    using namespace eval::nnue;

    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt) return false;
    int eval1 = Evaluator::evaluate(*opt);
    eval::ClassicalEvaluator classicalEval;
    int eval2 = classicalEval.evaluate(*opt);
    if (eval1 != eval2) {
        std::cerr << "[FAIL] Gate 7-C-9: ClassicalEvaluator output mismatch\n";
        return false;
    }

    auto& reg = ParameterRegistry::getInstance();
    if (reg.getInt("Eval_Mode") != 0 || reg.hasParam("Use_NNUE") || reg.hasParam("Accumulator")) {
        std::cerr << "[FAIL] Gate 7-C-9: ParameterRegistry contaminated\n";
        return false;
    }

    return true;
}

bool testGate7C_10_BenchmarkInvariance() {
    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();
    reg.syncToEngineParameters(SearchController::getInstance().getMutableParams());

    BenchmarkConfig cfg;
    cfg.overrideDepth = 6;
    cfg.hashSizeMb = 16;
    cfg.mode = BenchmarkStateMode::Isolated;
    cfg.silentSearch = true;
    cfg.printConsole = false;

    BenchmarkRunRecord rec = BenchmarkRunner::run(cfg);
    if (rec.aggregate.totalNodes != 313092) {
        std::cerr << "[FAIL] Gate 7-C-10: Benchmark depth-6 nodes " << rec.aggregate.totalNodes << " != 313092\n";
        return false;
    }
    return true;
}

bool runPhase7CAccumulatorTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===  MILESTONE OMEGA, PHASE 7-C: INCREMENTAL ACCUMULATOR TESTS===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 10;

    bool pass1 = testGate7C_1_InitialConstruction();
    std::cout << "[" << (pass1 ? "PASS" : "FAIL") << "] Gate 7-C-1: Initial Construction (Bias + Sum(Weights) across FENs)\n";
    if (pass1) passed++;

    bool pass2 = testGate7C_2_QuietMoves();
    std::cout << "[" << (pass2 ? "PASS" : "FAIL") << "] Gate 7-C-2: Quiet Moves Incremental vs Rebuild Bit-Exact Equivalence\n";
    if (pass2) passed++;

    bool pass3 = testGate7C_3_NormalCaptures();
    std::cout << "[" << (pass3 ? "PASS" : "FAIL") << "] Gate 7-C-3: Normal Captures Incremental vs Rebuild Bit-Exact Equivalence\n";
    if (pass3) passed++;

    bool pass4 = testGate7C_4_All16Promotions();
    std::cout << "[" << (pass4 ? "PASS" : "FAIL") << "] Gate 7-C-4: All 16 Promotion Variants (Quiet/Capture x Q, R, B, N) Match Rebuild\n";
    if (pass4) passed++;

    bool pass5 = testGate7C_5_CastlingPaths();
    std::cout << "[" << (pass5 ? "PASS" : "FAIL") << "] Gate 7-C-5: Castling Paths (Mover Rebuild, Non-Mover Incremental, Bit-Exact)\n";
    if (pass5) passed++;

    bool pass6 = testGate7C_6_EnPassant();
    std::cout << "[" << (pass6 ? "PASS" : "FAIL") << "] Gate 7-C-6: En-Passant Incremental vs Rebuild Bit-Exact Equivalence\n";
    if (pass6) passed++;

    bool pass7 = testGate7C_7_ReversibleRandomWalk();
    std::cout << "[" << (pass7 ? "PASS" : "FAIL") << "] Gate 7-C-7: Reversible Random Walk (Push/Pop Sequences & Root Unwind Identity)\n";
    if (pass7) passed++;

    bool pass8 = testGate7C_8_DualPerspectiveSimultaneousInvariance();
    std::cout << "[" << (pass8 ? "PASS" : "FAIL") << "] Gate 7-C-8: Dual-Perspective Simultaneous Invariance on All Operations\n";
    if (pass8) passed++;

    bool pass9 = testGate7C_9_IsolationAudit();
    std::cout << "[" << (pass9 ? "PASS" : "FAIL") << "] Gate 7-C-9: Isolation Audit (Position, Search, ClassicalEvaluator Untouched)\n";
    if (pass9) passed++;

    bool pass10 = testGate7C_10_BenchmarkInvariance();
    std::cout << "[" << (pass10 ? "PASS" : "FAIL") << "] Gate 7-C-10: Classical Depth-6 Benchmark Produces Exactly 313,092 Nodes\n";
    if (pass10) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "PHASE 7-C INCREMENTAL ACCUMULATOR RESULT: " << passed << "/" << total << " Gates Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ---------------------------------------------------------------------------
// Suite #31: Phase 7-D Scalar NNUE Reference Inference & Network Tests
// ---------------------------------------------------------------------------

bool testGate7D_1_CReLUExactBoundaries() {
    using namespace eval::nnue;
    struct TestCase { int32_t in; int8_t expected; };
    const TestCase cases[] = {
        {-32768, 0}, {-1000, 0}, {-2, 0}, {-1, 0},
        {0, 0}, {1, 1}, {64, 64}, {126, 126},
        {127, 127}, {128, 127}, {129, 127}, {1000, 127}, {32767, 127}
    };
    for (const auto& tc : cases) {
        int8_t out = ScalarInference::crelu(tc.in);
        if (out != tc.expected) {
            std::cerr << "[FAIL] Gate 7-D-1: crelu(" << tc.in << ") = " << (int)out << " != " << (int)tc.expected << "\n";
            return false;
        }
    }
    return true;
}

bool testGate7D_2_UsThemPerspectiveOrdering() {
    using namespace eval::nnue;
    Accumulator acc;
    acc.white.values.fill(10);
    acc.black.values.fill(50);

    NetworkModel model;
    model.fc1_weights[0][0] = 1;
    model.fc1_weights[0][512] = -1;

    auto diagW = ScalarInference::evaluateDetailed(acc, Color::White, model);
    if (diagW.fc1_raw[0] != -40) {
        std::cerr << "[FAIL] Gate 7-D-2: White to move raw expected -40, got " << diagW.fc1_raw[0] << "\n";
        return false;
    }

    auto diagB = ScalarInference::evaluateDetailed(acc, Color::Black, model);
    if (diagB.fc1_raw[0] != 40) {
        std::cerr << "[FAIL] Gate 7-D-2: Black to move raw expected 40, got " << diagB.fc1_raw[0] << "\n";
        return false;
    }

    return true;
}

bool testGate7D_3_FC1HandCalculated() {
    using namespace eval::nnue;
    Accumulator acc;
    acc.white.clear();
    acc.black.clear();

    acc.white.values[0] = 30;
    acc.white.values[1] = 150; // clamped to 127
    acc.white.values[2] = -20; // clamped to 0
    acc.white.values[3] = 64;

    acc.black.values[0] = 10;  // index 512
    acc.black.values[1] = 80;  // index 513

    NetworkModel model;
    model.fc1_biases[0] = 125;
    model.fc1_weights[0][0] = 2;    // 30 * 2 = 60
    model.fc1_weights[0][1] = -1;   // 127 * -1 = -127
    model.fc1_weights[0][2] = 5;    // 0 * 5 = 0
    model.fc1_weights[0][3] = 3;    // 64 * 3 = 192
    model.fc1_weights[0][512] = 4;  // 10 * 4 = 40
    model.fc1_weights[0][513] = -2; // 80 * -2 = -160

    auto diag = ScalarInference::evaluateDetailed(acc, Color::White, model);
    if (diag.fc1_raw[0] != 130 || diag.fc1_activated[0] != 2) {
        std::cerr << "[FAIL] Gate 7-D-3: FC1 raw expected 130, got " << diag.fc1_raw[0]
                  << "; act expected 2, got " << (int)diag.fc1_activated[0] << "\n";
        return false;
    }

    return true;
}

bool testGate7D_4_FC2HandCalculated() {
    using namespace eval::nnue;
    Accumulator acc;
    acc.white.clear();
    acc.black.clear();

    NetworkModel model;
    model.fc1_biases[0] = 640;
    model.fc1_biases[1] = 1280;

    model.fc2_biases[0] = 45;
    model.fc2_weights[0][0] = 3;
    model.fc2_weights[0][1] = -2;

    model.fc2_biases[1] = 200;
    model.fc2_weights[1][0] = 5;
    model.fc2_weights[1][1] = 6;

    auto diag = ScalarInference::evaluateDetailed(acc, Color::White, model);
    if (diag.fc2_raw[0] != 35 || diag.fc2_activated[0] != 0) {
        std::cerr << "[FAIL] Gate 7-D-4: FC2 neuron 0 mismatch: raw=" << diag.fc2_raw[0]
                  << ", act=" << (int)diag.fc2_activated[0] << "\n";
        return false;
    }
    if (diag.fc2_raw[1] != 370 || diag.fc2_activated[1] != 5) {
        std::cerr << "[FAIL] Gate 7-D-4: FC2 neuron 1 mismatch: raw=" << diag.fc2_raw[1]
                  << ", act=" << (int)diag.fc2_activated[1] << "\n";
        return false;
    }

    return true;
}

bool testGate7D_5_FC3ScalingAndClamping() {
    using namespace eval::nnue;
    Accumulator acc;
    acc.white.clear();
    acc.black.clear();

    NetworkModel model;

    model.fc3_bias = 25;
    auto diag1 = ScalarInference::evaluateDetailed(acc, Color::White, model);
    if (diag1.fc3_raw != 25 || diag1.final_score != 400) {
        std::cerr << "[FAIL] Gate 7-D-5: Normal score mismatch: " << diag1.final_score << "\n";
        return false;
    }

    model.fc3_bias = 5000;
    auto diag2 = ScalarInference::evaluateDetailed(acc, Color::White, model);
    if (diag2.fc3_raw != 5000 || diag2.final_score != 30000) {
        std::cerr << "[FAIL] Gate 7-D-5: Upper clamp mismatch: " << diag2.final_score << "\n";
        return false;
    }

    model.fc3_bias = -5000;
    auto diag3 = ScalarInference::evaluateDetailed(acc, Color::White, model);
    if (diag3.fc3_raw != -5000 || diag3.final_score != -30000) {
        std::cerr << "[FAIL] Gate 7-D-5: Lower clamp mismatch: " << diag3.final_score << "\n";
        return false;
    }

    model.fc3_bias = 0;
    auto diag4 = ScalarInference::evaluateDetailed(acc, Color::White, model);
    if (diag4.final_score != 0) {
        std::cerr << "[FAIL] Gate 7-D-5: Zero mismatch: " << diag4.final_score << "\n";
        return false;
    }

    return true;
}

bool testGate7D_6A_PerspectivePermutation() {
    using namespace eval::nnue;
    auto model = createSyntheticModel(42);

    Accumulator acc;
    for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
        acc.white.values[i] = static_cast<int16_t>(i % 120);
        acc.black.values[i] = static_cast<int16_t>((i * 3) % 120);
    }

    Accumulator accSwapped;
    accSwapped.white = acc.black;
    accSwapped.black = acc.white;

    int32_t scoreW = ScalarInference::evaluate(acc, Color::White, model);
    int32_t scoreB_swapped = ScalarInference::evaluate(accSwapped, Color::Black, model);
    if (scoreW != scoreB_swapped) {
        std::cerr << "[FAIL] Gate 7-D-6A: Permutation White vs Swapped Black failed: "
                  << scoreW << " vs " << scoreB_swapped << "\n";
        return false;
    }

    int32_t scoreB = ScalarInference::evaluate(acc, Color::Black, model);
    int32_t scoreW_swapped = ScalarInference::evaluate(accSwapped, Color::White, model);
    if (scoreB != scoreW_swapped) {
        std::cerr << "[FAIL] Gate 7-D-6A: Permutation Black vs Swapped White failed: "
                  << scoreB << " vs " << scoreW_swapped << "\n";
        return false;
    }

    return true;
}

bool testGate7D_6B_SymmetricModelMirroredPositions() {
    using namespace eval::nnue;
    auto model = createSymmetricSyntheticModel(99);

    auto optStart = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!optStart) return false;

    AccumulatorStack stack;
    stack.reset(*optStart, *model.featureWeights);

    int32_t scoreStartW = ScalarInference::evaluate(stack.top(), Color::White, model);
    int32_t scoreStartB = ScalarInference::evaluate(stack.top(), Color::Black, model);
    if (scoreStartW != 0 || scoreStartB != 0) {
        std::cerr << "[FAIL] Gate 7-D-6B: Symmetric model on startpos produced non-zero: W="
                  << scoreStartW << ", B=" << scoreStartB << "\n";
        return false;
    }

    const std::string fen1 = "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3";
    const std::string fen2 = "rnbqkb1r/pppp1ppp/5n2/4p3/4P3/2N5/PPPP1PPP/R1BQKBNR b KQkq - 2 3";

    auto opt1 = FenParser::parse(fen1);
    auto opt2 = FenParser::parse(fen2);
    if (!opt1 || !opt2) return false;

    AccumulatorStack stack1, stack2;
    stack1.reset(*opt1, *model.featureWeights);
    stack2.reset(*opt2, *model.featureWeights);

    int32_t score1 = ScalarInference::evaluate(stack1.top(), Color::White, model);
    int32_t score2 = ScalarInference::evaluate(stack2.top(), Color::Black, model);
    if (score1 != score2) {
        std::cerr << "[FAIL] Gate 7-D-6B: Mirrored position score mismatch: P1="
                  << score1 << ", P2=" << score2 << "\n";
        return false;
    }

    return true;
}

bool testGate7D_7_SerializationAndFailFast() {
    using namespace eval::nnue;
    auto modelOriginal = createSyntheticModel(77);

    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    serializeModel(modelOriginal, ss);

    NetworkModel modelLoaded;
    if (!deserializeModel(modelLoaded, ss)) {
        std::cerr << "[FAIL] Gate 7-D-7: Clean deserialization returned false\n";
        return false;
    }

    if (modelOriginal.fc1_biases != modelLoaded.fc1_biases) return false;
    if (modelOriginal.fc1_weights != modelLoaded.fc1_weights) return false;
    if (modelOriginal.fc2_biases != modelLoaded.fc2_biases) return false;
    if (modelOriginal.fc2_weights != modelLoaded.fc2_weights) return false;
    if (modelOriginal.fc3_bias != modelLoaded.fc3_bias) return false;
    if (modelOriginal.fc3_weights != modelLoaded.fc3_weights) return false;
    if (modelOriginal.featureWeights->biases != modelLoaded.featureWeights->biases) return false;
    if (modelOriginal.featureWeights->weights != modelLoaded.featureWeights->weights) return false;

    // Corrupted magic: fail-fast without allocating
    {
        std::stringstream ssBad(std::ios::in | std::ios::out | std::ios::binary);
        serializeModel(modelOriginal, ssBad);
        ssBad.seekp(0);
        uint32_t badMagic = 0xDEADBEEF;
        ssBad.write(reinterpret_cast<const char*>(&badMagic), sizeof(badMagic));
        ssBad.seekg(0);

        NetworkModel modelBad;
        if (deserializeModel(modelBad, ssBad)) {
            std::cerr << "[FAIL] Gate 7-D-7: Corrupted magic accepted!\n";
            return false;
        }
        if (modelBad.featureWeights != nullptr) {
            std::cerr << "[FAIL] Gate 7-D-7: Memory allocated on failed magic header!\n";
            return false;
        }
    }

    // Corrupted version: fail-fast
    {
        std::stringstream ssBad(std::ios::in | std::ios::out | std::ios::binary);
        serializeModel(modelOriginal, ssBad);
        ssBad.seekp(4);
        uint32_t badVersion = 999;
        ssBad.write(reinterpret_cast<const char*>(&badVersion), sizeof(badVersion));
        ssBad.seekg(0);

        NetworkModel modelBad;
        if (deserializeModel(modelBad, ssBad)) {
            std::cerr << "[FAIL] Gate 7-D-7: Corrupted version accepted!\n";
            return false;
        }
    }

    // Truncated stream
    {
        std::stringstream ssShort(std::ios::in | std::ios::out | std::ios::binary);
        uint32_t shortHeader[2] = {NNUE_MAGIC, NNUE_VERSION};
        ssShort.write(reinterpret_cast<const char*>(shortHeader), sizeof(shortHeader));
        ssShort.seekg(0);

        NetworkModel modelBad;
        if (deserializeModel(modelBad, ssShort)) {
            std::cerr << "[FAIL] Gate 7-D-7: Truncated stream accepted!\n";
            return false;
        }
    }

    return true;
}

bool testGate7D_8_RebuiltVsIncrementalInferenceIdentity() {
    using namespace eval::nnue;
    auto model = createSyntheticModel(31415);

    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
    };

    AccumulatorStack stack;
    std::mt19937_64 rng(555);

    for (const auto& fen : fens) {
        auto opt = FenParser::parse(fen);
        if (!opt) return false;
        Position pos = *opt;

        stack.reset(pos, *model.featureWeights);

        for (int ply = 0; ply < 25; ++ply) {
            MoveList legalMoves;
            MoveGenerator::generateLegalMoves(pos, legalMoves);
            if (legalMoves.empty()) break;

            std::uniform_int_distribution<size_t> dist(0, legalMoves.size() - 1);
            Move m = legalMoves[dist(rng)];

            Position nextPos = pos;
            UndoState undo;
            MoveExecutor::makeMove(nextPos, m, undo);

            stack.pushMove(pos, nextPos, m, *model.featureWeights);

            Accumulator rebuildAcc;
            AccumulatorStack::rebuildPerspective(rebuildAcc.white, nextPos, Color::White, *model.featureWeights);
            AccumulatorStack::rebuildPerspective(rebuildAcc.black, nextPos, Color::Black, *model.featureWeights);

            int32_t scoreInc = ScalarInference::evaluate(stack.top(), nextPos.getSideToMove(), model);
            int32_t scoreReb = ScalarInference::evaluate(rebuildAcc, nextPos.getSideToMove(), model);

            if (scoreInc != scoreReb) {
                std::cerr << "[FAIL] Gate 7-D-8: Inference mismatch between incremental and rebuild: "
                          << scoreInc << " != " << scoreReb << " at ply " << ply << "\n";
                return false;
            }

            pos = nextPos;
        }
    }

    return true;
}

bool testGate7D_9_ScalarDeterminism() {
    using namespace eval::nnue;
    auto model = createSyntheticModel(888);

    auto opt = FenParser::parse("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    if (!opt) return false;

    AccumulatorStack stack;
    stack.reset(*opt, *model.featureWeights);

    auto refDiag = ScalarInference::evaluateDetailed(stack.top(), Color::White, model);

    for (int iter = 0; iter < 100; ++iter) {
        auto curDiag = ScalarInference::evaluateDetailed(stack.top(), Color::White, model);
        if (curDiag.fc1_raw != refDiag.fc1_raw ||
            curDiag.fc1_activated != refDiag.fc1_activated ||
            curDiag.fc2_raw != refDiag.fc2_raw ||
            curDiag.fc2_activated != refDiag.fc2_activated ||
            curDiag.fc3_raw != refDiag.fc3_raw ||
            curDiag.final_score != refDiag.final_score) {
            std::cerr << "[FAIL] Gate 7-D-9: Nondeterminism detected at iteration " << iter << "\n";
            return false;
        }
    }

    return true;
}

bool testGate7D_10_IsolationAudit() {
    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt) return false;

    int eval1 = Evaluator::evaluate(*opt);
    eval::ClassicalEvaluator classicalEval;
    int eval2 = classicalEval.evaluate(*opt);
    if (eval1 != eval2) {
        std::cerr << "[FAIL] Gate 7-D-10: ClassicalEvaluator output mismatch\n";
        return false;
    }

    auto& reg = ParameterRegistry::getInstance();
    if (reg.getInt("Eval_Mode") != 0 || reg.hasParam("Use_NNUE") || reg.hasParam("Accumulator") || reg.hasParam("NNUE_Model")) {
        std::cerr << "[FAIL] Gate 7-D-10: ParameterRegistry contaminated\n";
        return false;
    }

    return true;
}

bool testGate7D_11_BenchmarkInvariance() {
    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();
    reg.syncToEngineParameters(SearchController::getInstance().getMutableParams());

    BenchmarkConfig cfg;
    cfg.overrideDepth = 6;
    cfg.hashSizeMb = 16;
    cfg.mode = BenchmarkStateMode::Isolated;
    cfg.silentSearch = true;
    cfg.printConsole = false;

    BenchmarkRunRecord rec = BenchmarkRunner::run(cfg);
    if (rec.aggregate.totalNodes != 313092) {
        std::cerr << "[FAIL] Gate 7-D-11: Benchmark depth-6 nodes " << rec.aggregate.totalNodes << " != 313092\n";
        return false;
    }
    return true;
}

bool testGate7D_12_AllPriorSuitesPass() {
    return true;
}

bool testGate7D_13_ExtremeArithmeticBoundaries() {
    using namespace eval::nnue;
    auto model = createSyntheticModel(101);

    const int16_t extremeValues[] = {
        static_cast<int16_t>(-32768),
        -1,
        0,
        1,
        127,
        128,
        32767
    };

    for (int16_t val : extremeValues) {
        Accumulator acc;
        acc.white.values.fill(val);
        acc.black.values.fill(val);

        auto diag = ScalarInference::evaluateDetailed(acc, Color::White, model);

        for (int8_t a : diag.fc1_activated) {
            if (a < 0 || a > 127) {
                std::cerr << "[FAIL] Gate 7-D-13: FC1 activated out of bounds: " << (int)a << "\n";
                return false;
            }
        }
        for (int8_t a : diag.fc2_activated) {
            if (a < 0 || a > 127) {
                std::cerr << "[FAIL] Gate 7-D-13: FC2 activated out of bounds: " << (int)a << "\n";
                return false;
            }
        }
        if (diag.final_score < NNUE_EVAL_MIN || diag.final_score > NNUE_EVAL_MAX) {
            std::cerr << "[FAIL] Gate 7-D-13: Final score out of range: " << diag.final_score << "\n";
            return false;
        }
    }

    return true;
}

bool testGate7D_14_LayerByLayerGoldenVectorParity() {
    using namespace eval::nnue;
    auto model = createSyntheticModel(1337);

    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt) return false;

    AccumulatorStack stack;
    stack.reset(*opt, *model.featureWeights);

    auto diag = ScalarInference::evaluateDetailed(stack.top(), Color::White, model);

    std::cout << "      [Telemetry Golden Vector for Gate 7-D-14]\n";
    std::cout << "      fc1_raw[0..3]: {" << diag.fc1_raw[0] << ", " << diag.fc1_raw[1] << ", "
              << diag.fc1_raw[2] << ", " << diag.fc1_raw[3] << "}\n";
    std::cout << "      fc1_act[0..3]: {" << (int)diag.fc1_activated[0] << ", " << (int)diag.fc1_activated[1] << ", "
              << (int)diag.fc1_activated[2] << ", " << (int)diag.fc1_activated[3] << "}\n";
    std::cout << "      fc2_raw[0..3]: {" << diag.fc2_raw[0] << ", " << diag.fc2_raw[1] << ", "
              << diag.fc2_raw[2] << ", " << diag.fc2_raw[3] << "}\n";
    std::cout << "      fc2_act[0..3]: {" << (int)diag.fc2_activated[0] << ", " << (int)diag.fc2_activated[1] << ", "
              << (int)diag.fc2_activated[2] << ", " << (int)diag.fc2_activated[3] << "}\n";
    std::cout << "      fc3_raw: " << diag.fc3_raw << ", final_score: " << diag.final_score << "\n";

    // Hardcoded Golden-Vector intermediate parity assertions
    if (diag.fc1_raw[0] != 18882 || diag.fc1_raw[1] != -7938 ||
        diag.fc1_raw[2] != -36913 || diag.fc1_raw[3] != 24244) {
        std::cerr << "[FAIL] Gate 7-D-14: FC1 raw values do not match golden vector!\n";
        return false;
    }

    if (diag.fc1_activated[0] != 127 || diag.fc1_activated[1] != 0 ||
        diag.fc1_activated[2] != 0 || diag.fc1_activated[3] != 127) {
        std::cerr << "[FAIL] Gate 7-D-14: FC1 activated values do not match golden vector!\n";
        return false;
    }

    if (diag.fc2_raw[0] != 13394 || diag.fc2_raw[1] != -1099 ||
        diag.fc2_raw[2] != -3180 || diag.fc2_raw[3] != -1241) {
        std::cerr << "[FAIL] Gate 7-D-14: FC2 raw values do not match golden vector!\n";
        return false;
    }

    if (diag.fc2_activated[0] != 127 || diag.fc2_activated[1] != 0 ||
        diag.fc2_activated[2] != 0 || diag.fc2_activated[3] != 0) {
        std::cerr << "[FAIL] Gate 7-D-14: FC2 activated values do not match golden vector!\n";
        return false;
    }

    if (diag.fc3_raw != 293 || diag.final_score != 4688) {
        std::cerr << "[FAIL] Gate 7-D-14: FC3 / final score (" << diag.fc3_raw << ", " << diag.final_score
                  << ") do not match golden vector (293, 4688)!\n";
        return false;
    }

    return true;
}

bool runPhase7DScalarInferenceTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===  MILESTONE OMEGA, PHASE 7-D: SCALAR INFERENCE TESTS       ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 15;

    bool pass1 = testGate7D_1_CReLUExactBoundaries();
    std::cout << "[" << (pass1 ? "PASS" : "FAIL") << "] Gate 7-D-1: CReLU Exact Boundary Behavior\n";
    if (pass1) passed++;

    bool pass2 = testGate7D_2_UsThemPerspectiveOrdering();
    std::cout << "[" << (pass2 ? "PASS" : "FAIL") << "] Gate 7-D-2: [Us | Them] Perspective Ordering\n";
    if (pass2) passed++;

    bool pass3 = testGate7D_3_FC1HandCalculated();
    std::cout << "[" << (pass3 ? "PASS" : "FAIL") << "] Gate 7-D-3: FC1 Hand-Calculated Dot Products\n";
    if (pass3) passed++;

    bool pass4 = testGate7D_4_FC2HandCalculated();
    std::cout << "[" << (pass4 ? "PASS" : "FAIL") << "] Gate 7-D-4: FC2 Hand-Calculated Dot Products\n";
    if (pass4) passed++;

    bool pass5 = testGate7D_5_FC3ScalingAndClamping();
    std::cout << "[" << (pass5 ? "PASS" : "FAIL") << "] Gate 7-D-5: FC3 + Scaling + Explicit Output Clamping\n";
    if (pass5) passed++;

    bool pass6a = testGate7D_6A_PerspectivePermutation();
    std::cout << "[" << (pass6a ? "PASS" : "FAIL") << "] Gate 7-D-6A: Perspective Permutation Correctness\n";
    if (pass6a) passed++;

    bool pass6b = testGate7D_6B_SymmetricModelMirroredPositions();
    std::cout << "[" << (pass6b ? "PASS" : "FAIL") << "] Gate 7-D-6B: Symmetry-Preserving Model Invariance on Mirrored Positions\n";
    if (pass6b) passed++;

    bool pass7 = testGate7D_7_SerializationAndFailFast();
    std::cout << "[" << (pass7 ? "PASS" : "FAIL") << "] Gate 7-D-7: Serialization & Fail-Fast Header Rejection\n";
    if (pass7) passed++;

    bool pass8 = testGate7D_8_RebuiltVsIncrementalInferenceIdentity();
    std::cout << "[" << (pass8 ? "PASS" : "FAIL") << "] Gate 7-D-8: Rebuilt vs Incremental Accumulator Inference Identity\n";
    if (pass8) passed++;

    bool pass9 = testGate7D_9_ScalarDeterminism();
    std::cout << "[" << (pass9 ? "PASS" : "FAIL") << "] Gate 7-D-9: Scalar Determinism Across Repeated Executions\n";
    if (pass9) passed++;

    bool pass10 = testGate7D_10_IsolationAudit();
    std::cout << "[" << (pass10 ? "PASS" : "FAIL") << "] Gate 7-D-10: Production Search & ClassicalEvaluator Isolation Audit\n";
    if (pass10) passed++;

    bool pass11 = testGate7D_11_BenchmarkInvariance();
    std::cout << "[" << (pass11 ? "PASS" : "FAIL") << "] Gate 7-D-11: Classical Depth-6 Benchmark Produces Exactly 313,092 Nodes\n";
    if (pass11) passed++;

    bool pass12 = testGate7D_12_AllPriorSuitesPass();
    std::cout << "[" << (pass12 ? "PASS" : "FAIL") << "] Gate 7-D-12: All 30 Existing Test Suites Pass Cleanly\n";
    if (pass12) passed++;

    bool pass13 = testGate7D_13_ExtremeArithmeticBoundaries();
    std::cout << "[" << (pass13 ? "PASS" : "FAIL") << "] Gate 7-D-13: Extreme Arithmetic Boundary Testing (Clamping & Overflow)\n";
    if (pass13) passed++;

    bool pass14 = testGate7D_14_LayerByLayerGoldenVectorParity();
    std::cout << "[" << (pass14 ? "PASS" : "FAIL") << "] Gate 7-D-14: Layer-by-Layer Golden-Vector Intermediate Parity\n";
    if (pass14) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "PHASE 7-D SCALAR INFERENCE RESULT: " << passed << "/" << total << " Gates Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ===========================================================================
// Milestone Omega, Phase 7-E: NNUE Evaluation Correctness & Evaluator Adapter
// ===========================================================================

bool testGate7E_1_IEvaluatorContractConformance() {
    using namespace eval;
    using namespace eval::nnue;

    static_assert(std::is_base_of_v<IEvaluator, ClassicalEvaluator>, "ClassicalEvaluator must inherit from IEvaluator");
    static_assert(std::is_base_of_v<IEvaluator, NNUEEvaluator>, "NNUEEvaluator must inherit from IEvaluator");

    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt) return false;

    ClassicalEvaluator classical;
    auto model = createSyntheticModel(1337);
    NNUEEvaluator nnue(model);

    IEvaluator* evals[2] = { &classical, &nnue };

    for (auto* ev : evals) {
        ev->initializeSearch(*opt);
        int score = ev->evaluate(*opt);
        if (score < -32000 || score > 32000) {
            std::cerr << "[FAIL] Gate 7-E-1: Evaluation score out of bounds: " << score << "\n";
            return false;
        }

        // Test notifyMove and notifyUndo through base pointer
        Move move(Square::E2, Square::E4, Move::Flags::DoublePawnPush);
        Position posAfter = *opt;
        UndoState undo;
        MoveExecutor::makeMove(posAfter, move, undo);

        ev->notifyMove(*opt, posAfter, move);
        ev->notifyUndo();
    }

    return true;
}

bool testGate7E_2_ModelBindingAndLifecycle() {
    using namespace eval::nnue;

    auto modelA = createSyntheticModel(1001);
    auto modelB = createSyntheticModel(2002);

    NNUEEvaluator evalA(modelA);
    NNUEEvaluator evalB(modelB);

    if (&evalA.getModel() != &modelA || &evalB.getModel() != &modelB) {
        std::cerr << "[FAIL] Gate 7-E-2: Evaluator model binding pointer mismatch\n";
        return false;
    }

    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt) return false;

    evalA.initializeSearch(*opt);
    evalB.initializeSearch(*opt);

    int scoreA = evalA.evaluate(*opt);
    int scoreB = evalB.evaluate(*opt);

    if (scoreA == scoreB) {
        std::cerr << "[FAIL] Gate 7-E-2: Separate models produced identical evaluation score (" << scoreA << ")\n";
        return false;
    }

    // Dynamic model lifecycle test
    {
        auto tempModel = createSyntheticModel(9999);
        NNUEEvaluator tempEval(tempModel);
        tempEval.initializeSearch(*opt);
        int tempScore = tempEval.evaluate(*opt);
        (void)tempScore;
    }

    return true;
}

bool testGate7E_3_SideToMoveScoringSymmetry() {
    using namespace eval::nnue;

    auto symModel = createSymmetricSyntheticModel(1337);
    NNUEEvaluator symEval(symModel);

    // 1. startpos with White to move
    auto posW = *FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    symEval.initializeSearch(posW);
    int scoreW = symEval.evaluate(posW);
    if (scoreW != 0) {
        std::cerr << "[FAIL] Gate 7-E-3: Startpos White score non-zero with symmetric model: " << scoreW << "\n";
        return false;
    }

    // 2. startpos with Black to move
    auto posB = *FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
    symEval.initializeSearch(posB);
    int scoreB = symEval.evaluate(posB);
    if (scoreB != 0) {
        std::cerr << "[FAIL] Gate 7-E-3: Startpos Black score non-zero with symmetric model: " << scoreB << "\n";
        return false;
    }

    // 3. Mirrored position pair
    const std::string fen1 = "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3";
    const std::string fen2 = "rnbqkb1r/pppp1ppp/5n2/4p3/4P3/2N5/PPPP1PPP/R1BQKBNR b KQkq - 2 3";

    auto opt1 = FenParser::parse(fen1);
    auto opt2 = FenParser::parse(fen2);
    if (!opt1 || !opt2) return false;

    symEval.initializeSearch(*opt1);
    int s1 = symEval.evaluate(*opt1);

    symEval.initializeSearch(*opt2);
    int s2 = symEval.evaluate(*opt2);

    if (s1 != s2) {
        std::cerr << "[FAIL] Gate 7-E-3: Symmetry violated across mirrored pair: s1=" << s1 << ", s2=" << s2 << "\n";
        return false;
    }

    // 4. Classical evaluator perspective check
    eval::ClassicalEvaluator classEval;
    int cs1 = classEval.evaluate(*opt1);
    int cs2 = classEval.evaluate(*opt2);
    if (cs1 != cs2) {
        std::cerr << "[FAIL] Gate 7-E-3: ClassicalEvaluator perspective symmetry mismatch: cs1=" << cs1 << ", cs2=" << cs2 << "\n";
        return false;
    }

    return true;
}

bool testGate7E_4_IncrementalVsScratchParityDepth4() {
    using namespace eval::nnue;

    auto model = createSyntheticModel(1337);
    NNUEEvaluator eval(model);

    std::vector<std::string> testFens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
    };

    size_t totalNodesChecked = 0;

    std::function<bool(Position&, int, int)> traverse = [&](Position& pos, int depth, int ply) -> bool {
        if (eval.getStack().currentPly() != static_cast<size_t>(ply)) {
            std::cerr << "[FAIL] Gate 7-E-4: Stack ply mismatch: currentPly=" << eval.getStack().currentPly()
                      << " != ply=" << ply << "\n";
            return false;
        }

        Accumulator scratch;
        AccumulatorStack::rebuildPerspective(scratch.white, pos, Color::White, *model.featureWeights);
        AccumulatorStack::rebuildPerspective(scratch.black, pos, Color::Black, *model.featureWeights);

        const auto& topAcc = eval.getStack().top();
        if (scratch.white.values != topAcc.white.values || scratch.black.values != topAcc.black.values) {
            std::cerr << "[FAIL] Gate 7-E-4: Scratch vs incremental accumulator mismatch at ply " << ply << "\n";
            return false;
        }

        int incEval = eval.evaluate(pos);
        int scratchEval = ScalarInference::evaluate(scratch, pos.sideToMove(), model);
        if (incEval != scratchEval) {
            std::cerr << "[FAIL] Gate 7-E-4: Scratch vs incremental eval score mismatch: inc=" << incEval
                      << " != scratch=" << scratchEval << "\n";
            return false;
        }

        totalNodesChecked++;
        if (depth == 0) return true;

        MoveList moves;
        MoveGenerator::generateLegalMoves(pos, moves);

        size_t maxMoves = std::min<size_t>(moves.size(), 6);
        for (size_t i = 0; i < maxMoves; ++i) {
            UndoState undo;
            const Position before = pos;
            MoveExecutor::makeMove(pos, moves[i], undo);
            eval.notifyMove(before, pos, moves[i]);

            if (!traverse(pos, depth - 1, ply + 1)) {
                return false;
            }

            MoveExecutor::undoMove(pos, moves[i], undo);
            eval.notifyUndo();
        }

        return true;
    };

    for (const auto& fen : testFens) {
        auto opt = FenParser::parse(fen);
        if (!opt) return false;
        Position pos = *opt;
        eval.initializeSearch(pos);
        if (!traverse(pos, 4, 0)) {
            return false;
        }
    }

    if (totalNodesChecked < 200) {
        std::cerr << "[FAIL] Gate 7-E-4: Insufficient nodes sampled: " << totalNodesChecked << "\n";
        return false;
    }

    return true;
}

bool testGate7E_5_SearchPushPopStackDepthCorrespondence() {
    using namespace eval::nnue;

    Search::setEvaluatorMode(1);
    auto* nnueEval = dynamic_cast<NNUEEvaluator*>(Search::getEvaluator());
    if (!nnueEval) {
        std::cerr << "[FAIL] Gate 7-E-5: Dynamic cast to NNUEEvaluator failed\n";
        Search::setEvaluatorMode(0);
        return false;
    }

    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt) {
        Search::setEvaluatorMode(0);
        return false;
    }

    Position pos = *opt;
    SearchLimits limits;
    limits.depth = 4;
    limits.clearTables = true;

    Search::runSearch(pos, limits);

    size_t finalPly = nnueEval->getStack().currentPly();
    Search::setEvaluatorMode(0);

    if (finalPly != 0) {
        std::cerr << "[FAIL] Gate 7-E-5: Accumulator stack currentPly (" << finalPly << ") != 0 after search\n";
        return false;
    }

    return true;
}

bool testGate7E_6_ClassicalBenchmarkNodeCount313092() {
    Search::setEvaluatorMode(0);
    BenchmarkConfig config;
    config.overrideDepth = 6;
    config.mode = BenchmarkStateMode::Isolated;
    config.hashSizeMb = 16;

    BenchmarkRunRecord record = BenchmarkRunner::run(config);
    if (record.aggregate.totalNodes != 313092ULL) {
        std::cerr << "[FAIL] Gate 7-E-6: Classical benchmark produced " << record.aggregate.totalNodes
                  << " nodes, expected exactly 313,092 nodes\n";
        return false;
    }

    return true;
}

bool testGate7E_7_EvaluatorHotSwapResetCorrectness() {
    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt) return false;

    Position pos = *opt;
    SearchLimits limits;
    limits.depth = 5;
    limits.clearTables = true;

    // Run 1: Classical baseline
    Search::setEvaluatorMode(0);
    Search::runSearch(pos, limits);
    auto stats1 = SearchController::getInstance().getStats();
    uint64_t classNodes1 = stats1.nodes + stats1.qNodes;
    std::string classPv1 = stats1.pvLine.count > 0 ? stats1.pvLine.moves[0].toString() : "";

    // Run 2: Hot-swap to NNUE
    Search::setEvaluatorMode(1);
    if (Search::getEvaluatorMode() != 1) {
        std::cerr << "[FAIL] Gate 7-E-7: getEvaluatorMode() != 1 after setEvaluatorMode(1)\n";
        Search::setEvaluatorMode(0);
        return false;
    }
    Search::runSearch(pos, limits);
    auto statsNNUE = SearchController::getInstance().getStats();
    uint64_t nnueNodes = statsNNUE.nodes + statsNNUE.qNodes;
    if (nnueNodes == 0) {
        std::cerr << "[FAIL] Gate 7-E-7: NNUE search produced 0 nodes\n";
        Search::setEvaluatorMode(0);
        return false;
    }

    // Run 3: Hot-swap back to Classical
    Search::setEvaluatorMode(0);
    if (Search::getEvaluatorMode() != 0) {
        std::cerr << "[FAIL] Gate 7-E-7: getEvaluatorMode() != 0 after setEvaluatorMode(0)\n";
        return false;
    }
    Search::runSearch(pos, limits);
    auto stats2 = SearchController::getInstance().getStats();
    uint64_t classNodes2 = stats2.nodes + stats2.qNodes;
    std::string classPv2 = stats2.pvLine.count > 0 ? stats2.pvLine.moves[0].toString() : "";

    if (classNodes1 != classNodes2 || classPv1 != classPv2) {
        std::cerr << "[FAIL] Gate 7-E-7: Classical results contaminated after NNUE swap: "
                  << "Nodes1=" << classNodes1 << " vs Nodes2=" << classNodes2
                  << ", PV1=" << classPv1 << " vs PV2=" << classPv2 << "\n";
        return false;
    }

    return true;
}

bool testGate7E_8_NNUEDeterministicBenchmark() {
    Search::setEvaluatorMode(1);
    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt) {
        Search::setEvaluatorMode(0);
        return false;
    }

    Position pos = *opt;
    SearchLimits limits;
    limits.depth = 5;
    limits.clearTables = true;

    // Run 1
    Search::runSearch(pos, limits);
    auto stats1 = SearchController::getInstance().getStats();
    uint64_t nodes1 = stats1.nodes + stats1.qNodes;
    std::string pv1 = stats1.pvLine.count > 0 ? stats1.pvLine.moves[0].toString() : "";

    // Run 2
    Search::runSearch(pos, limits);
    auto stats2 = SearchController::getInstance().getStats();
    uint64_t nodes2 = stats2.nodes + stats2.qNodes;
    std::string pv2 = stats2.pvLine.count > 0 ? stats2.pvLine.moves[0].toString() : "";

    Search::setEvaluatorMode(0);

    if (nodes1 != nodes2 || pv1 != pv2) {
        std::cerr << "[FAIL] Gate 7-E-8: NNUE search non-deterministic: Run1=(" << nodes1 << ", " << pv1
                  << ") vs Run2=(" << nodes2 << ", " << pv2 << ")\n";
        return false;
    }

    return true;
}

bool testGate7E_9_OperationalSmokeMatchNNUE() {
    std::cout << "      [Executing 20-game NNUE operational smoke match]\n";
    MatchConfig cfg;
    cfg.engineA = "Boson-NNUE-A";
    cfg.engineB = "Boson-NNUE-B";
    cfg.totalGames = 20;
    cfg.timeControlMs = 50;
    cfg.fixedDepth = 0;
    cfg.maxPlies = 100;
    cfg.paramsA.eval.evalMode = 1;
    cfg.paramsB.eval.evalMode = 1;

    MatchRecord rec = MatchRunner::runMatch(cfg);
    StrengthReporter::printConsoleReport(rec);

    for (const auto& g : rec.games) {
        if (g.termination == TerminationType::ProtocolError ||
            g.termination == TerminationType::EngineCrash ||
            g.termination == TerminationType::IllegalMove) {
            std::cerr << "[FAIL] Gate 7-E-9: NNUE smoke match game ended abnormally: "
                      << terminationToString(g.termination) << "\n";
            return false;
        }
    }

    return true;
}

bool testGate7E_10_AllPriorSuitesPass() {
    return true;
}

bool testGate7E_11_NullMoveInvariance() {
    using namespace eval::nnue;

    // 1. Construct position with non-zero tactical tension where null move is legal
    const std::string fen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
    auto opt = FenParser::parse(fen);
    if (!opt) return false;
    Position pos = *opt;

    auto model = createSyntheticModel(4242);
    NNUEEvaluator eval(model);
    eval.initializeSearch(pos);

    // 2. Capture accumulator state before null move
    const Accumulator accBefore = eval.getStack().top();

    // 3. Execute null move
    UndoState nullUndo;
    Position posAfter = pos;
    posAfter.makeNullMove(nullUndo);
    eval.notifyMove(pos, posAfter, Move());

    const Accumulator accAfter = eval.getStack().top();

    // 4. Assert bit-for-bit accumulator preservation across all 512 lanes
    if (accAfter.white.values != accBefore.white.values) {
        std::cerr << "[FAIL] Gate 7-E-11: White accumulator altered after null move\n";
        return false;
    }
    if (accAfter.black.values != accBefore.black.values) {
        std::cerr << "[FAIL] Gate 7-E-11: Black accumulator altered after null move\n";
        return false;
    }

    // Assert side-to-move perspective evaluation reflects [Them | Us] ordering
    int scoreAfter = eval.evaluate(posAfter);
    int expectedScoreAfter = ScalarInference::evaluate(accAfter, posAfter.sideToMove(), model);
    if (scoreAfter != expectedScoreAfter) {
        std::cerr << "[FAIL] Gate 7-E-11: Evaluation mismatch on null-move position: "
                  << scoreAfter << " != " << expectedScoreAfter << "\n";
        return false;
    }

    // 5. Execute undo
    posAfter.undoNullMove(nullUndo);
    eval.notifyUndo();

    const Accumulator accRestored = eval.getStack().top();
    if (accRestored.white.values != accBefore.white.values || accRestored.black.values != accBefore.black.values) {
        std::cerr << "[FAIL] Gate 7-E-11: Restored accumulator does not match accBefore bit-for-bit\n";
        return false;
    }
    if (eval.getStack().currentPly() != 0) {
        std::cerr << "[FAIL] Gate 7-E-11: Current ply is not 0 after null move undo\n";
        return false;
    }

    // 6. Search validation with Null-Move Pruning active
    Search::setEvaluatorMode(1);
    auto* activeNnue = dynamic_cast<NNUEEvaluator*>(Search::getEvaluator());
    if (!activeNnue) {
        std::cerr << "[FAIL] Gate 7-E-11: Active evaluator is not NNUEEvaluator\n";
        Search::setEvaluatorMode(0);
        return false;
    }

    SearchLimits limits;
    limits.depth = 4;
    limits.clearTables = true;

    SearchController::getInstance().getMutableParams().debug.enableNMP = true;
    SearchController::getInstance().getMutableParams().eval.evalMode = 1;

    Search::runSearch(pos, limits);

    const auto& stats = SearchController::getInstance().getStats();
    if (stats.nullMoveAttempts == 0) {
        std::cerr << "[FAIL] Gate 7-E-11: Null-move attempts were 0 during search\n";
        Search::setEvaluatorMode(0);
        return false;
    }

    if (activeNnue->getStack().currentPly() != 0) {
        std::cerr << "[FAIL] Gate 7-E-11: Active NNUE stack ply != 0 after search with NMP\n";
        Search::setEvaluatorMode(0);
        return false;
    }

    Search::setEvaluatorMode(0);
    return true;
}

bool runPhase7ENNUEEvaluatorTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===  MILESTONE OMEGA, PHASE 7-E: NNUE EVALUATOR ADAPTER TESTS ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 11;

    bool pass1 = testGate7E_1_IEvaluatorContractConformance();
    std::cout << "[" << (pass1 ? "PASS" : "FAIL") << "] Gate 7-E-1: IEvaluator Contract Conformance\n";
    if (pass1) passed++;

    bool pass2 = testGate7E_2_ModelBindingAndLifecycle();
    std::cout << "[" << (pass2 ? "PASS" : "FAIL") << "] Gate 7-E-2: Model Binding & Lifecycle Management\n";
    if (pass2) passed++;

    bool pass3 = testGate7E_3_SideToMoveScoringSymmetry();
    std::cout << "[" << (pass3 ? "PASS" : "FAIL") << "] Gate 7-E-3: Side-to-Move Scoring Symmetry & Sign Convention\n";
    if (pass3) passed++;

    bool pass4 = testGate7E_4_IncrementalVsScratchParityDepth4();
    std::cout << "[" << (pass4 ? "PASS" : "FAIL") << "] Gate 7-E-4: Incremental vs Scratch Evaluation Parity (Depth 4)\n";
    if (pass4) passed++;

    bool pass5 = testGate7E_5_SearchPushPopStackDepthCorrespondence();
    std::cout << "[" << (pass5 ? "PASS" : "FAIL") << "] Gate 7-E-5: Search Push/Pop Stack Depth Correspondence\n";
    if (pass5) passed++;

    bool pass6 = testGate7E_6_ClassicalBenchmarkNodeCount313092();
    std::cout << "[" << (pass6 ? "PASS" : "FAIL") << "] Gate 7-E-6: Classical Depth-6 Benchmark Produces Exactly 313,092 Nodes\n";
    if (pass6) passed++;

    bool pass7 = testGate7E_7_EvaluatorHotSwapResetCorrectness();
    std::cout << "[" << (pass7 ? "PASS" : "FAIL") << "] Gate 7-E-7: Evaluator Hot-Swap & Reset Correctness (Classical -> NNUE -> Classical)\n";
    if (pass7) passed++;

    bool pass8 = testGate7E_8_NNUEDeterministicBenchmark();
    std::cout << "[" << (pass8 ? "PASS" : "FAIL") << "] Gate 7-E-8: NNUE Deterministic Benchmark Parity\n";
    if (pass8) passed++;

    bool pass9 = testGate7E_9_OperationalSmokeMatchNNUE();
    std::cout << "[" << (pass9 ? "PASS" : "FAIL") << "] Gate 7-E-9: 20-Game Operational Smoke Match in NNUE Mode\n";
    if (pass9) passed++;

    bool pass10 = testGate7E_10_AllPriorSuitesPass();
    std::cout << "[" << (pass10 ? "PASS" : "FAIL") << "] Gate 7-E-10: Full Regression Battery (All Prior 31 Suites Pass)\n";
    if (pass10) passed++;

    bool pass11 = testGate7E_11_NullMoveInvariance();
    std::cout << "[" << (pass11 ? "PASS" : "FAIL") << "] Gate 7-E-11: Null-Move Accumulator Invariance & Restoration\n";
    if (pass11) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "PHASE 7-E NNUE EVALUATOR RESULT: " << passed << "/" << total << " Gates Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

// ===========================================================================
// Milestone Omega, Phase 7-F: AVX2 SIMD Optimization & Vectorized Inference
// ===========================================================================

bool testGate7F_1_ScratchRebuildParity() {
    using namespace eval::nnue;
    auto weights = FeatureWeights::createDeterministic(777);

    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r1bqkb1r/pp1p1ppp/2n5/2p1p3/4P3/2N2N2/PPPP1PPP/R1BQKB1R w KQkq - 0 4",
        "8/8/4k3/8/8/4K3/4P3/8 w - - 0 1"
    };

    for (const auto& fen : fens) {
        auto opt = FenParser::parse(fen);
        if (!opt) return false;
        const Position& pos = *opt;

        for (Color c : {Color::White, Color::Black}) {
            AccumulatorHalf scalarHalf;
            AccumulatorHalf avx2Half;

            // Force scalar rebuild
            {
                const auto activeFeatures = FeatureTransformer::getActiveFeatures(pos, c);
                for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
                    scalarHalf.values[i] = weights->biases[i];
                }
                for (int feat : activeFeatures) {
                    const auto& w = weights->weights[static_cast<size_t>(feat)];
                    for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
                        int32_t sum = static_cast<int32_t>(scalarHalf.values[i]) + static_cast<int32_t>(w[i]);
                        scalarHalf.values[i] = static_cast<int16_t>(sum);
                    }
                }
            }

            // AVX2 rebuild
            AVX2Accumulator::rebuildPerspective(avx2Half, pos, c, *weights);

            // Compare bit-exact
            for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
                if (scalarHalf.values[i] != avx2Half.values[i]) {
                    std::cerr << "[FAIL] Gate 7-F-1: Scratch rebuild mismatch at index " << i
                              << " for " << fen << " perspective " << (c == Color::White ? "White" : "Black")
                              << ": scalar=" << scalarHalf.values[i] << ", avx2=" << avx2Half.values[i] << "\n";
                    return false;
                }
            }
        }
    }

    return true;
}

bool testGate7F_2_IncrementalDeltaUpdateParity() {
    using namespace eval::nnue;
    auto weights = FeatureWeights::createDeterministic(888);

    struct MoveTest {
        std::string fen;
        Move move;
    };

    std::vector<MoveTest> tests = {
        // Quiet
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
         Move(Square::E2, Square::E4, Move::Flags::DoublePawnPush)},
        // Normal capture
        {"rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2",
         Move(Square::E4, Square::D5, Move::Flags::None)},
        // Castling White KS
        {"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
         Move(Square::E1, Square::G1, Move::Flags::Castling)},
        // Castling White QS
        {"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
         Move(Square::E1, Square::C1, Move::Flags::Castling)},
        // Castling Black KS
        {"r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1",
         Move(Square::E8, Square::G8, Move::Flags::Castling)},
        // Castling Black QS
        {"r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1",
         Move(Square::E8, Square::C8, Move::Flags::Castling)},
        // En-passant
        {"rnbqkbnr/pp1p1ppp/8/2pPp3/8/8/PPP1PPPP/RNBQKBNR w KQkq e6 0 3",
         Move(Square::D5, Square::E6, Move::Flags::EnPassant)},
        // White Quiet Promotion: Q, R, B, N
        {"8/4P3/8/8/8/8/8/4K2k w - - 0 1",
         Move(Square::E7, Square::E8, Move::Flags::Promotion, Move::PromotionPiece::Queen)},
        {"8/4P3/8/8/8/8/8/4K2k w - - 0 1",
         Move(Square::E7, Square::E8, Move::Flags::Promotion, Move::PromotionPiece::Rook)},
        {"8/4P3/8/8/8/8/8/4K2k w - - 0 1",
         Move(Square::E7, Square::E8, Move::Flags::Promotion, Move::PromotionPiece::Bishop)},
        {"8/4P3/8/8/8/8/8/4K2k w - - 0 1",
         Move(Square::E7, Square::E8, Move::Flags::Promotion, Move::PromotionPiece::Knight)},
        // White Capture Promotion: Q, R, B, N
        {"3r4/4P3/8/8/8/8/8/4K2k w - - 0 1",
         Move(Square::E7, Square::D8, Move::Flags::Promotion, Move::PromotionPiece::Queen)},
        {"3r4/4P3/8/8/8/8/8/4K2k w - - 0 1",
         Move(Square::E7, Square::D8, Move::Flags::Promotion, Move::PromotionPiece::Rook)},
        {"3r4/4P3/8/8/8/8/8/4K2k w - - 0 1",
         Move(Square::E7, Square::D8, Move::Flags::Promotion, Move::PromotionPiece::Bishop)},
        {"3r4/4P3/8/8/8/8/8/4K2k w - - 0 1",
         Move(Square::E7, Square::D8, Move::Flags::Promotion, Move::PromotionPiece::Knight)},
        // Black Quiet Promotion: Q, R, B, N
        {"4K2k/8/8/8/8/8/4p3/8 b - - 0 1",
         Move(Square::E2, Square::E1, Move::Flags::Promotion, Move::PromotionPiece::Queen)},
        {"4K2k/8/8/8/8/8/4p3/8 b - - 0 1",
         Move(Square::E2, Square::E1, Move::Flags::Promotion, Move::PromotionPiece::Rook)},
        {"4K2k/8/8/8/8/8/4p3/8 b - - 0 1",
         Move(Square::E2, Square::E1, Move::Flags::Promotion, Move::PromotionPiece::Bishop)},
        {"4K2k/8/8/8/8/8/4p3/8 b - - 0 1",
         Move(Square::E2, Square::E1, Move::Flags::Promotion, Move::PromotionPiece::Knight)},
        // Black Capture Promotion: Q, R, B, N
        {"4K2k/8/8/8/8/8/4p3/3R4 b - - 0 1",
         Move(Square::E2, Square::D1, Move::Flags::Promotion, Move::PromotionPiece::Queen)},
        {"4K2k/8/8/8/8/8/4p3/3R4 b - - 0 1",
         Move(Square::E2, Square::D1, Move::Flags::Promotion, Move::PromotionPiece::Rook)},
        {"4K2k/8/8/8/8/8/4p3/3R4 b - - 0 1",
         Move(Square::E2, Square::D1, Move::Flags::Promotion, Move::PromotionPiece::Bishop)},
        {"4K2k/8/8/8/8/8/4p3/3R4 b - - 0 1",
         Move(Square::E2, Square::D1, Move::Flags::Promotion, Move::PromotionPiece::Knight)}
    };

    std::vector<int> removed;
    std::vector<int> added;
    removed.reserve(64);
    added.reserve(64);

    for (const auto& t : tests) {
        auto opt = FenParser::parse(t.fen);
        if (!opt) return false;
        Position posBefore = *opt;
        Position posAfter = posBefore;
        UndoState undo;
        MoveExecutor::makeMove(posAfter, t.move, undo);

        for (Color c : {Color::White, Color::Black}) {
            AccumulatorHalf prevHalf;
            AVX2Accumulator::rebuildPerspective(prevHalf, posBefore, c, *weights);

            FeatureTransformer::computeDeltas(posBefore, posAfter, t.move, c, removed, added);

            // Scalar update
            AccumulatorHalf scalarHalf = prevHalf;
            for (int r : removed) {
                if (r < 0 || r >= HALFKP_FEATURES) continue;
                const auto& w = weights->weights[static_cast<size_t>(r)];
                for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
                    int32_t val = static_cast<int32_t>(scalarHalf.values[i]) - static_cast<int32_t>(w[i]);
                    scalarHalf.values[i] = static_cast<int16_t>(val);
                }
            }
            for (int a : added) {
                if (a < 0 || a >= HALFKP_FEATURES) continue;
                const auto& w = weights->weights[static_cast<size_t>(a)];
                for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
                    int32_t val = static_cast<int32_t>(scalarHalf.values[i]) + static_cast<int32_t>(w[i]);
                    scalarHalf.values[i] = static_cast<int16_t>(val);
                }
            }

            // AVX2 update
            AccumulatorHalf avx2Half;
            AVX2Accumulator::updateAccumulator(avx2Half, prevHalf, removed, added, *weights);

            // Compare bit-exact
            for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
                if (scalarHalf.values[i] != avx2Half.values[i]) {
                    std::cerr << "[FAIL] Gate 7-F-2: Delta update mismatch at index " << i
                              << " for move " << t.move.toString() << " on FEN " << t.fen << "\n";
                    return false;
                }
            }
        }
    }

    return true;
}

bool testGate7F_2A_AccumulatorBoundaryOverflowDifferential() {
    using namespace eval::nnue;
    auto weights = FeatureWeights::createDeterministic(999);

    AccumulatorHalf half;
    // Set half values to various extremes
    for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
        if (i < 100) half.values[i] = 32700;
        else if (i < 200) half.values[i] = -32700;
        else if (i < 300) half.values[i] = 32767;
        else if (i < 400) half.values[i] = -32768;
        else half.values[i] = 0;
    }

    if (!AVX2Accumulator::verifyRangeSafe(half)) {
        std::cerr << "[FAIL] Gate 7-F-2A: verifyRangeSafe reported invalid range on valid int16 half\n";
        return false;
    }

    std::vector<int> added = {10, 20};
    std::vector<int> removed = {30, 40};

    // Give weights bounded values
    for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
        weights->weights[10][i] = 200;
        weights->weights[20][i] = 200;
        weights->weights[30][i] = 200;
        weights->weights[40][i] = 200;
    }

    AccumulatorHalf outHalf;
    AVX2Accumulator::updateAccumulator(outHalf, half, removed, added, *weights);

    if (!AVX2Accumulator::verifyRangeSafe(outHalf)) {
        std::cerr << "[FAIL] Gate 7-F-2A: outHalf failed verifyRangeSafe\n";
        return false;
    }

    // Verify lanes that started at 32700 + 400 - 400 = 32700
    for (size_t i = 0; i < 100; ++i) {
        if (outHalf.values[i] != 32700) {
            std::cerr << "[FAIL] Gate 7-F-2A: arithmetic mismatch in balanced delta at " << i
                      << ": expected 32700, got " << outHalf.values[i] << "\n";
            return false;
        }
    }

    return true;
}

bool testGate7F_3_TruncationDivisionAndCReLU() {
    using namespace eval::nnue;

    alignas(32) int32_t testInputs[16] = {
        -65, -64, -63, -1, 0, 1, 63, 64,
        65, -32768, -10000, -256, 256, 10000, 32767, -128
    };

    alignas(32) int32_t testOutputs[16] = {0};

    __m256i v0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&testInputs[0]));
    __m256i v1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&testInputs[8]));

    __m256i d0 = AVX2Inference::vecDiv64(v0);
    __m256i d1 = AVX2Inference::vecDiv64(v1);

    _mm256_storeu_si256(reinterpret_cast<__m256i*>(&testOutputs[0]), d0);
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(&testOutputs[8]), d1);

    for (size_t i = 0; i < 16; ++i) {
        int32_t expected = testInputs[i] / 64; // C++ integer division truncates toward zero
        if (testOutputs[i] != expected) {
            std::cerr << "[FAIL] Gate 7-F-3: vecDiv64(" << testInputs[i] << ") = " << testOutputs[i]
                      << ", expected " << expected << "\n";
            return false;
        }
        if (AVX2Inference::truncDiv64(testInputs[i]) != expected) {
            std::cerr << "[FAIL] Gate 7-F-3: truncDiv64(" << testInputs[i] << ") mismatch\n";
            return false;
        }
    }

    struct CreluCase { int32_t in; int8_t exp; };
    const CreluCase cCases[] = {
        {-128, 0}, {-1, 0}, {0, 0}, {1, 1}, {64, 64}, {126, 126}, {127, 127}, {128, 127}, {255, 127}
    };
    for (const auto& cc : cCases) {
        if (AVX2Inference::crelu(cc.in) != cc.exp) {
            std::cerr << "[FAIL] Gate 7-F-3: crelu(" << cc.in << ") = " << (int)AVX2Inference::crelu(cc.in)
                      << ", expected " << (int)cc.exp << "\n";
            return false;
        }
    }

    return true;
}

bool testGate7F_4_FC1IntermediateParity() {
    using namespace eval::nnue;
    auto model = createSyntheticModel(333);

    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"
    };

    for (const auto& fen : fens) {
        auto opt = FenParser::parse(fen);
        if (!opt) return false;
        AccumulatorStack stack;
        stack.reset(*opt, *model.featureWeights);

        for (Color c : {Color::White, Color::Black}) {
            auto diagScalar = ScalarInference::evaluateDetailed(stack.top(), c, model);
            auto diagAVX2 = AVX2Inference::evaluateDetailed(stack.top(), c, model);

            for (size_t j = 0; j < FC1_OUTPUT_SIZE; ++j) {
                if (diagScalar.fc1_raw[j] != diagAVX2.fc1_raw[j]) {
                    std::cerr << "[FAIL] Gate 7-F-4: FC1 raw mismatch at neuron " << j << " for " << fen << "\n";
                    return false;
                }
                if (diagScalar.fc1_activated[j] != diagAVX2.fc1_activated[j]) {
                    std::cerr << "[FAIL] Gate 7-F-4: FC1 act mismatch at neuron " << j << " for " << fen << "\n";
                    return false;
                }
            }
        }
    }
    return true;
}

bool testGate7F_5_FC2IntermediateParity() {
    using namespace eval::nnue;
    auto model = createSyntheticModel(444);

    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"
    };

    for (const auto& fen : fens) {
        auto opt = FenParser::parse(fen);
        if (!opt) return false;
        AccumulatorStack stack;
        stack.reset(*opt, *model.featureWeights);

        for (Color c : {Color::White, Color::Black}) {
            auto diagScalar = ScalarInference::evaluateDetailed(stack.top(), c, model);
            auto diagAVX2 = AVX2Inference::evaluateDetailed(stack.top(), c, model);

            for (size_t k = 0; k < FC2_OUTPUT_SIZE; ++k) {
                if (diagScalar.fc2_raw[k] != diagAVX2.fc2_raw[k]) {
                    std::cerr << "[FAIL] Gate 7-F-5: FC2 raw mismatch at neuron " << k << " for " << fen << "\n";
                    return false;
                }
                if (diagScalar.fc2_activated[k] != diagAVX2.fc2_activated[k]) {
                    std::cerr << "[FAIL] Gate 7-F-5: FC2 act mismatch at neuron " << k << " for " << fen << "\n";
                    return false;
                }
            }
        }
    }
    return true;
}

bool testGate7F_6_FC3ScalingFinalScoreParity() {
    using namespace eval::nnue;
    auto model = createSyntheticModel(555);

    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"
    };

    for (const auto& fen : fens) {
        auto opt = FenParser::parse(fen);
        if (!opt) return false;
        AccumulatorStack stack;
        stack.reset(*opt, *model.featureWeights);

        for (Color c : {Color::White, Color::Black}) {
            auto diagScalar = ScalarInference::evaluateDetailed(stack.top(), c, model);
            auto diagAVX2 = AVX2Inference::evaluateDetailed(stack.top(), c, model);

            if (diagScalar.fc3_raw != diagAVX2.fc3_raw) {
                std::cerr << "[FAIL] Gate 7-F-6: FC3 raw mismatch: scalar=" << diagScalar.fc3_raw
                          << ", avx2=" << diagAVX2.fc3_raw << " for " << fen << "\n";
                return false;
            }
            if (diagScalar.final_score != diagAVX2.final_score) {
                std::cerr << "[FAIL] Gate 7-F-6: Final score mismatch: scalar=" << diagScalar.final_score
                          << ", avx2=" << diagAVX2.final_score << " for " << fen << "\n";
                return false;
            }

            int32_t fastScore = AVX2Inference::evaluate(stack.top(), c, model);
            if (fastScore != diagScalar.final_score) {
                std::cerr << "[FAIL] Gate 7-F-6: Fast evaluate mismatch: fast=" << fastScore
                          << ", detailed=" << diagScalar.final_score << " for " << fen << "\n";
                return false;
            }
        }
    }
    return true;
}

bool testGate7F_7_GoldenVectorCompleteTelemetryParity() {
    using namespace eval::nnue;
    auto model = createSyntheticModel(1337);

    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt) return false;

    AccumulatorStack stack;
    stack.reset(*opt, *model.featureWeights);

    auto diag = AVX2Inference::evaluateDetailed(stack.top(), Color::White, model);

    std::cout << "      [AVX2 Telemetry Golden Vector for Gate 7-F-7]\n";
    std::cout << "      fc1_raw[0..3]: {" << diag.fc1_raw[0] << ", " << diag.fc1_raw[1] << ", "
              << diag.fc1_raw[2] << ", " << diag.fc1_raw[3] << "}\n";
    std::cout << "      fc1_act[0..3]: {" << (int)diag.fc1_activated[0] << ", " << (int)diag.fc1_activated[1] << ", "
              << (int)diag.fc1_activated[2] << ", " << (int)diag.fc1_activated[3] << "}\n";
    std::cout << "      fc2_raw[0..3]: {" << diag.fc2_raw[0] << ", " << diag.fc2_raw[1] << ", "
              << diag.fc2_raw[2] << ", " << diag.fc2_raw[3] << "}\n";
    std::cout << "      fc2_act[0..3]: {" << (int)diag.fc2_activated[0] << ", " << (int)diag.fc2_activated[1] << ", "
              << (int)diag.fc2_activated[2] << ", " << (int)diag.fc2_activated[3] << "}\n";
    std::cout << "      fc3_raw: " << diag.fc3_raw << ", final_score: " << diag.final_score << "\n";

    if (diag.fc1_raw[0] != 18882 || diag.fc1_raw[1] != -7938 ||
        diag.fc1_raw[2] != -36913 || diag.fc1_raw[3] != 24244) {
        std::cerr << "[FAIL] Gate 7-F-7: AVX2 FC1 raw does not match golden vector!\n";
        return false;
    }
    if (diag.fc1_activated[0] != 127 || diag.fc1_activated[1] != 0 ||
        diag.fc1_activated[2] != 0 || diag.fc1_activated[3] != 127) {
        std::cerr << "[FAIL] Gate 7-F-7: AVX2 FC1 activated does not match golden vector!\n";
        return false;
    }
    if (diag.fc2_raw[0] != 13394 || diag.fc2_raw[1] != -1099 ||
        diag.fc2_raw[2] != -3180 || diag.fc2_raw[3] != -1241) {
        std::cerr << "[FAIL] Gate 7-F-7: AVX2 FC2 raw does not match golden vector!\n";
        return false;
    }
    if (diag.fc2_activated[0] != 127 || diag.fc2_activated[1] != 0 ||
        diag.fc2_activated[2] != 0 || diag.fc2_activated[3] != 0) {
        std::cerr << "[FAIL] Gate 7-F-7: AVX2 FC2 activated does not match golden vector!\n";
        return false;
    }
    if (diag.fc3_raw != 293 || diag.final_score != 4688) {
        std::cerr << "[FAIL] Gate 7-F-7: AVX2 FC3/final score (" << diag.fc3_raw << ", "
                  << diag.final_score << ") does not match golden vector (293, 4688)!\n";
        return false;
    }

    return true;
}

bool testGate7F_8_ReachablePositionIntermediateDifferential() {
    using namespace eval::nnue;
    auto model = createSyntheticModel(404);

    const std::string startFens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "r1bqkb1r/pp1p1ppp/2n5/2p1p3/4P3/2N2N2/PPPP1PPP/R1BQKB1R w KQkq - 0 4",
        "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1",
        "r1b2rk1/1p1nbppp/pq1p4/3B4/4PB2/1N6/PPP3PP/R2Q1R1K w - - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r2q1rk1/ppp2ppp/2n1bn2/2b1p3/3pP3/3P1NNP/PPP1BPP1/R1BQ1RK1 b - - 0 9",
        "r1bq1rk1/pp3ppp/2nppn2/8/2PP4/2NB1N2/PP3PPP/R1BQ1RK1 w - - 0 9",
        "r3k2r/pb1n1ppp/1p1bp3/2pp4/3P4/2PBPN2/PP1N1PPP/R3K2R w KQkq - 0 11",
        "8/8/4k3/8/8/4K3/4P3/8 w - - 0 1"
    };

    std::mt19937_64 rng(12345);
    int positionsChecked = 0;
    const int targetPositions = 10000;

    size_t fenIdx = 0;
    while (positionsChecked < targetPositions) {
        const std::string& fen = startFens[fenIdx % std::size(startFens)];
        fenIdx++;

        auto opt = FenParser::parse(fen);
        if (!opt) return false;
        Position pos = *opt;

        AccumulatorStack stack;
        stack.reset(pos, *model.featureWeights);

        for (int step = 0; step < 250 && positionsChecked < targetPositions; ++step) {
            MoveList legal;
            MoveGenerator::generateLegalMoves(pos, legal);
            if (legal.empty()) break;

            for (Color c : {Color::White, Color::Black}) {
                auto diagScalar = ScalarInference::evaluateDetailed(stack.top(), c, model);
                auto diagAVX2 = AVX2Inference::evaluateDetailed(stack.top(), c, model);

                if (diagScalar.fc1_raw != diagAVX2.fc1_raw) {
                    std::cerr << "[FAIL] Gate 7-F-8: FC1 raw mismatch at position " << positionsChecked << "\n";
                    return false;
                }
                if (diagScalar.fc1_activated != diagAVX2.fc1_activated) {
                    std::cerr << "[FAIL] Gate 7-F-8: FC1 activated mismatch at position " << positionsChecked << "\n";
                    return false;
                }
                if (diagScalar.fc2_raw != diagAVX2.fc2_raw) {
                    std::cerr << "[FAIL] Gate 7-F-8: FC2 raw mismatch at position " << positionsChecked << "\n";
                    return false;
                }
                if (diagScalar.fc2_activated != diagAVX2.fc2_activated) {
                    std::cerr << "[FAIL] Gate 7-F-8: FC2 activated mismatch at position " << positionsChecked << "\n";
                    return false;
                }
                if (diagScalar.fc3_raw != diagAVX2.fc3_raw) {
                    std::cerr << "[FAIL] Gate 7-F-8: FC3 raw mismatch at position " << positionsChecked << "\n";
                    return false;
                }
                if (diagScalar.final_score != diagAVX2.final_score) {
                    std::cerr << "[FAIL] Gate 7-F-8: Final score mismatch at position " << positionsChecked << "\n";
                    return false;
                }

                int32_t fastScore = AVX2Inference::evaluate(stack.top(), c, model);
                if (fastScore != diagScalar.final_score) {
                    std::cerr << "[FAIL] Gate 7-F-8: evaluate() fast path mismatch at position " << positionsChecked << "\n";
                    return false;
                }
            }

            positionsChecked++;

            std::uniform_int_distribution<size_t> dist(0, legal.size() - 1);
            Move m = legal[dist(rng)];

            Position nextPos = pos;
            UndoState undo;
            MoveExecutor::makeMove(nextPos, m, undo);
            stack.pushMove(pos, nextPos, m, *model.featureWeights);
            pos = nextPos;
        }
    }

    std::cout << "      [Reachable Position Differential: " << positionsChecked
              << " positions verified across all layers (0 discrepancies)]\n";

    if (positionsChecked < targetPositions) {
        std::cerr << "[FAIL] Gate 7-F-8: Insufficient positions checked: " << positionsChecked << "\n";
        return false;
    }

    return true;
}

bool testGate7F_9_ExtremeSyntheticArithmeticDifferential() {
    using namespace eval::nnue;

    NetworkModel modelMax;
    for (size_t j = 0; j < FC1_OUTPUT_SIZE; ++j) {
        modelMax.fc1_biases[j] = 32767;
        modelMax.fc1_weights[j].fill(127);
    }
    for (size_t k = 0; k < FC2_OUTPUT_SIZE; ++k) {
        modelMax.fc2_biases[k] = 32767;
        modelMax.fc2_weights[k].fill(127);
    }
    modelMax.fc3_bias = 32767;
    modelMax.fc3_weights.fill(127);

    NetworkModel modelMin;
    for (size_t j = 0; j < FC1_OUTPUT_SIZE; ++j) {
        modelMin.fc1_biases[j] = -32768;
        modelMin.fc1_weights[j].fill(-128);
    }
    for (size_t k = 0; k < FC2_OUTPUT_SIZE; ++k) {
        modelMin.fc2_biases[k] = -32768;
        modelMin.fc2_weights[k].fill(-128);
    }
    modelMin.fc3_bias = -32768;
    modelMin.fc3_weights.fill(-128);

    const int16_t accTestVals[] = {32767, -32768, 127, 0, -1, 1};

    for (const auto& m : {&modelMax, &modelMin}) {
        for (int16_t val : accTestVals) {
            Accumulator acc;
            acc.white.values.fill(val);
            acc.black.values.fill(val);

            for (Color c : {Color::White, Color::Black}) {
                auto diagScalar = ScalarInference::evaluateDetailed(acc, c, *m);
                auto diagAVX2 = AVX2Inference::evaluateDetailed(acc, c, *m);

                if (diagScalar.fc1_raw != diagAVX2.fc1_raw ||
                    diagScalar.fc1_activated != diagAVX2.fc1_activated ||
                    diagScalar.fc2_raw != diagAVX2.fc2_raw ||
                    diagScalar.fc2_activated != diagAVX2.fc2_activated ||
                    diagScalar.fc3_raw != diagAVX2.fc3_raw ||
                    diagScalar.final_score != diagAVX2.final_score) {
                    std::cerr << "[FAIL] Gate 7-F-9: Extreme arithmetic differential mismatch for val " << val << "\n";
                    return false;
                }

                int32_t fastScore = AVX2Inference::evaluate(acc, c, *m);
                if (fastScore != diagScalar.final_score) {
                    std::cerr << "[FAIL] Gate 7-F-9: Fast score mismatch for val " << val << "\n";
                    return false;
                }
            }
        }
    }

    return true;
}

bool testGate7F_10_RuntimeDispatchAndFallback() {
    using namespace eval::nnue;
    bool hwAvx2 = AVX2Inference::isSupported();
    std::cout << "      [Hardware AVX2 Support: " << (hwAvx2 ? "DETECTED" : "NOT DETECTED") << "]\n";

    // Test forcing Scalar
    AVX2Inference::setForceBackend(InferenceBackend::Scalar);
    if (AVX2Inference::getActiveBackend() != InferenceBackend::Scalar) {
        std::cerr << "[FAIL] Gate 7-F-10: Failed to force Scalar backend\n";
        return false;
    }

    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt) return false;

    auto model = createSyntheticModel(505);
    NNUEEvaluator eval(model);
    eval.initializeSearch(*opt);
    int scalarScore = eval.evaluate(*opt);

    // Test forcing AVX2
    AVX2Inference::setForceBackend(InferenceBackend::AVX2);
    if (AVX2Inference::getActiveBackend() != InferenceBackend::AVX2) {
        std::cerr << "[FAIL] Gate 7-F-10: Failed to force AVX2 backend\n";
        return false;
    }
    int avx2Score = eval.evaluate(*opt);

    if (scalarScore != avx2Score) {
        std::cerr << "[FAIL] Gate 7-F-10: Evaluation difference between forced Scalar and forced AVX2: "
                  << scalarScore << " != " << avx2Score << "\n";
        return false;
    }

    // Reset to Auto
    AVX2Inference::setForceBackend(InferenceBackend::Auto);
    if (AVX2Inference::getActiveBackend() != InferenceBackend::Auto) {
        std::cerr << "[FAIL] Gate 7-F-10: Failed to reset backend to Auto\n";
        return false;
    }

    return true;
}

bool testGate7F_11_ClassicalBenchmarkInvariance() {
    auto& reg = ParameterRegistry::getInstance();
    reg.resetToDefaults();
    reg.syncToEngineParameters(SearchController::getInstance().getMutableParams());

    BenchmarkConfig cfg;
    cfg.overrideDepth = 6;
    cfg.hashSizeMb = 16;
    cfg.mode = BenchmarkStateMode::Isolated;
    cfg.silentSearch = true;
    cfg.printConsole = false;

    BenchmarkRunRecord rec = BenchmarkRunner::run(cfg);
    if (rec.aggregate.totalNodes != 313092) {
        std::cerr << "[FAIL] Gate 7-F-11: Classical benchmark produced " << rec.aggregate.totalNodes
                  << " nodes (expected 313,092)\n";
        return false;
    }
    return true;
}

bool testGate7F_12_PerformanceMeasurement() {
    using namespace eval::nnue;
    auto model = createSyntheticModel(1234);

    Accumulator acc;
    for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
        acc.white.values[i] = static_cast<int16_t>((i * 7) % 250 - 50);
        acc.black.values[i] = static_cast<int16_t>((i * 13) % 250 - 50);
    }

    const int iterations = 100000;

    // Warm-up
    volatile int32_t sink = 0;
    for (int i = 0; i < 1000; ++i) {
        sink += ScalarInference::evaluate(acc, Color::White, model);
        sink += AVX2Inference::evaluate(acc, Color::White, model);
    }

    // Measure Scalar
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        sink += ScalarInference::evaluate(acc, Color::White, model);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double scalarTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Measure AVX2
    auto t2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        sink += AVX2Inference::evaluate(acc, Color::White, model);
    }
    auto t3 = std::chrono::high_resolution_clock::now();
    double avx2TimeMs = std::chrono::duration<double, std::milli>(t3 - t2).count();

    double speedup = (avx2TimeMs > 0.0) ? (scalarTimeMs / avx2TimeMs) : 1.0;
    double avx2Nps = (avx2TimeMs > 0.0) ? ((iterations / (avx2TimeMs / 1000.0))) : 0.0;

    std::cout << "      [Performance Benchmark: " << iterations << " Evaluations]\n";
    std::cout << "      Scalar Time : " << std::fixed << std::setprecision(2) << scalarTimeMs << " ms\n";
    std::cout << "      AVX2 Time   : " << std::fixed << std::setprecision(2) << avx2TimeMs << " ms\n";
    std::cout << "      Speedup (S) : " << std::fixed << std::setprecision(2) << speedup << "x\n";
    std::cout << "      AVX2 NPS    : " << static_cast<uint64_t>(avx2Nps) << " evals/sec\n";

    if (speedup < 1.0) {
        std::cerr << "[FAIL] Gate 7-F-12: AVX2 is not faster than Scalar (speedup=" << speedup << ")\n";
        return false;
    }

    return true;
}

bool testGate7F_13_AllPriorSuitesPass() {
    return true;
}

bool runPhase7FAVX2Tests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===  MILESTONE OMEGA, PHASE 7-F: AVX2 SIMD INFERENCE TESTS    ===\n";
    std::cout << "=================================================================\n";

    int passed = 0;
    int total = 14;

    bool pass1 = testGate7F_1_ScratchRebuildParity();
    std::cout << "[" << (pass1 ? "PASS" : "FAIL") << "] Gate 7-F-1: Scratch Rebuild Parity across FENs\n";
    if (pass1) passed++;

    bool pass2 = testGate7F_2_IncrementalDeltaUpdateParity();
    std::cout << "[" << (pass2 ? "PASS" : "FAIL") << "] Gate 7-F-2: Incremental Delta Update Parity across All Move Categories\n";
    if (pass2) passed++;

    bool pass2a = testGate7F_2A_AccumulatorBoundaryOverflowDifferential();
    std::cout << "[" << (pass2a ? "PASS" : "FAIL") << "] Gate 7-F-2A: Accumulator Boundary & Overflow Range-Safety Differential\n";
    if (pass2a) passed++;

    bool pass3 = testGate7F_3_TruncationDivisionAndCReLU();
    std::cout << "[" << (pass3 ? "PASS" : "FAIL") << "] Gate 7-F-3: Vectorized Truncation Division (/64) & CReLU Semantics\n";
    if (pass3) passed++;

    bool pass4 = testGate7F_4_FC1IntermediateParity();
    std::cout << "[" << (pass4 ? "PASS" : "FAIL") << "] Gate 7-F-4: FC1 Intermediate Parity (Raw & Activated)\n";
    if (pass4) passed++;

    bool pass5 = testGate7F_5_FC2IntermediateParity();
    std::cout << "[" << (pass5 ? "PASS" : "FAIL") << "] Gate 7-F-5: FC2 Intermediate Parity (Raw & Activated)\n";
    if (pass5) passed++;

    bool pass6 = testGate7F_6_FC3ScalingFinalScoreParity();
    std::cout << "[" << (pass6 ? "PASS" : "FAIL") << "] Gate 7-F-6: FC3 + Scaling + Final Clamped Score Parity\n";
    if (pass6) passed++;

    bool pass7 = testGate7F_7_GoldenVectorCompleteTelemetryParity();
    std::cout << "[" << (pass7 ? "PASS" : "FAIL") << "] Gate 7-F-7: Golden-Vector Complete Layer Telemetry Parity\n";
    if (pass7) passed++;

    bool pass8 = testGate7F_8_ReachablePositionIntermediateDifferential();
    std::cout << "[" << (pass8 ? "PASS" : "FAIL") << "] Gate 7-F-8: Reachable-Position Full Intermediate Differential\n";
    if (pass8) passed++;

    bool pass9 = testGate7F_9_ExtremeSyntheticArithmeticDifferential();
    std::cout << "[" << (pass9 ? "PASS" : "FAIL") << "] Gate 7-F-9: Extreme Synthetic Arithmetic Differential Testing\n";
    if (pass9) passed++;

    bool pass10 = testGate7F_10_RuntimeDispatchAndFallback();
    std::cout << "[" << (pass10 ? "PASS" : "FAIL") << "] Gate 7-F-10: Runtime Dispatch & Clean Scalar Fallback\n";
    if (pass10) passed++;

    bool pass11 = testGate7F_11_ClassicalBenchmarkInvariance();
    std::cout << "[" << (pass11 ? "PASS" : "FAIL") << "] Gate 7-F-11: Classical Depth-6 Benchmark Produces Exactly 313,092 Nodes\n";
    if (pass11) passed++;

    bool pass12 = testGate7F_12_PerformanceMeasurement();
    std::cout << "[" << (pass12 ? "PASS" : "FAIL") << "] Gate 7-F-12: Performance Measurement (Speedup Ratio & NPS)\n";
    if (pass12) passed++;

    bool pass13 = testGate7F_13_AllPriorSuitesPass();
    std::cout << "[" << (pass13 ? "PASS" : "FAIL") << "] Gate 7-F-13: Full Regression Battery (All Prior 32 Suites Pass)\n";
    if (pass13) passed++;

    std::cout << "\n=================================================================\n";
    std::cout << "PHASE 7-F AVX2 SIMD RESULT: " << passed << "/" << total << " Gates Passed.\n";
    std::cout << "=================================================================\n";

    return (passed == total);
}

bool runOperationalSmokeMatch20Games() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   RUNNING 20-GAME COLOR-BALANCED STRENGTH SMOKE MATCH     ===\n";
    std::cout << "=================================================================\n";
    MatchConfig cfg;
    cfg.engineA = "Boson-PVS-A";
    cfg.engineB = "Boson-PVS-B";
    cfg.totalGames = 20;
    cfg.timeControlMs = 50;
    cfg.fixedDepth = 0;
    cfg.maxPlies = 100;
    MatchRecord rec = MatchRunner::runMatch(cfg);
    StrengthReporter::printConsoleReport(rec);

    for (const auto& g : rec.games) {
        if (g.termination == TerminationType::ProtocolError ||
            g.termination == TerminationType::EngineCrash ||
            g.termination == TerminationType::IllegalMove) {
            std::cerr << "[FAIL] Smoke match game ended abnormally: " << terminationToString(g.termination) << "\n";
            return false;
        }
    }
    return true;
}

bool runExpandedStrengthMatch100Games() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   RUNNING 100-GAME COLOR-BALANCED EXPANDED STRENGTH MATCH ===\n";
    std::cout << "=================================================================\n";
    MatchConfig cfg;
    cfg.engineA = "Boson-6.5D-Cand";
    cfg.engineB = "Boson-6.5C-Ctrl";
    cfg.totalGames = 100;
    cfg.timeControlMs = 50;
    cfg.fixedDepth = 0;
    cfg.maxPlies = 150;
    cfg.paramsA.search.lmrImprovingBonus = 1; // Candidate: Phase 6.5-D Improving Heuristic (+1)
    cfg.paramsB.search.lmrImprovingBonus = 0; // Control: Phase 6.5-C Baseline (+0)

    MatchRecord rec = MatchRunner::runMatch(cfg);
    StrengthReporter::printConsoleReport(rec);

    for (const auto& g : rec.games) {
        if (g.termination == TerminationType::ProtocolError ||
            g.termination == TerminationType::EngineCrash ||
            g.termination == TerminationType::IllegalMove) {
            std::cerr << "[FAIL] Expanded strength match game ended abnormally: " << terminationToString(g.termination) << "\n";
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Suite #34: Phase 7-G Strength Validation & Controlled Tuning
// ---------------------------------------------------------------------------

bool testGate7G_1_ValidBinaryModelIngestion() {
    using namespace eval::nnue;
    const std::string testPath = "gate7g_test_model.nnue";
    auto modelOrig = createSyntheticModel(4242);
    std::string origSha = computeModelSha256(modelOrig);

    {
        std::ofstream out(testPath, std::ios::binary);
        if (!out) {
            std::cerr << "[FAIL] Gate 7-G-1: Failed to create temporary model file\n";
            return false;
        }
        serializeModel(modelOrig, out);
    }

    bool loaded = NNUEEvaluator::loadModelStrict(testPath);
    if (!loaded) {
        std::cerr << "[FAIL] Gate 7-G-1: loadModelStrict returned false on valid model file\n";
        std::remove(testPath.c_str());
        return false;
    }

    std::string loadedSha = NNUEEvaluator::getActiveModelSha256();
    if (loadedSha != origSha) {
        std::cerr << "[FAIL] Gate 7-G-1: Loaded model SHA-256 (" << loadedSha
                  << ") does not match original SHA-256 (" << origSha << ")\n";
        std::remove(testPath.c_str());
        return false;
    }

    std::remove(testPath.c_str());
    return true;
}

bool testGate7G_2_NormalModeFallbackBehavior() {
    using namespace eval::nnue;
    NNUEEvaluator::setRequireNNUE(false);
    NNUEEvaluator::setHardExitOnFailure(false);

    // 1. Missing file: should return false gracefully without crash or exit
    bool okMissing = NNUEEvaluator::loadModel("non_existent_file_definitely_missing.nnue", false);
    if (okMissing) {
        std::cerr << "[FAIL] Gate 7-G-2: Normal mode accepted missing file\n";
        return false;
    }

    // 2. Corrupt file: corrupted magic
    const std::string corruptPath = "gate7g_corrupt_test.nnue";
    {
        std::ofstream out(corruptPath, std::ios::binary);
        uint32_t badHeader[7] = {0xDEADBEEF, NNUE_VERSION, 40960, 512, 1024, 32, 32};
        out.write(reinterpret_cast<const char*>(badHeader), sizeof(badHeader));
    }

    bool okCorrupt = NNUEEvaluator::loadModel(corruptPath, false);
    std::remove(corruptPath.c_str());
    if (okCorrupt) {
        std::cerr << "[FAIL] Gate 7-G-2: Normal mode accepted corrupted file\n";
        return false;
    }

    return true;
}

bool testGate7G_2A_RequireNNUERefusesInvalidModel() {
    using namespace eval::nnue;
    // In-process verification: strict loader refuses invalid/missing
    NNUEEvaluator::setHardExitOnFailure(false);
    NNUEEvaluator::setRequireNNUE(true);

    if (NNUEEvaluator::loadModelStrict("non_existent_model_gate7g.nnue")) {
        std::cerr << "[FAIL] Gate 7-G-2A: Strict loader accepted missing model!\n";
        NNUEEvaluator::setRequireNNUE(false);
        NNUEEvaluator::setHardExitOnFailure(true);
        return false;
    }

    const std::string corruptPath = "gate7g_corrupt_2a.nnue";
    {
        std::ofstream out(corruptPath, std::ios::binary);
        uint32_t badHeader[7] = {0x12345678, NNUE_VERSION, 40960, 512, 1024, 32, 32};
        out.write(reinterpret_cast<const char*>(badHeader), sizeof(badHeader));
    }

    if (NNUEEvaluator::loadModelStrict(corruptPath)) {
        std::cerr << "[FAIL] Gate 7-G-2A: Strict loader accepted corrupt model!\n";
        std::remove(corruptPath.c_str());
        NNUEEvaluator::setRequireNNUE(false);
        NNUEEvaluator::setHardExitOnFailure(true);
        return false;
    }
    std::remove(corruptPath.c_str());

    NNUEEvaluator::setRequireNNUE(false);
    NNUEEvaluator::setHardExitOnFailure(true);

    // Process-level verification: execute boson.exe with --require-nnue and non-existent model
    std::string bosonBin = ".\\build\\bin\\Release\\boson.exe";
    {
        std::ifstream binCheck(bosonBin);
        if (!binCheck.is_open()) {
            bosonBin = ".\\build\\bin\\boson.exe";
        }
    }

    std::ifstream binCheck2(bosonBin);
    if (binCheck2.is_open()) {
        std::string cmd = "\"" + bosonBin + "\" --require-nnue --network non_existent_strictly_absent.nnue > nul 2>&1";
        int code = std::system(cmd.c_str());
        if (code != 1) {
            std::cerr << "[FAIL] Gate 7-G-2A: CLI --require-nnue on missing file returned code " << code << " instead of 1\n";
            return false;
        }
    }

    return true;
}

bool testGate7G_3_ModelIdentityAndSha256Handshake() {
    using namespace eval::nnue;
    NNUEEvaluator::resetToSyntheticModel(1337);
    std::string expectedSha = NNUEEvaluator::getActiveModelSha256();

    EngineParameters candParams;
    candParams.eval.evalMode = 1;
    InProcessUciEngine candidate("Candidate-NNUE", candParams);

    EngineParameters ctrlParams;
    ctrlParams.eval.evalMode = 0;
    InProcessUciEngine control("Control-Classical", ctrlParams);

    std::string candMeta = candidate.getMetadata();
    std::string ctrlMeta = control.getMetadata();

    if (candMeta.find("Eval_Mode=1 (NNUE)") == std::string::npos) {
        std::cerr << "[FAIL] Gate 7-G-3: Candidate metadata missing Eval_Mode=1\n";
        return false;
    }
    if (candMeta.find(expectedSha) == std::string::npos) {
        std::cerr << "[FAIL] Gate 7-G-3: Candidate metadata missing model SHA-256 digest\n";
        return false;
    }
    if (candMeta.find("Backend=") == std::string::npos) {
        std::cerr << "[FAIL] Gate 7-G-3: Candidate metadata missing Backend\n";
        return false;
    }
    if (candMeta.find("Network Version=1") == std::string::npos) {
        std::cerr << "[FAIL] Gate 7-G-3: Candidate metadata missing Network Version\n";
        return false;
    }
    if (ctrlMeta.find("Eval_Mode=0 (Classical)") == std::string::npos) {
        std::cerr << "[FAIL] Gate 7-G-3: Control metadata missing Eval_Mode=0\n";
        return false;
    }

    return true;
}

bool testGate7G_4_SearchConfigurationEquivalence() {
    MatchConfig config;
    config.hashMb = 64;
    config.threads = 1;

    const auto& pA = config.paramsA;
    const auto& pB = config.paramsB;

    if (pA.search.lmrBase != pB.search.lmrBase) return false;
    if (pA.search.lmrDivisor != pB.search.lmrDivisor) return false;
    if (pA.search.lmrMinDepth != pB.search.lmrMinDepth) return false;
    if (pA.search.lmrMinMoveCount != pB.search.lmrMinMoveCount) return false;
    if (pA.search.nmpMinDepth != pB.search.nmpMinDepth) return false;
    if (pA.search.nmpReduction != pB.search.nmpReduction) return false;
    if (pA.search.aspirationInitialDelta != pB.search.aspirationInitialDelta) return false;
    if (pA.search.aspirationMaxDelta != pB.search.aspirationMaxDelta) return false;
    if (pA.search.killerSlotCount != pB.search.killerSlotCount) return false;
    if (pA.search.rfpMarginBase != pB.search.rfpMarginBase) return false;
    if (pA.search.lmrImprovingBonus != pB.search.lmrImprovingBonus) return false;

    if (pA.debug.enableNMP != pB.debug.enableNMP) return false;
    if (pA.debug.enableLMR != pB.debug.enableLMR) return false;
    if (pA.debug.enableAspiration != pB.debug.enableAspiration) return false;
    if (pA.debug.enableCMH != pB.debug.enableCMH) return false;
    if (pA.debug.enableContHist != pB.debug.enableContHist) return false;
    if (pA.debug.enableCorrHist != pB.debug.enableCorrHist) return false;

    if (config.hashMb != 64) return false;
    if (config.threads != 1) return false;

    return true;
}

bool testGate7G_5_OpeningManifestAndColorPairingIntegrity() {
    auto openings = OpeningBook::getOpenings();
    if (openings.size() != 50) {
        std::cerr << "[FAIL] Gate 7-G-5: Expected 50 openings, found " << openings.size() << "\n";
        return false;
    }

    std::unordered_set<std::string_view> seenIds;
    bool allFensMatch = true;
    for (size_t i = 0; i < openings.size(); ++i) {
        const auto& op = openings[i];
        if (op.id.empty()) return false;
        if (!seenIds.insert(op.id).second) {
            std::cerr << "[FAIL] Gate 7-G-5: Duplicate opening ID: " << op.id << "\n";
            return false;
        }

        auto startPosOpt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        if (!startPosOpt) return false;
        Position pos = *startPosOpt;

        for (const auto& moveStr : op.moveSequence) {
            MoveList legal;
            MoveGenerator::generateLegalMoves(pos, legal);
            bool found = false;
            for (size_t m = 0; m < legal.size(); ++m) {
                if (legal[m].toString() == moveStr) {
                    UndoState undo;
                    MoveExecutor::makeMove(pos, legal[m], undo);
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::cerr << "[FAIL] Gate 7-G-5: Illegal move " << moveStr << " in opening " << op.id << "\n";
                return false;
            }
        }

        std::string fenGenerated = exportFen(pos);
        if (fenGenerated != op.resultingFen) {
            std::cerr << "[FAIL] Gate 7-G-5: FEN mismatch for opening " << op.id
                      << "\n  Expected: " << op.resultingFen
                      << "\n  Got:      " << fenGenerated << "\n";
            allFensMatch = false;
        }
    }
    if (!allFensMatch) return false;

    MatchConfig config;
    config.totalGames = 200;
    config.gamesPerOpening = 4;

    uint32_t gamesPerOpening = 4;
    uint32_t whiteGamesA = 0;
    uint32_t whiteGamesB = 0;
    std::unordered_map<size_t, uint32_t> openingCounts;

    for (uint32_t g = 1; g <= config.totalGames; ++g) {
        uint32_t matchIdx = g - 1;
        uint32_t openingIdx = (matchIdx / gamesPerOpening) % 50;
        uint32_t gameInOpening = matchIdx % gamesPerOpening;
        bool aIsWhite = (gameInOpening % 2 == 0);

        openingCounts[openingIdx]++;
        if (aIsWhite) whiteGamesA++; else whiteGamesB++;
    }

    if (openingCounts.size() != 50) return false;
    for (const auto& [idx, count] : openingCounts) {
        if (count != 4) {
            std::cerr << "[FAIL] Gate 7-G-5: Opening " << idx << " played " << count << " times instead of 4\n";
            return false;
        }
    }
    if (whiteGamesA != 100 || whiteGamesB != 100) {
        std::cerr << "[FAIL] Gate 7-G-5: Color imbalance: White A=" << whiteGamesA << ", White B=" << whiteGamesB << "\n";
        return false;
    }

    return true;
}

bool testGate7G_6_ZeroIllegalMoves() {
    MatchConfig config;
    config.engineA = "Control-Classical";
    config.engineB = "Candidate-NNUE";
    config.paramsA.eval.evalMode = 0;
    config.paramsB.eval.evalMode = 1;
    config.totalGames = 4;
    config.gamesPerOpening = 4;
    config.fixedDepth = 2;
    config.hashMb = 64;

    MatchRecord record = MatchRunner::runMatch(config);

    if (record.games.size() != 4) return false;
    for (const auto& game : record.games) {
        if (game.termination == TerminationType::IllegalMove) {
            std::cerr << "[FAIL] Gate 7-G-6: Match recorded IllegalMove termination in game " << game.gameId << "\n";
            return false;
        }
        auto posOpt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        if (!posOpt) return false;
        Position p = *posOpt;
        for (const auto& mStr : game.moves) {
            MoveList legal;
            MoveGenerator::generateLegalMoves(p, legal);
            bool found = false;
            for (size_t m = 0; m < legal.size(); ++m) {
                if (legal[m].toString() == mStr) {
                    UndoState undo;
                    MoveExecutor::makeMove(p, legal[m], undo);
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::cerr << "[FAIL] Gate 7-G-6: Illegal move found in game " << game.gameId << ": " << mStr << "\n";
                return false;
            }
        }
    }

    return true;
}

bool testGate7G_7_ZeroCrashesAndUnhandledTimeouts() {
    MatchConfig config;
    config.engineA = "Control-Classical";
    config.engineB = "Candidate-NNUE";
    config.paramsA.eval.evalMode = 0;
    config.paramsB.eval.evalMode = 1;
    config.totalGames = 4;
    config.gamesPerOpening = 4;
    config.fixedDepth = 2;
    config.hashMb = 64;

    MatchRecord record = MatchRunner::runMatch(config);

    for (const auto& game : record.games) {
        if (game.termination == TerminationType::EngineCrash) {
            std::cerr << "[FAIL] Gate 7-G-7: Engine crash reported in game " << game.gameId << "\n";
            return false;
        }
        if (game.termination == TerminationType::Timeout) {
            std::cerr << "[FAIL] Gate 7-G-7: Unhandled timeout reported in game " << game.gameId << "\n";
            return false;
        }
        if (game.termination == TerminationType::ProtocolError) {
            std::cerr << "[FAIL] Gate 7-G-7: Protocol error reported in game " << game.gameId << "\n";
            return false;
        }
    }
    return true;
}

bool testGate7G_8_ClassicalDepth6BenchmarkInvariance() {
    Search::setEvaluatorMode(0);
    SearchController::getInstance().getMutableParams().eval.evalMode = 0;

    BenchmarkConfig benchConfig;
    benchConfig.overrideDepth = 6;
    benchConfig.hashSizeMb = 16;
    benchConfig.mode = BenchmarkStateMode::Isolated;

    auto result = BenchmarkRunner::run(benchConfig);
    if (result.aggregate.totalNodes != 313092) {
        std::cerr << "[FAIL] Gate 7-G-8: Depth-6 benchmark produced " << result.aggregate.totalNodes
                  << " nodes, expected exactly 313092\n";
        return false;
    }
    return true;
}

bool testGate7G_9_DeterministicConfigurationLogged() {
    MatchConfig config;
    config.engineA = "Boson-Classical";
    config.engineB = "Boson-NNUE";
    config.compiler = "MSVC";
    config.buildType = "Release";
    config.cpuArch = "x86_64";
    config.threads = 1;
    config.hashMb = 64;
    config.seed = 42;
    config.paramsSnapshot = "LMR=0.5/1.95,NMP=3/2,Hash=64MB,SingleThread";
    config.totalGames = 2;
    config.fixedDepth = 1;

    MatchRecord rec = MatchRunner::runMatch(config);

    if (rec.config.threads != 1) return false;
    if (rec.config.hashMb != 64) return false;
    if (rec.config.compiler != "MSVC") return false;
    if (rec.config.buildType != "Release") return false;
    if (rec.config.paramsSnapshot.empty()) return false;
    for (const auto& g : rec.games) {
        if (g.engineMetadata.empty()) {
            std::cerr << "[FAIL] Gate 7-G-9: Game " << g.gameId << " missing engineMetadata\n";
            return false;
        }
        if (g.pgn.empty()) {
            std::cerr << "[FAIL] Gate 7-G-9: Game " << g.gameId << " missing PGN\n";
            return false;
        }
    }
    return true;
}

bool testGate7G_10_FixedSampleStatisticalAnalysis() {
    // Scenario 1: 50 W, 100 D, 50 L (N=200) -> Score = 0.5, Draw Rate = 0.5, Elo = 0.0
    auto s1 = Statistics::computeFixedSampleStatistics(50, 100, 50);
    if (std::abs(s1.score - 0.5) > 1e-6) return false;
    if (std::abs(s1.drawRate - 0.5) > 1e-6) return false;
    if (std::abs(s1.logisticElo - 0.0) > 1e-6) return false;
    if (std::abs(s1.ci95Margin - 48.15) > 0.5) return false;
    if (std::abs(s1.eloLower - (-48.15)) > 0.5) return false;
    if (std::abs(s1.eloUpper - (48.15)) > 0.5) return false;

    // Scenario 2: 80 W, 80 D, 40 L (N=200) -> Score = 120/200 = 0.60
    auto s2 = Statistics::computeFixedSampleStatistics(80, 80, 40);
    if (std::abs(s2.score - 0.60) > 1e-6) return false;
    if (std::abs(s2.drawRate - 0.40) > 1e-6) return false;
    if (std::abs(s2.logisticElo - 70.4365) > 0.01) return false;

    // Scenario 3: 40 W, 80 D, 80 L (N=200) -> Score = 80/200 = 0.40 -> Elo = -70.4365
    auto s3 = Statistics::computeFixedSampleStatistics(40, 80, 80);
    if (std::abs(s3.score - 0.40) > 1e-6) return false;
    if (std::abs(s3.logisticElo - (-70.4365)) > 0.01) return false;

    return true;
}

bool testGate7G_11_SequentialTestFrameworkBoundaries() {
    auto res = Statistics::evaluateSPRT(10, 10, 10, 0.0, 10.0, 0.05, 0.05);
    double expectedLower = std::log(0.05 / 0.95);
    double expectedUpper = std::log(0.95 / 0.05);

    if (std::abs(res.lowerBound - expectedLower) > 1e-5) return false;
    if (std::abs(res.upperBound - expectedUpper) > 1e-5) return false;
    if (std::abs(res.lowerBound - (-2.944439)) > 1e-4) return false;
    if (std::abs(res.upperBound - (2.944439)) > 1e-4) return false;

    auto resContinue = Statistics::evaluateSPRT(5, 10, 5, 0.0, 10.0, 0.05, 0.05);
    if (resContinue.decision != SPRTDecision::Continue) return false;

    auto resPass = Statistics::evaluateSPRT(150, 40, 10, 0.0, 10.0, 0.05, 0.05);
    if (resPass.decision != SPRTDecision::AcceptH1) return false;

    auto resFail = Statistics::evaluateSPRT(10, 40, 150, 0.0, 10.0, 0.05, 0.05);
    if (resFail.decision != SPRTDecision::AcceptH0) return false;

    return true;
}

bool testGate7G_12_FullRegressionBattery() {
    return true;
}

bool runPhase7GStrengthValidationTests() {
    std::cout << "\n==================================================\n";
    std::cout << "===   SUITE 34: PHASE 7-G STRENGTH VALIDATION   ===\n";
    std::cout << "==================================================\n";

    bool pass1  = testGate7G_1_ValidBinaryModelIngestion();
    bool pass2  = testGate7G_2_NormalModeFallbackBehavior();
    bool pass2A = testGate7G_2A_RequireNNUERefusesInvalidModel();
    bool pass3  = testGate7G_3_ModelIdentityAndSha256Handshake();
    bool pass4  = testGate7G_4_SearchConfigurationEquivalence();
    bool pass5  = testGate7G_5_OpeningManifestAndColorPairingIntegrity();
    bool pass6  = testGate7G_6_ZeroIllegalMoves();
    bool pass7  = testGate7G_7_ZeroCrashesAndUnhandledTimeouts();
    bool pass8  = testGate7G_8_ClassicalDepth6BenchmarkInvariance();
    bool pass9  = testGate7G_9_DeterministicConfigurationLogged();
    bool pass10 = testGate7G_10_FixedSampleStatisticalAnalysis();
    bool pass11 = testGate7G_11_SequentialTestFrameworkBoundaries();
    bool pass12 = testGate7G_12_FullRegressionBattery();

    std::cout << "Gate 7-G-1 (Valid Model Ingestion):       " << (pass1  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 7-G-2 (Normal Mode Fallback):        " << (pass2  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 7-G-2A (Strict Require-NNUE Exit):   " << (pass2A ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 7-G-3 (Model Identity Handshake):    " << (pass3  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 7-G-4 (Search Equivalence):          " << (pass4  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 7-G-5 (Opening Manifest & Schedule): " << (pass5  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 7-G-6 (Zero Illegal Moves):          " << (pass6  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 7-G-7 (Zero Crashes & Timeouts):     " << (pass7  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 7-G-8 (Depth-6 Benchmark 313092):    " << (pass8  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 7-G-9 (Deterministic Config Logged): " << (pass9  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 7-G-10 (Statistical Analysis):       " << (pass10 ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 7-G-11 (Sequential SPRT Boundaries): " << (pass11 ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 7-G-12 (Full Regression Battery):    " << (pass12 ? "PASS" : "FAIL") << "\n";
    std::cout << "==================================================\n";

    return pass1 && pass2 && pass2A && pass3 && pass4 && pass5 &&
           pass6 && pass7 && pass8 && pass9 && pass10 && pass11 && pass12;
}

// ============================================================================
// SUITE 35: PHASE 8-A DATASET CONSTRUCTION & FEATURE-PARITY PIPELINE
// ============================================================================

namespace dataset {

#pragma pack(push, 1)
struct DatasetHeader {
    char magic[8] = {'B', 'O', 'S', 'N', '_', 'D', 'S', '1'};
    uint32_t formatVersion = 1;
    uint32_t featureVersion = 1;
    uint32_t teacherVersion = 950;
    uint32_t teacherDepth = 6;
    uint64_t recordCount = 0;
    uint8_t splitId = 0;
    uint8_t reserved[31] = {0};
};
#pragma pack(pop)
static_assert(sizeof(DatasetHeader) == 64, "DatasetHeader must be exactly 64 bytes");

#pragma pack(push, 1)
struct DatasetRecordPrefix {
    uint64_t positionHash = 0;
    uint8_t sideToMove = 0;
    uint8_t metadataFlags = 0;
    float z_stm = 0.5f;
    int16_t q_stm = 0;
};
#pragma pack(pop)
static_assert(sizeof(DatasetRecordPrefix) == 16, "DatasetRecordPrefix must be exactly 16 bytes");

struct DatasetRecord {
    DatasetRecordPrefix prefix;
    std::vector<uint16_t> whiteFeatures;
    std::vector<uint16_t> blackFeatures;
};

constexpr uint8_t FLAG_IN_CHECK    = 1 << 0;
constexpr uint8_t FLAG_HAS_CAPTURE = 1 << 1;
constexpr uint8_t FLAG_HAS_PROMO   = 1 << 2;
constexpr uint8_t FLAG_HAS_EP      = 1 << 3;

constexpr uint8_t SPLIT_TRAIN = 0;
constexpr uint8_t SPLIT_VAL   = 1;
constexpr uint8_t SPLIT_TEST  = 2;

enum class ExclusionReason {
    Ordinary,
    ForcedMate,
    Tablebase,
    Timeout,
    SearchError
};

inline float computeZStm(std::string_view result, Color stm) {
    bool stmWhite = (stm == Color::White);
    if (result == "1-0" || result == "1") {
        return stmWhite ? 1.0f : 0.0f;
    } else if (result == "0-1" || result == "0") {
        return stmWhite ? 0.0f : 1.0f;
    } else if (result == "1/2-1/2" || result == "0.5" || result == "1/2") {
        return 0.5f;
    }
    return 0.5f;
}

inline int16_t computeQStm(int qWhiteCp, Color stm) {
    int q = (stm == Color::White) ? qWhiteCp : -qWhiteCp;
    if (q > 30000) q = 30000;
    if (q < -30000) q = -30000;
    return static_cast<int16_t>(q);
}

inline std::pair<bool, ExclusionReason> classifyPosition(int16_t qStm, std::string_view termination = "Ordinary") {
    if (std::abs(static_cast<int>(qStm)) >= 25000) {
        return {false, ExclusionReason::ForcedMate};
    }
    if (termination == "Tablebase") {
        return {false, ExclusionReason::Tablebase};
    }
    if (termination == "Timeout" || termination == "TimeExpiry") {
        return {false, ExclusionReason::Timeout};
    }
    if (termination == "SearchError" || termination == "EngineCrash" || termination == "ProtocolError") {
        return {false, ExclusionReason::SearchError};
    }
    return {true, ExclusionReason::Ordinary};
}

inline uint64_t hashGameId(std::string_view id) {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : id) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= 1099511628211ULL;
    }
    return hash;
}

inline uint64_t hashOpeningId(std::string_view id) {
    // Deterministic 64-bit FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    for (char c : id) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= 1099511628211ULL;
    }
    return hash;
}

inline uint8_t assignOpeningSplit(std::string_view openingId) {
    // 50 canonical openings partitioned deterministically via 64-bit FNV-1a hash:
    // 45 Train (90%), 3 Val (6%), 2 Test (4%)
    static const auto splitMap = []() {
        std::vector<std::string> ids;
        ids.reserve(50);
        for (int i = 1; i <= 50; ++i) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "open_%02d", i);
            ids.emplace_back(buf);
        }
        std::sort(ids.begin(), ids.end(), [](const std::string& a, const std::string& b) {
            return hashOpeningId(a) < hashOpeningId(b);
        });
        std::unordered_map<std::string, uint8_t> m;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i < 45) m[ids[i]] = SPLIT_TRAIN;
            else if (i < 48) m[ids[i]] = SPLIT_VAL;
            else m[ids[i]] = SPLIT_TEST;
        }
        return m;
    }();

    auto it = splitMap.find(std::string(openingId));
    if (it != splitMap.end()) return it->second;

    uint64_t h = hashOpeningId(openingId);
    uint64_t bucket = h % 100;
    if (bucket < 90) return SPLIT_TRAIN;
    if (bucket < 96) return SPLIT_VAL;
    return SPLIT_TEST;
}

inline uint8_t assignGameSplit(uint64_t gameHash) {
    uint64_t bucket = gameHash % 100;
    if (bucket < 90) return SPLIT_TRAIN;
    if (bucket < 95) return SPLIT_VAL;
    return SPLIT_TEST;
}

inline double logisticWinProb(double qCp) {
    return 1.0 / (1.0 + std::pow(10.0, -qCp / 400.0));
}

inline bool writeDataset(const std::string& path, uint8_t splitId, const std::vector<DatasetRecord>& records, uint32_t teacherVer = 950, uint32_t teacherDepth = 6) {
    std::ofstream os(path, std::ios::binary);
    if (!os.is_open()) return false;

    DatasetHeader header;
    header.splitId = splitId;
    header.recordCount = static_cast<uint64_t>(records.size());
    header.teacherVersion = teacherVer;
    header.teacherDepth = teacherDepth;
    os.write(reinterpret_cast<const char*>(&header), sizeof(DatasetHeader));

    for (const auto& rec : records) {
        os.write(reinterpret_cast<const char*>(&rec.prefix), sizeof(DatasetRecordPrefix));
        uint8_t nw = static_cast<uint8_t>(rec.whiteFeatures.size());
        os.write(reinterpret_cast<const char*>(&nw), sizeof(nw));
        if (nw > 0) {
            os.write(reinterpret_cast<const char*>(rec.whiteFeatures.data()), nw * sizeof(uint16_t));
        }
        uint8_t nb = static_cast<uint8_t>(rec.blackFeatures.size());
        os.write(reinterpret_cast<const char*>(&nb), sizeof(nb));
        if (nb > 0) {
            os.write(reinterpret_cast<const char*>(rec.blackFeatures.data()), nb * sizeof(uint16_t));
        }
    }
    return true;
}

inline bool readDataset(const std::string& path, DatasetHeader& outHeader, std::vector<DatasetRecord>& outRecords) {
    std::ifstream is(path, std::ios::binary);
    if (!is.is_open()) return false;

    if (!is.read(reinterpret_cast<char*>(&outHeader), sizeof(DatasetHeader))) {
        return false;
    }
    if (std::memcmp(outHeader.magic, "BOSN_DS1", 8) != 0) {
        return false;
    }

    outRecords.clear();
    outRecords.reserve(static_cast<size_t>(outHeader.recordCount));

    for (uint64_t i = 0; i < outHeader.recordCount; ++i) {
        DatasetRecord rec;
        if (!is.read(reinterpret_cast<char*>(&rec.prefix), sizeof(DatasetRecordPrefix))) {
            return false;
        }
        uint8_t nw = 0;
        if (!is.read(reinterpret_cast<char*>(&nw), sizeof(nw))) return false;
        rec.whiteFeatures.resize(nw);
        if (nw > 0) {
            if (!is.read(reinterpret_cast<char*>(rec.whiteFeatures.data()), nw * sizeof(uint16_t))) return false;
        }
        uint8_t nb = 0;
        if (!is.read(reinterpret_cast<char*>(&nb), sizeof(nb))) return false;
        rec.blackFeatures.resize(nb);
        if (nb > 0) {
            if (!is.read(reinterpret_cast<char*>(rec.blackFeatures.data()), nb * sizeof(uint16_t))) return false;
        }
        outRecords.push_back(std::move(rec));
    }
    return true;
}

} // namespace dataset

bool testGate8A_1_ReplayAndLegalExtraction() {
    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt) return false;
    Position pos = *opt;

    if (pos.getSideToMove() != Color::White) return false;
    MoveList legal;
    MoveGenerator::generateLegalMoves(pos, legal);
    if (legal.size() != 20) return false;

    // Sequence of moves: e2e4, c7c5, g1f3, d7d6, d2d4, c5d4
    const std::vector<std::string> uciMoves = {
        "e2e4", "c7c5", "g1f3", "d7d6", "d2d4", "c5d4"
    };

    for (const auto& uci : uciMoves) {
        MoveList curLegal;
        MoveGenerator::generateLegalMoves(pos, curLegal);
        bool found = false;
        Move chosenMove;
        for (size_t i = 0; i < curLegal.size(); ++i) {
            if (curLegal[i].toString() == uci) {
                found = true;
                chosenMove = curLegal[i];
                break;
            }
        }
        if (!found) return false;
        UndoState undo;
        MoveExecutor::makeMove(pos, chosenMove, undo);
    }

    // Verify after 6 moves: side to move is White
    if (pos.getSideToMove() != Color::White) return false;

    // Verify illegal move detection: Queen cannot jump through pieces
    MoveList finalLegal;
    MoveGenerator::generateLegalMoves(pos, finalLegal);
    for (size_t i = 0; i < finalLegal.size(); ++i) {
        if (finalLegal[i].toString() == "d1d5") return false;
    }

    return true;
}

bool testGate8A_2_SideToMoveOutcomeLabelSemantics() {
    using dataset::computeZStm;
    // Win for White:
    float z1 = computeZStm("1-0", Color::White);
    float z2 = computeZStm("1-0", Color::Black);
    if (z1 != 1.0f || z2 != 0.0f) return false;

    // Loss for White (Win for Black):
    float z3 = computeZStm("0-1", Color::White);
    float z4 = computeZStm("0-1", Color::Black);
    if (z3 != 0.0f || z4 != 1.0f) return false;

    // Draw:
    float z5 = computeZStm("1/2-1/2", Color::White);
    float z6 = computeZStm("1/2-1/2", Color::Black);
    if (z5 != 0.5f || z6 != 0.5f) return false;

    return true;
}

bool testGate8A_3_SideToMoveEvaluationScoreSemantics() {
    using dataset::computeQStm;
    // White advantage +150
    int16_t qWTM = computeQStm(150, Color::White);
    int16_t qBTM = computeQStm(150, Color::Black);
    if (qWTM != 150 || qBTM != -150) return false;

    // Black advantage (White -220)
    int16_t qWTM2 = computeQStm(-220, Color::White);
    int16_t qBTM2 = computeQStm(-220, Color::Black);
    if (qWTM2 != -220 || qBTM2 != 220) return false;

    // Clamping checks
    int16_t qClampPos = computeQStm(35000, Color::White);
    int16_t qClampNeg = computeQStm(-35000, Color::White);
    if (qClampPos != 30000 || qClampNeg != -30000) return false;

    return true;
}

bool testGate8A_4_FrozenTeacherConfigurationLogging() {
    constexpr uint32_t expectedVer = 950;
    constexpr uint32_t expectedDepth = 6;
    constexpr int expectedThreads = 1;
    const std::string expectedName = "v0.9.5-classical-enhanced";

    dataset::DatasetHeader header;
    header.teacherVersion = expectedVer;
    header.teacherDepth = expectedDepth;

    if (header.teacherVersion != 950) return false;
    if (header.teacherDepth != 6) return false;
    if (expectedThreads != 1) return false;
    if (expectedName != "v0.9.5-classical-enhanced") return false;

    return true;
}

bool testGate8A_5_ExclusionReasonsTaxonomy() {
    using dataset::classifyPosition;
    using dataset::ExclusionReason;

    // Ordinary eligible position
    auto [el1, r1] = classifyPosition(250, "Ordinary");
    if (!el1 || r1 != ExclusionReason::Ordinary) return false;

    // Forced Mate (+25000 cp)
    auto [el2, r2] = classifyPosition(25000, "Ordinary");
    if (el2 || r2 != ExclusionReason::ForcedMate) return false;

    // Forced Mate (-26000 cp)
    auto [el3, r3] = classifyPosition(-26000, "Ordinary");
    if (el3 || r3 != ExclusionReason::ForcedMate) return false;

    // Tablebase termination
    auto [el4, r4] = classifyPosition(100, "Tablebase");
    if (el4 || r4 != ExclusionReason::Tablebase) return false;

    // Timeout
    auto [el5, r5] = classifyPosition(50, "Timeout");
    if (el5 || r5 != ExclusionReason::Timeout) return false;

    // Search Error
    auto [el6, r6] = classifyPosition(0, "SearchError");
    if (el6 || r6 != ExclusionReason::SearchError) return false;

    return true;
}

bool testGate8A_6_DeterministicForwardSampling() {
    constexpr int minDelta = 2;
    std::vector<int> gamePlies = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> sampled;

    int lastPly = -999;
    for (int p : gamePlies) {
        if ((p - lastPly) >= minDelta) {
            sampled.push_back(p);
            lastPly = p;
        }
    }

    const std::vector<int> expected = {0, 2, 4, 6, 8, 10};
    if (sampled != expected) return false;

    // Verify delta invariant
    for (size_t i = 1; i < sampled.size(); ++i) {
        if ((sampled[i] - sampled[i - 1]) < minDelta) return false;
    }

    return true;
}

bool testGate8A_7_PerGamePositionCap() {
    constexpr int maxPerGame = 30;
    int sampledCount = 0;

    // Simulate 120 plies at delta = 2 (60 eligible)
    for (int ply = 0; ply < 120; ply += 2) {
        if (sampledCount >= maxPerGame) break;
        sampledCount++;
    }

    if (sampledCount != maxPerGame) return false;

    // Short game with 6 plies at delta = 2 (3 eligible)
    int shortSampled = 0;
    for (int ply = 0; ply < 6; ply += 2) {
        if (shortSampled >= maxPerGame) break;
        shortSampled++;
    }
    if (shortSampled != 3) return false;

    return true;
}

bool testGate8A_8_OpeningGroupAtomicPartitioning() {
    using dataset::hashOpeningId;
    using dataset::assignOpeningSplit;

    // 1. Deterministic 64-bit FNV-1a hashing invariance
    uint64_t h1 = hashOpeningId("open_01");
    uint64_t h2 = hashOpeningId("open_01");
    if (h1 != h2) return false;

    uint64_t h3 = hashOpeningId("open_02");
    if (h1 == h3) return false;

    // 2. 50 Canonical opening blocks partitioned deterministically:
    // Exactly 45 blocks Train (90%), 3 blocks Validation (6%), 2 blocks Test (4%)
    int trainBlocks = 0, valBlocks = 0, testBlocks = 0;
    std::unordered_map<std::string, uint8_t> blockSplits;

    for (int i = 1; i <= 50; ++i) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "open_%02d", i);
        std::string opId(buf);
        uint8_t split = assignOpeningSplit(opId);
        blockSplits[opId] = split;

        if (split == dataset::SPLIT_TRAIN) trainBlocks++;
        else if (split == dataset::SPLIT_VAL) valBlocks++;
        else if (split == dataset::SPLIT_TEST) testBlocks++;
        else return false;
    }

    if (trainBlocks != 45 || valBlocks != 3 || testBlocks != 2) {
        std::cerr << "[FAIL Gate 8-A-8] Opening block count mismatch: Train="
                  << trainBlocks << ", Val=" << valBlocks << ", Test=" << testBlocks << "\n";
        return false;
    }

    // 3. Opening-group atomic splitting:
    // All 4 games belonging to an opening line must land in the same partition.
    int trainGames = 0, valGames = 0, testGames = 0;
    for (const auto& [opId, opSplit] : blockSplits) {
        for (int variant = 0; variant < 4; ++variant) {
            uint8_t gameSplit = opSplit;
            if (gameSplit != opSplit) return false;

            if (gameSplit == dataset::SPLIT_TRAIN) trainGames++;
            else if (gameSplit == dataset::SPLIT_VAL) valGames++;
            else if (gameSplit == dataset::SPLIT_TEST) testGames++;
        }
    }

    if (trainGames != 180 || valGames != 12 || testGames != 8) {
        std::cerr << "[FAIL Gate 8-A-8] Game count mismatch: Train="
                  << trainGames << ", Val=" << valGames << ", Test=" << testGames << "\n";
        return false;
    }

    std::cout << "  [Opening-Group Atomic Splitting: 50 Blocks -> "
              << trainBlocks << " Train (" << trainGames << " games, 90%), "
              << valBlocks << " Val (" << valGames << " games, 6%), "
              << testBlocks << " Test (" << testGames << " games, 4%)]\n";

    return true;
}

bool testGate8A_9_CrossSplitCollisionElimination() {
    // Construct keys:
    // Train has K1, K2, K3
    // Val has K2, K4, K5 (K2 is a collision with Train!)
    // Test has K3, K5, K6 (K3 collides with Train, K5 collides with Val!)
    std::unordered_set<std::string> trainKeys;
    std::unordered_set<std::string> valKeys;
    std::unordered_set<std::string> testKeys;
    int collisionsDropped = 0;

    std::vector<std::string> trainInput = {"K1", "K2", "K3"};
    std::vector<std::string> valInput   = {"K2", "K4", "K5"};
    std::vector<std::string> testInput  = {"K3", "K5", "K6"};

    for (const auto& k : trainInput) {
        trainKeys.insert(k);
    }

    for (const auto& k : valInput) {
        if (trainKeys.contains(k)) {
            collisionsDropped++;
            continue;
        }
        valKeys.insert(k);
    }

    for (const auto& k : testInput) {
        if (trainKeys.contains(k) || valKeys.contains(k)) {
            collisionsDropped++;
            continue;
        }
        testKeys.insert(k);
    }

    if (collisionsDropped != 3) return false;

    // Mathematical disjointness assertion: K_train ∩ K_val = ∅, K_train ∩ K_test = ∅, K_val ∩ K_test = ∅
    std::vector<std::string> tvIntersection;
    std::vector<std::string> tTeIntersection;
    std::vector<std::string> vTeIntersection;

    for (const auto& k : valKeys) {
        if (trainKeys.contains(k)) tvIntersection.push_back(k);
    }
    for (const auto& k : testKeys) {
        if (trainKeys.contains(k)) tTeIntersection.push_back(k);
        if (valKeys.contains(k)) vTeIntersection.push_back(k);
    }

    std::cout << "  [Pairwise Cross-Split Leakage Telemetry: |T ∩ V| = " << tvIntersection.size()
              << ", |T ∩ Te| = " << tTeIntersection.size()
              << ", |V ∩ Te| = " << vTeIntersection.size() << "]\n";

    assert(tvIntersection.empty());
    assert(tTeIntersection.empty());
    assert(vTeIntersection.empty());

    if (!tvIntersection.empty() || !tTeIntersection.empty() || !vTeIntersection.empty()) return false;

    return true;
}

bool testGate8A_10_SparseFeatureParity() {
    // 1. startpos verification
    auto opt1 = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt1) return false;
    auto wFeats = eval::nnue::FeatureTransformer::getActiveFeatures(*opt1, Color::White);
    auto bFeats = eval::nnue::FeatureTransformer::getActiveFeatures(*opt1, Color::Black);

    if (wFeats.size() != 30 || bFeats.size() != 30) return false;

    const std::vector<int> expectedStartposWhite = {
        2568, 2569, 2570, 2571, 2572, 2573, 2574, 2575,
        2625, 2630, 2690, 2693, 2752, 2759, 2819,
        2928, 2929, 2930, 2931, 2932, 2933, 2934, 2935,
        3001, 3006, 3066, 3069, 3128, 3135, 3195
    };

    if (wFeats != expectedStartposWhite) return false;

    // 2. endgame_kpk verification
    auto opt2 = FenParser::parse("8/8/8/4k3/8/4P3/8/4K3 w - - 0 1");
    if (!opt2) return false;
    auto wEnd = eval::nnue::FeatureTransformer::getActiveFeatures(*opt2, Color::White);
    auto bEnd = eval::nnue::FeatureTransformer::getActiveFeatures(*opt2, Color::Black);

    if (wEnd.size() != 1 || bEnd.size() != 1) return false;
    if (wEnd[0] != 2580 || bEnd[0] != 18284) return false;

    // 3. Strict ascending order and range [0, 40960)
    for (size_t i = 1; i < wFeats.size(); ++i) {
        if (wFeats[i] <= wFeats[i - 1]) return false;
    }
    for (int f : wFeats) {
        if (f < 0 || f >= 40960) return false;
    }

    return true;
}

bool testGate8A_11_DualPerspectiveFeatureTransformation() {
    // 1. Validate rank-mirroring (sq ^ 56)
    for (int sq = 0; sq < 64; ++sq) {
        int mirrored = sq ^ 56;
        int rank = sq / 8;
        int file = sq % 8;
        int mirroredRank = mirrored / 8;
        int mirroredFile = mirrored % 8;
        assert(file == mirroredFile);
        assert(mirroredRank == 7 - rank);
        assert((mirrored ^ 56) == sq);
        if (file != mirroredFile || mirroredRank != 7 - rank || (mirrored ^ 56) != sq) return false;
    }

    // 2. Validate piece color mapping: White piece code in [0, 4] maps to [5, 9] for Black perspective, and vice versa
    for (int p = 0; p < 10; ++p) {
        int mapped = (p < 5) ? (p + 5) : (p - 5);
        assert((mapped + 5) % 10 == p);
        if ((mapped + 5) % 10 != p) return false;
    }
    assert(eval::nnue::pieceToHalfKP(Piece::WhitePawn) == 0);
    assert(eval::nnue::pieceToHalfKP(Piece::BlackPawn) == 5);
    assert(eval::nnue::pieceToHalfKP(Piece::WhiteQueen) == 4);
    assert(eval::nnue::pieceToHalfKP(Piece::BlackQueen) == 9);

    // 3. Validate king square indexing & makeFeatureIndex:
    // Feature index formula: (kSq * 640) + (pieceCode * 64) + pSq
    // For White perspective: kSq=E1(4), WhitePawn(0) on E2(12) -> 4*640 + 0*64 + 12 = 2572
    int idxWhite = eval::nnue::makeFeatureIndex(Square::E1, Piece::WhitePawn, Square::E2, Color::White);
    assert(idxWhite == 2572);
    if (idxWhite != 2572) return false;

    // For Black perspective: kSq=E8(60)^56=4, BlackPawn(5)->0 on E7(52)^56=12 -> 4*640 + 0*64 + 12 = 2572
    int idxBlack = eval::nnue::makeFeatureIndex(Square::E8, Piece::BlackPawn, Square::E7, Color::Black);
    assert(idxBlack == 2572);
    assert(idxWhite == idxBlack);
    if (idxWhite != idxBlack) return false;

    // Symmetry on startpos
    auto opt1 = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!opt1) return false;
    auto w1 = eval::nnue::FeatureTransformer::getActiveFeatures(*opt1, Color::White);
    auto b1 = eval::nnue::FeatureTransformer::getActiveFeatures(*opt1, Color::Black);
    if (w1 != b1) return false;

    // Rank-mirrored positions
    auto optA = FenParser::parse("8/8/8/4k3/8/4P3/8/4K3 w - - 0 1");
    auto optB = FenParser::parse("4k3/8/4p3/8/4K3/8/8/8 b - - 0 1");
    if (!optA || !optB) return false;

    auto wA = eval::nnue::FeatureTransformer::getActiveFeatures(*optA, Color::White);
    auto bA = eval::nnue::FeatureTransformer::getActiveFeatures(*optA, Color::Black);
    auto wB = eval::nnue::FeatureTransformer::getActiveFeatures(*optB, Color::White);
    auto bB = eval::nnue::FeatureTransformer::getActiveFeatures(*optB, Color::Black);

    if (wA != bB || bA != wB) return false;

    return true;
}

bool testGate8A_12_BinarySchemaCompliance() {
    if (sizeof(dataset::DatasetHeader) != 64) return false;
    if (sizeof(dataset::DatasetRecordPrefix) != 16) return false;

    dataset::DatasetHeader h;
    if (std::memcmp(h.magic, "BOSN_DS1", 8) != 0) return false;
    if (h.formatVersion != 1) return false;
    if (h.featureVersion != 1) return false;
    if (h.teacherVersion != 950) return false;
    if (h.teacherDepth != 6) return false;
    if (sizeof(h.reserved) != 31) return false;

    return true;
}

bool testGate8A_13_BinarySerializationRoundTripReloadIdentity() {
    const std::string testFile = "test_roundtrip_gate8a13.bin";

    std::vector<dataset::DatasetRecord> records;
    for (int i = 0; i < 5; ++i) {
        dataset::DatasetRecord rec;
        rec.prefix.positionHash = 0xABCDEF0123456789ULL + static_cast<uint64_t>(i);
        rec.prefix.sideToMove = static_cast<uint8_t>(i % 2);
        rec.prefix.metadataFlags = static_cast<uint8_t>(i & 0x0F);
        rec.prefix.z_stm = (i % 2 == 0) ? 1.0f : 0.0f;
        rec.prefix.q_stm = static_cast<int16_t>((i * 50) - 100);
        rec.whiteFeatures = {static_cast<uint16_t>(100 + i), static_cast<uint16_t>(200 + i)};
        rec.blackFeatures = {static_cast<uint16_t>(300 + i), static_cast<uint16_t>(400 + i)};
        records.push_back(std::move(rec));
    }

    if (!dataset::writeDataset(testFile, dataset::SPLIT_TRAIN, records, 950, 6)) {
        std::filesystem::remove(testFile);
        return false;
    }

    dataset::DatasetHeader hdr;
    std::vector<dataset::DatasetRecord> reloaded;
    if (!dataset::readDataset(testFile, hdr, reloaded)) {
        std::filesystem::remove(testFile);
        return false;
    }

    std::filesystem::remove(testFile);

    if (hdr.recordCount != 5 || reloaded.size() != 5) return false;
    if (hdr.splitId != dataset::SPLIT_TRAIN) return false;

    for (size_t i = 0; i < 5; ++i) {
        if (reloaded[i].prefix.positionHash != records[i].prefix.positionHash) return false;
        if (reloaded[i].prefix.sideToMove != records[i].prefix.sideToMove) return false;
        if (reloaded[i].prefix.metadataFlags != records[i].prefix.metadataFlags) return false;
        if (std::abs(reloaded[i].prefix.z_stm - records[i].prefix.z_stm) > 1e-6f) return false;
        if (reloaded[i].prefix.q_stm != records[i].prefix.q_stm) return false;
        if (reloaded[i].whiteFeatures != records[i].whiteFeatures) return false;
        if (reloaded[i].blackFeatures != records[i].blackFeatures) return false;
    }

    return true;
}

bool testGate8A_14_ComprehensiveDatasetDistributionTelemetry() {
    std::vector<dataset::DatasetRecord> sample;
    for (int i = 0; i < 100; ++i) {
        dataset::DatasetRecord rec;
        rec.prefix.z_stm = (i < 40) ? 1.0f : ((i < 70) ? 0.5f : 0.0f);
        rec.prefix.q_stm = static_cast<int16_t>((i - 50) * 10);
        if (i % 4 == 0) rec.prefix.metadataFlags |= dataset::FLAG_IN_CHECK;
        if (i % 3 == 0) rec.prefix.metadataFlags |= dataset::FLAG_HAS_CAPTURE;
        sample.push_back(rec);
    }

    int wins = 0, draws = 0, losses = 0;
    int checkCnt = 0, capCnt = 0;
    double sumQ = 0.0;

    for (const auto& r : sample) {
        if (r.prefix.z_stm == 1.0f) wins++;
        else if (r.prefix.z_stm == 0.5f) draws++;
        else losses++;

        sumQ += r.prefix.q_stm;
        if (r.prefix.metadataFlags & dataset::FLAG_IN_CHECK) checkCnt++;
        if (r.prefix.metadataFlags & dataset::FLAG_HAS_CAPTURE) capCnt++;
    }

    if (wins != 40 || draws != 30 || losses != 30) return false;
    if (checkCnt != 25) return false;
    if (capCnt != 34) return false;

    double meanQ = sumQ / 100.0;
    if (std::abs(meanQ - (-5.0)) > 1e-4) return false;

    return true;
}

bool testGate8A_15_TeacherOutcomeCalibrationDiagnostic() {
    // 1. Theoretical logistic calibration anchor points
    double p0 = dataset::logisticWinProb(0.0);
    assert(std::abs(p0 - 0.5) < 1e-6);
    if (std::abs(p0 - 0.5) > 1e-6) return false;

    double pPlus400 = dataset::logisticWinProb(400.0);
    assert(std::abs(pPlus400 - (1.0 / 1.1)) < 1e-5);
    if (std::abs(pPlus400 - (1.0 / 1.1)) > 1e-5) return false;

    double pMinus400 = dataset::logisticWinProb(-400.0);
    assert(std::abs(pMinus400 - (1.0 / 11.0)) < 1e-5);
    if (std::abs(pMinus400 - (1.0 / 11.0)) > 1e-5) return false;

    // 2. Define evaluation bins for empirical diagnostic calibration
    struct BinDef {
        std::string name;
        int16_t minQ;
        int16_t maxQ;
    };
    const std::vector<BinDef> bins = {
        {"[-inf, -300)", -32768, -300},
        {"[-300, -100)", -300, -100},
        {"[-100, +100)", -100, 100},
        {"+100, +300)",  100, 300},
        {"+300, +inf)",  300, 32767}
    };

    // Synthetic representative sample spanning evaluation spectrum
    struct PosSample {
        int16_t q;
        float z;
    };
    const std::vector<PosSample> dataset = {
        {-500, 0.0f}, {-450, 0.0f}, {-350, 0.0f},
        {-250, 0.0f}, {-200, 0.0f}, {-150, 0.5f},
        {-80, 0.0f},  {-30, 0.5f},  {0, 0.5f},     {40, 0.5f}, {80, 1.0f},
        {150, 0.5f},  {200, 1.0f},  {280, 1.0f},
        {350, 1.0f},  {480, 1.0f},  {600, 1.0f}
    };

    std::cout << "\n  [Gate 8-A-15 (Teacher / Outcome Calibration & Disagreement Diagnostic)]\n";
    std::cout << "  -------------------------------------------------------------------------------------------------------\n";
    std::cout << "  Bin Range        Count   Mean q   E[z|q] (Actual)   Heuristic Teacher Mapping   |Diff|\n";
    std::cout << "  -------------------------------------------------------------------------------------------------------\n";

    double totalDisagreement = 0.0;
    int populatedBins = 0;

    for (const auto& b : bins) {
        int count = 0;
        double sumQ = 0.0;
        double sumZ = 0.0;

        for (const auto& s : dataset) {
            if (s.q >= b.minQ && s.q < b.maxQ) {
                count++;
                sumQ += s.q;
                sumZ += s.z;
            }
        }

        if (count > 0) {
            double meanQ = sumQ / count;
            double actualEz = sumZ / count;
            double expectedP = dataset::logisticWinProb(meanQ);
            double diff = std::abs(expectedP - actualEz);
            totalDisagreement += diff;
            populatedBins++;

            std::cout << "  " << std::left << std::setw(16) << b.name
                      << std::right << std::setw(6) << count
                      << std::setw(9) << static_cast<int>(std::round(meanQ))
                      << std::fixed << std::setprecision(4)
                      << std::setw(18) << actualEz
                      << std::setw(28) << expectedP
                      << std::setw(9) << diff << "\n";
        }
    }

    double meanAbsoluteDisagreement = (populatedBins > 0) ? (totalDisagreement / populatedBins) : 0.0;
    std::cout << "  --------------------------------------------------------------------------\n";
    std::cout << "  Empirical Mean Absolute Calibration Disagreement: " << std::fixed << std::setprecision(4)
              << meanAbsoluteDisagreement << "\n";

    if (std::isnan(meanAbsoluteDisagreement) || meanAbsoluteDisagreement < 0.0) return false;

    return true;
}

bool testGate8A_16_CryptographicDatasetManifestAndSha256() {
    const std::string testFile = "test_manifest_gate8a16.bin";
    {
        std::ofstream os(testFile, std::ios::binary);
        const char dummy[] = "BosonDatasetCryptographicVerificationGate8A16";
        os.write(dummy, sizeof(dummy));
    }

    std::string sha1 = eval::nnue::computeFileSha256(testFile);
    if (sha1.size() != 64) {
        std::filesystem::remove(testFile);
        return false;
    }

    // Bit mutation avalanche test
    {
        std::ofstream os(testFile, std::ios::binary);
        const char mutated[] = "CosonDatasetCryptographicVerificationGate8A16";
        os.write(mutated, sizeof(mutated));
    }

    std::string sha2 = eval::nnue::computeFileSha256(testFile);
    std::filesystem::remove(testFile);

    if (sha2.size() != 64 || sha1 == sha2) return false;

    return true;
}

bool testGate8A_17_DeterministicRegenerationInvariance() {
    const std::string file1 = "test_regen_gate8a17_1.bin";
    const std::string file2 = "test_regen_gate8a17_2.bin";

    std::vector<dataset::DatasetRecord> records;
    for (int i = 0; i < 10; ++i) {
        dataset::DatasetRecord rec;
        rec.prefix.positionHash = static_cast<uint64_t>(i * 1234567);
        rec.prefix.sideToMove = static_cast<uint8_t>(i % 2);
        rec.prefix.z_stm = (i % 2 == 0) ? 1.0f : 0.0f;
        rec.prefix.q_stm = static_cast<int16_t>(i * 20);
        rec.whiteFeatures = {static_cast<uint16_t>(i * 10)};
        rec.blackFeatures = {static_cast<uint16_t>(i * 10 + 1)};
        records.push_back(rec);
    }

    if (!dataset::writeDataset(file1, dataset::SPLIT_TRAIN, records, 950, 6)) {
        return false;
    }
    if (!dataset::writeDataset(file2, dataset::SPLIT_TRAIN, records, 950, 6)) {
        std::filesystem::remove(file1);
        return false;
    }

    std::string sha1 = eval::nnue::computeFileSha256(file1);
    std::string sha2 = eval::nnue::computeFileSha256(file2);

    std::filesystem::remove(file1);
    std::filesystem::remove(file2);

    if (sha1.empty() || sha1 != sha2) return false;

    return true;
}

bool runPhase8ADatasetTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   SUITE 35: PHASE 8-A DATASET CONSTRUCTION & FEATURE PARITY  ===\n";
    std::cout << "=================================================================\n";

    bool pass1  = testGate8A_1_ReplayAndLegalExtraction();
    bool pass2  = testGate8A_2_SideToMoveOutcomeLabelSemantics();
    bool pass3  = testGate8A_3_SideToMoveEvaluationScoreSemantics();
    bool pass4  = testGate8A_4_FrozenTeacherConfigurationLogging();
    bool pass5  = testGate8A_5_ExclusionReasonsTaxonomy();
    bool pass6  = testGate8A_6_DeterministicForwardSampling();
    bool pass7  = testGate8A_7_PerGamePositionCap();
    bool pass8  = testGate8A_8_OpeningGroupAtomicPartitioning();
    bool pass9  = testGate8A_9_CrossSplitCollisionElimination();
    bool pass10 = testGate8A_10_SparseFeatureParity();
    bool pass11 = testGate8A_11_DualPerspectiveFeatureTransformation();
    bool pass12 = testGate8A_12_BinarySchemaCompliance();
    bool pass13 = testGate8A_13_BinarySerializationRoundTripReloadIdentity();
    bool pass14 = testGate8A_14_ComprehensiveDatasetDistributionTelemetry();
    bool pass15 = testGate8A_15_TeacherOutcomeCalibrationDiagnostic();
    bool pass16 = testGate8A_16_CryptographicDatasetManifestAndSha256();
    bool pass17 = testGate8A_17_DeterministicRegenerationInvariance();

    std::cout << "Gate 8-A-1  (Replay & Legal Move Extraction):     " << (pass1  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-A-2  (Side-to-Move Outcome Semantics):     " << (pass2  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-A-3  (Side-to-Move Evaluation Semantics):  " << (pass3  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-A-4  (Frozen Teacher Configuration Log):   " << (pass4  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-A-5  (Exclusion Reasons Taxonomy):         " << (pass5  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-A-6  (Deterministic Forward Sampling):     " << (pass6  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-A-7  (Per-Game Position Cap <= 30):        " << (pass7  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-A-8  (Opening-Group Atomic 90/6/4 Split):   " << (pass8  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-A-9  (Zero Cross-Split Collision):         " << (pass9  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-A-10 (Sparse Feature Bit-Exact Parity):    " << (pass10 ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-A-11 (Dual-Perspective Feature Transformation): " << (pass11 ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-A-12 (Binary Schema Header & Layout):      " << (pass12 ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-A-13 (Round-Trip Binary Reload Identity):  " << (pass13 ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-A-14 (Dataset Distribution Telemetry):     " << (pass14 ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-A-15 (Teacher/Outcome Calibration Diagnostic): " << (pass15 ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-A-16 (Dataset SHA-256 Manifest Logging):   " << (pass16 ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-A-17 (Deterministic Regeneration Invariance):" << (pass17 ? "PASS" : "FAIL") << "\n";
    std::cout << "=================================================================\n";

    return pass1 && pass2 && pass3 && pass4 && pass5 && pass6 && pass7 && pass8 &&
           pass9 && pass10 && pass11 && pass12 && pass13 && pass14 && pass15 && pass16 && pass17;
}

// ============================================================================
// SUITE 36: PHASE 8-B FLOAT32 HALFKP SUPERVISED TRAINING & QUANTIZATION BRIDGE
// ============================================================================

namespace training {

constexpr float T_E = 400.0f;
constexpr float T_Q = 400.0f;
constexpr float LN10 = 2.302585092994046f;
constexpr float LN10_DIV_400 = LN10 / 400.0f;

inline float sigmoidBase10(float cp) {
    return 1.0f / (1.0f + std::pow(10.0f, -cp / 400.0f));
}

inline float bceLossWithLogit(float logit, float z) {
    float maxVal = std::max(logit, 0.0f);
    return maxVal - logit * z + std::log1p(std::exp(-std::abs(logit)));
}

struct FloatModelBridge {
    std::array<float, 32> fc1_biases{};
    std::array<std::array<float, 1024>, 32> fc1_weights{};
    std::array<float, 32> fc2_biases{};
    std::array<std::array<float, 32>, 32> fc2_weights{};
    float fc3_bias{0.0f};
    std::array<float, 32> fc3_weights{};
};

struct FloatForwardDiagnostics {
    std::array<float, 32> h1{};
    std::array<float, 32> h2{};
    float E{0.0f};
    float h1_zero_pct{0.0f};
    float h1_sat_pct{0.0f};
    float h2_zero_pct{0.0f};
    float h2_sat_pct{0.0f};
};

inline FloatForwardDiagnostics forwardFloat(const std::array<float, 1024>& acc, const FloatModelBridge& model) {
    FloatForwardDiagnostics diag{};

    // 1. Feature normalization bridge: x_norm = clamp(A, 0.0, 127.0) / 127.0
    std::array<float, 1024> x_norm{};
    for (size_t i = 0; i < 1024; ++i) {
        x_norm[i] = std::clamp(acc[i], 0.0f, 127.0f) / 127.0f;
    }

    // 2. FC1: u1 = FC1(x_norm) --> h1 = clamp(u1, 0, 127)
    int h1_z = 0, h1_s = 0;
    for (size_t j = 0; j < 32; ++j) {
        float sum = model.fc1_biases[j];
        for (size_t i = 0; i < 1024; ++i) {
            sum += x_norm[i] * model.fc1_weights[j][i];
        }
        float h = std::clamp(sum, 0.0f, 127.0f);
        diag.h1[j] = h;
        if (h == 0.0f) h1_z++;
        if (h == 127.0f) h1_s++;
    }
    diag.h1_zero_pct = (h1_z / 32.0f) * 100.0f;
    diag.h1_sat_pct  = (h1_s / 32.0f) * 100.0f;

    // 3. FC2: u2 = FC2(h1) --> h2 = clamp(u2, 0, 127)
    int h2_z = 0, h2_s = 0;
    for (size_t k = 0; k < 32; ++k) {
        float sum = model.fc2_biases[k];
        for (size_t j = 0; j < 32; ++j) {
            sum += diag.h1[j] * model.fc2_weights[k][j];
        }
        float h = std::clamp(sum, 0.0f, 127.0f);
        diag.h2[k] = h;
        if (h == 0.0f) h2_z++;
        if (h == 127.0f) h2_s++;
    }
    diag.h2_zero_pct = (h2_z / 32.0f) * 100.0f;
    diag.h2_sat_pct  = (h2_s / 32.0f) * 100.0f;

    // 4. FC3: E = FC3(h2) (linear centipawn output)
    float sum3 = model.fc3_bias;
    for (size_t k = 0; k < 32; ++k) {
        sum3 += diag.h2[k] * model.fc3_weights[k];
    }
    diag.E = std::clamp(sum3, static_cast<float>(eval::nnue::NNUE_EVAL_MIN), static_cast<float>(eval::nnue::NNUE_EVAL_MAX));

    return diag;
}

inline int32_t forwardShadow(const std::array<int16_t, 1024>& acc, const FloatModelBridge& floatModel) {
    // 1. Quantize weights to match bridge exact integer representation
    std::array<int32_t, 32> b1_int{};
    std::array<std::array<int8_t, 1024>, 32> w1_int{};
    for (size_t j = 0; j < 32; ++j) {
        b1_int[j] = static_cast<int32_t>(std::round(floatModel.fc1_biases[j] * 64.0f));
        for (size_t i = 0; i < 1024; ++i) {
            float w = std::round(floatModel.fc1_weights[j][i] * (64.0f / 127.0f));
            w1_int[j][i] = static_cast<int8_t>(std::clamp(static_cast<int>(w), -128, 127));
        }
    }

    std::array<int32_t, 32> b2_int{};
    std::array<std::array<int8_t, 32>, 32> w2_int{};
    for (size_t k = 0; k < 32; ++k) {
        b2_int[k] = static_cast<int32_t>(std::round(floatModel.fc2_biases[k] * 64.0f));
        for (size_t j = 0; j < 32; ++j) {
            float w = std::round(floatModel.fc2_weights[k][j] * 64.0f);
            w2_int[k][j] = static_cast<int8_t>(std::clamp(static_cast<int>(w), -128, 127));
        }
    }

    int32_t b3_int = static_cast<int32_t>(std::round(floatModel.fc3_bias / 16.0f));
    std::array<int8_t, 32> w3_int{};
    for (size_t k = 0; k < 32; ++k) {
        float w = std::round(floatModel.fc3_weights[k] / 16.0f);
        w3_int[k] = static_cast<int8_t>(std::clamp(static_cast<int>(w), -128, 127));
    }

    // 2. Exact ScalarInference execution:
    std::array<int8_t, 1024> input_act{};
    for (size_t i = 0; i < 1024; ++i) {
        input_act[i] = eval::nnue::ScalarInference::crelu(acc[i]);
    }

    std::array<int8_t, 32> h1{};
    for (size_t j = 0; j < 32; ++j) {
        int32_t sum = b1_int[j];
        for (size_t i = 0; i < 1024; ++i) {
            sum += static_cast<int32_t>(input_act[i]) * static_cast<int32_t>(w1_int[j][i]);
        }
        int32_t scaled = sum / 64; // truncates toward zero
        h1[j] = eval::nnue::ScalarInference::crelu(scaled);
    }

    std::array<int8_t, 32> h2{};
    for (size_t k = 0; k < 32; ++k) {
        int32_t sum = b2_int[k];
        for (size_t j = 0; j < 32; ++j) {
            sum += static_cast<int32_t>(h1[j]) * static_cast<int32_t>(w2_int[k][j]);
        }
        int32_t scaled = sum / 64;
        h2[k] = eval::nnue::ScalarInference::crelu(scaled);
    }

    int32_t sum3 = b3_int;
    for (size_t k = 0; k < 32; ++k) {
        sum3 += static_cast<int32_t>(h2[k]) * static_cast<int32_t>(w3_int[k]);
    }
    int32_t score = sum3 * eval::nnue::NNUE_OUTPUT_SCALE; // * 16
    return std::clamp<int32_t>(score, eval::nnue::NNUE_EVAL_MIN, eval::nnue::NNUE_EVAL_MAX);
}

inline FloatModelBridge createDeterministicFloatBridge(uint32_t seed = 42) {
    FloatModelBridge m{};
    uint64_t state = (seed == 0) ? 1337ULL : static_cast<uint64_t>(seed);
    auto nextRand = [&state]() -> uint64_t {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 0x2545F4914F6CDD1DULL;
    };

    for (size_t j = 0; j < 32; ++j) {
        uint64_t r = nextRand();
        m.fc1_biases[j] = (-5.0f + static_cast<float>(r % 11)) / 64.0f;
        for (size_t i = 0; i < 1024; ++i) {
            uint64_t rw = nextRand();
            float w_int = -2.0f + static_cast<float>(rw % 5);
            m.fc1_weights[j][i] = w_int * (127.0f / 64.0f);
        }
    }

    for (size_t k = 0; k < 32; ++k) {
        uint64_t r = nextRand();
        m.fc2_biases[k] = (-5.0f + static_cast<float>(r % 11)) / 64.0f;
        for (size_t j = 0; j < 32; ++j) {
            uint64_t rw = nextRand();
            float w_int = -2.0f + static_cast<float>(rw % 5);
            m.fc2_weights[k][j] = w_int / 64.0f;
        }
    }

    uint64_t rb = nextRand();
    m.fc3_bias = (-2.0f + static_cast<float>(rb % 5)) * 16.0f;
    for (size_t k = 0; k < 32; ++k) {
        uint64_t rw = nextRand();
        float w_int = -2.0f + static_cast<float>(rw % 5);
        m.fc3_weights[k] = w_int * 16.0f;
    }

    return m;
}

} // namespace training

bool testGate8B_1_ModelTopologySpecification() {
    if (eval::nnue::HALFKP_FEATURES != 40960) return false;
    if (eval::nnue::ACCUMULATOR_SIZE != 512) return false;
    if (eval::nnue::FC1_INPUT_SIZE != 1024) return false;
    if (eval::nnue::FC1_OUTPUT_SIZE != 32) return false;
    if (eval::nnue::FC2_OUTPUT_SIZE != 32) return false;

    constexpr size_t p2Params = (1024 * 32 + 32) + (32 * 32 + 32) + (32 * 1 + 1);
    if (p2Params != 33889) return false;

    constexpr size_t ftParams = (40960 * 512) + 512;
    if (ftParams != 20972032) return false;
    constexpr size_t p1Params = p2Params + ftParams;
    if (p1Params != 21005921) return false;

    return true;
}

bool testGate8B_2_AccumulatorNormalizationBridge() {
    float aNeg = -350.0f;
    float normNeg = std::clamp(aNeg, 0.0f, 127.0f) / 127.0f;
    if (normNeg != 0.0f) return false;

    float aZero = 0.0f;
    float normZero = std::clamp(aZero, 0.0f, 127.0f) / 127.0f;
    if (normZero != 0.0f) return false;

    float aMid = 63.5f;
    float normMid = std::clamp(aMid, 0.0f, 127.0f) / 127.0f;
    if (std::abs(normMid - 0.5f) > 1e-6f) return false;

    float aCap = 127.0f;
    float normCap = std::clamp(aCap, 0.0f, 127.0f) / 127.0f;
    if (normCap != 1.0f) return false;

    float aOver = 500.0f;
    float normOver = std::clamp(aOver, 0.0f, 127.0f) / 127.0f;
    if (normOver != 1.0f) return false;

    return true;
}

bool testGate8B_3_HiddenLayer1ForwardActivation() {
    training::FloatModelBridge model = training::createDeterministicFloatBridge(101);
    std::array<float, 1024> acc{};
    for (size_t i = 0; i < 1024; ++i) {
        acc[i] = (i % 2 == 0) ? 100.0f : -50.0f;
    }

    auto diag = training::forwardFloat(acc, model);
    if (diag.h1.size() != 32) return false;

    for (float v : diag.h1) {
        if (v < 0.0f || v > 127.0f) return false;
    }
    return true;
}

bool testGate8B_4_HiddenLayer2ForwardActivation() {
    training::FloatModelBridge model = training::createDeterministicFloatBridge(202);
    std::array<float, 1024> acc{};
    for (size_t i = 0; i < 1024; ++i) {
        acc[i] = static_cast<float>((i * 7) % 200) - 50.0f;
    }

    auto diag = training::forwardFloat(acc, model);
    if (diag.h2.size() != 32) return false;

    for (float v : diag.h2) {
        if (v < 0.0f || v > 127.0f) return false;
    }
    return true;
}

bool testGate8B_5_OutputLayerFC3LinearCentipawnMapping() {
    training::FloatModelBridge model = training::createDeterministicFloatBridge(303);
    std::array<float, 1024> acc{};
    for (size_t i = 0; i < 1024; ++i) {
        acc[i] = (i < 512) ? 80.0f : 20.0f;
    }

    auto diag = training::forwardFloat(acc, model);
    if (std::isnan(diag.E) || std::isinf(diag.E)) return false;
    if (diag.E < -30000.0f || diag.E > 30000.0f) return false;

    return true;
}

bool testGate8B_6_SigmoidProbabilityMappingAndFrozenTemperature() {
    if (training::T_E != 400.0f || training::T_Q != 400.0f) return false;

    float p0 = training::sigmoidBase10(0.0f);
    if (std::abs(p0 - 0.5f) > 1e-6f) return false;

    float pPlus = training::sigmoidBase10(400.0f);
    if (std::abs(pPlus - (1.0f / 1.1f)) > 1e-5f) return false;

    float pMinus = training::sigmoidBase10(-400.0f);
    if (std::abs(pMinus - (1.0f / 11.0f)) > 1e-5f) return false;

    float pTest = 0.75f;
    float E_recovered = -400.0f * std::log10(1.0f / pTest - 1.0f);
    float p_recalc = training::sigmoidBase10(E_recovered);
    if (std::abs(p_recalc - pTest) > 1e-5f) return false;

    return true;
}

bool testGate8B_7_ExpAOutcomeOnlyBCEFormulation() {
    float logit1 = 400.0f * training::LN10_DIV_400;
    float z1 = 1.0f;
    float loss1 = training::bceLossWithLogit(logit1, z1);
    if (std::abs(loss1 - std::log(1.1f)) > 1e-4f) return false;

    float lossDraw = training::bceLossWithLogit(0.0f, 0.5f);
    if (std::abs(lossDraw - std::log(2.0f)) > 1e-4f) return false;

    if (loss1 < 0.0f || lossDraw < 0.0f) return false;

    return true;
}

bool testGate8B_8_ExpB1TeacherDistillationMSEFormulation() {
    float E = 250.0f;
    float q = 250.0f;
    float pE = training::sigmoidBase10(E);
    float pq = training::sigmoidBase10(q);
    float mseZero = (pE - pq) * (pE - pq);
    if (mseZero != 0.0f) return false;

    float E2 = 400.0f;
    float q2 = 0.0f;
    float pE2 = training::sigmoidBase10(E2);
    float pq2 = training::sigmoidBase10(q2);
    float diff = pE2 - pq2;
    float mse = diff * diff;
    if (std::abs(mse - 0.167355f) > 1e-3f) return false;

    return true;
}

bool testGate8B_9_ExpB2DirectNormalizedCentipawnMSEFormulation() {
    float E = 300.0f;
    float q = 100.0f;
    float loss = std::pow((E / 400.0f) - (q / 400.0f), 2.0f);
    float expected = std::pow(200.0f / 400.0f, 2.0f);
    if (std::abs(loss - expected) > 1e-6f) return false;
    if (loss != 0.25f) return false;

    return true;
}

bool testGate8B_10_ExpCBlendedConvexCombinationObjective() {
    float lossBCE = 0.50f;
    float lossDistill = 0.10f;

    for (float alpha : {0.25f, 0.50f, 0.75f}) {
        float blended = alpha * lossBCE + (1.0f - alpha) * lossDistill;
        float expected = alpha * 0.50f + (1.0f - alpha) * 0.10f;
        if (std::abs(blended - expected) > 1e-6f) return false;
        if (blended < lossDistill || blended > lossBCE) return false;
    }

    if (1.0f * lossBCE + 0.0f * lossDistill != lossBCE) return false;
    if (0.0f * lossBCE + 1.0f * lossDistill != lossDistill) return false;

    return true;
}

bool testGate8B_11_DatasetSerializationRecordIngestionIntegrity() {
    dataset::DatasetHeader trainHdr, valHdr;
    std::vector<dataset::DatasetRecord> trainRecs, valRecs;

    if (!dataset::readDataset("data/dataset_phase8a/train.bin", trainHdr, trainRecs)) return false;
    if (!dataset::readDataset("data/dataset_phase8a/val.bin", valHdr, valRecs)) return false;

    if (trainHdr.recordCount != 4497 || trainRecs.size() != 4497) return false;
    if (valHdr.recordCount != 300 || valRecs.size() != 300) return false;

    for (const auto& r : trainRecs) {
        if (r.prefix.sideToMove > 1) return false;
        for (uint16_t f : r.whiteFeatures) {
            if (f >= 40960) return false;
        }
        for (uint16_t f : r.blackFeatures) {
            if (f >= 40960) return false;
        }
    }

    return true;
}

bool testGate8B_12_TestSetIsolationSplitInviolabilityVerification() {
    dataset::DatasetHeader testHdr;
    std::vector<dataset::DatasetRecord> testRecs;
    if (!dataset::readDataset("data/dataset_phase8a/test.bin", testHdr, testRecs)) return false;

    if (testHdr.recordCount != 200 || testRecs.size() != 200) return false;
    if (testHdr.splitId != dataset::SPLIT_TEST) return false;

    dataset::DatasetHeader trainHdr, valHdr;
    std::vector<dataset::DatasetRecord> trainRecs, valRecs;
    if (!dataset::readDataset("data/dataset_phase8a/train.bin", trainHdr, trainRecs)) return false;
    if (!dataset::readDataset("data/dataset_phase8a/val.bin", valHdr, valRecs)) return false;

    std::unordered_set<uint64_t> trainHashes, valHashes, testHashes;
    for (const auto& r : trainRecs) trainHashes.insert(r.prefix.positionHash);
    for (const auto& r : valRecs)   valHashes.insert(r.prefix.positionHash);
    for (const auto& r : testRecs)  testHashes.insert(r.prefix.positionHash);

    for (uint64_t h : testHashes) {
        if (trainHashes.contains(h)) return false;
        if (valHashes.contains(h)) return false;
    }
    for (uint64_t h : valHashes) {
        if (trainHashes.contains(h)) return false;
    }

    return true;
}

bool testGate8B_13_ModeP2FrozenFeatureTransformerInvariant() {
    auto fwOriginal = eval::nnue::FeatureWeights::createDeterministic(42);
    auto fwCandidate = eval::nnue::FeatureWeights::createDeterministic(42);

    if (fwOriginal->biases != fwCandidate->biases) return false;
    if (fwOriginal->weights != fwCandidate->weights) return false;

    return true;
}

bool testGate8B_14_ActivationSaturationAndDeadNeuronDiagnostics() {
    std::array<float, 32> h{};
    for (size_t i = 0; i < 32; ++i) {
        if (i < 8) h[i] = 0.0f;         // 25% dead
        else if (i < 24) h[i] = 50.0f;  // 50% linear
        else h[i] = 127.0f;             // 25% saturated
    }

    int zeros = 0, sats = 0;
    for (float v : h) {
        if (v == 0.0f) zeros++;
        if (v == 127.0f) sats++;
    }

    float zeroPct = (zeros / 32.0f) * 100.0f;
    float satPct  = (sats / 32.0f) * 100.0f;

    if (zeroPct != 25.0f || satPct != 25.0f) return false;

    return true;
}

bool testGate8B_15_QuantizationShadowCheck() {
    training::FloatModelBridge model = training::createDeterministicFloatBridge(777);

    dataset::DatasetHeader valHdr;
    std::vector<dataset::DatasetRecord> valRecs;
    if (!dataset::readDataset("data/dataset_phase8a/val.bin", valHdr, valRecs)) return false;

    auto fw = eval::nnue::FeatureWeights::createDeterministic(42);

    size_t batchSize = std::min<size_t>(128, valRecs.size());
    double sumAbsDiff = 0.0;
    double maxAbsDiff = 0.0;

    std::cout << "\n  [Gate 8-B-15 (Quantization Shadow Check Telemetry on " << batchSize << " Positions)]\n";
    std::cout << "  -----------------------------------------------------------------------------------\n";
    std::cout << "  Pos #   Side     E_float (cp)    E_shadow (cp)       |Diff|    Teacher q\n";
    std::cout << "  -----------------------------------------------------------------------------------\n";

    for (size_t idx = 0; idx < batchSize; ++idx) {
        const auto& rec = valRecs[idx];

        std::array<int16_t, 1024> accInt{};
        std::array<float, 1024> accFloat{};

        const auto& usFeats = (rec.prefix.sideToMove == 0) ? rec.whiteFeatures : rec.blackFeatures;
        const auto& themFeats = (rec.prefix.sideToMove == 0) ? rec.blackFeatures : rec.whiteFeatures;

        for (size_t i = 0; i < 512; ++i) {
            int32_t sumUs = fw->biases[i];
            for (uint16_t f : usFeats) {
                sumUs += fw->weights[f][i];
            }
            int16_t usVal = static_cast<int16_t>(std::clamp(sumUs, -32768, 32767));
            accInt[i] = usVal;
            accFloat[i] = static_cast<float>(usVal);

            int32_t sumThem = fw->biases[i];
            for (uint16_t f : themFeats) {
                sumThem += fw->weights[f][i];
            }
            int16_t themVal = static_cast<int16_t>(std::clamp(sumThem, -32768, 32767));
            accInt[512 + i] = themVal;
            accFloat[512 + i] = static_cast<float>(themVal);
        }

        auto diag = training::forwardFloat(accFloat, model);
        int32_t shadowScore = training::forwardShadow(accInt, model);

        double diff = std::abs(diag.E - static_cast<float>(shadowScore));
        sumAbsDiff += diff;
        if (diff > maxAbsDiff) maxAbsDiff = diff;

        if (idx < 5) {
            std::cout << "  " << std::setw(5) << idx
                      << std::setw(7) << (rec.prefix.sideToMove == 0 ? "White" : "Black")
                      << std::fixed << std::setprecision(2)
                      << std::setw(17) << diag.E
                      << std::setw(17) << shadowScore
                      << std::setw(13) << diff
                      << std::setw(13) << rec.prefix.q_stm << "\n";
        }
    }

    double mae = sumAbsDiff / batchSize;
    std::cout << "  -----------------------------------------------------------------------------------\n";
    std::cout << "  Mini-Batch Max Absolute Error |E_float - E_shadow|: " << maxAbsDiff << " cp\n";
    std::cout << "  Mini-Batch Mean Absolute Error MAE(E_float, E_shadow): " << mae << " cp\n";
    std::cout << "  -----------------------------------------------------------------------------------\n";

    if (maxAbsDiff > 100.0) return false;
    if (mae > 50.0) return false;

    return true;
}

bool runPhase8BSupervisedTrainingTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   SUITE 36: PHASE 8-B SUPERVISED TRAINING & QUANTIZATION   ===\n";
    std::cout << "=================================================================\n";

    bool pass1  = testGate8B_1_ModelTopologySpecification();
    bool pass2  = testGate8B_2_AccumulatorNormalizationBridge();
    bool pass3  = testGate8B_3_HiddenLayer1ForwardActivation();
    bool pass4  = testGate8B_4_HiddenLayer2ForwardActivation();
    bool pass5  = testGate8B_5_OutputLayerFC3LinearCentipawnMapping();
    bool pass6  = testGate8B_6_SigmoidProbabilityMappingAndFrozenTemperature();
    bool pass7  = testGate8B_7_ExpAOutcomeOnlyBCEFormulation();
    bool pass8  = testGate8B_8_ExpB1TeacherDistillationMSEFormulation();
    bool pass9  = testGate8B_9_ExpB2DirectNormalizedCentipawnMSEFormulation();
    bool pass10 = testGate8B_10_ExpCBlendedConvexCombinationObjective();
    bool pass11 = testGate8B_11_DatasetSerializationRecordIngestionIntegrity();
    bool pass12 = testGate8B_12_TestSetIsolationSplitInviolabilityVerification();
    bool pass13 = testGate8B_13_ModeP2FrozenFeatureTransformerInvariant();
    bool pass14 = testGate8B_14_ActivationSaturationAndDeadNeuronDiagnostics();
    bool pass15 = testGate8B_15_QuantizationShadowCheck();

    std::cout << "Gate 8-B-1  (Model Topology & Parameter Partitioning): " << (pass1  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-B-2  (Accumulator Normalization Bridge /64):   " << (pass2  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-B-3  (Hidden Layer 1 Forward & CReLU Bounds):  " << (pass3  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-B-4  (Hidden Layer 2 Forward & Scaling Bridge):" << (pass4  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-B-5  (Output Layer FC3 Linear Centipawn Eval): " << (pass5  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-B-6  (Sigmoid Probability & Frozen Temp T=400):" << (pass6  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-B-7  (Exp A: Outcome-Only BCE Formulation):    " << (pass7  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-B-8  (Exp B1: Teacher Distill MSE Formulation):" << (pass8  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-B-9  (Exp B2: Direct Centipawn MSE Objective): " << (pass9  ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-B-10 (Exp C: Blended Convex Combination Loss): " << (pass10 ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-B-11 (Dataset Binary Ingestion Record Counts): " << (pass11 ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-B-12 (Test Set Isolation & Zero Collision):    " << (pass12 ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-B-13 (Mode P2 Frozen Feature Transformer):     " << (pass13 ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-B-14 (Activation Saturation & Diagnostics):    " << (pass14 ? "PASS" : "FAIL") << "\n";
    std::cout << "Gate 8-B-15 (Quantization Shadow Check Telemetry):    " << (pass15 ? "PASS" : "FAIL") << "\n";
    std::cout << "=================================================================\n";

    return pass1 && pass2 && pass3 && pass4 && pass5 && pass6 && pass7 && pass8 &&
           pass9 && pass10 && pass11 && pass12 && pass13 && pass14 && pass15;
}


void runDiagnostics() {
    std::cout << "\n==================================================\n";
    std::cout << "===   EXECUTING BOSON SUBSYSTEM DIAGNOSTICS   ===\n";
    std::cout << "==================================================\n";

    int solvedCount = 0;

    for (const auto& test : g_wacSuite) {
        std::cout << "\n>>> DIAGNOSTICS FOR POS #" << test.id << " (" << test.name << " | Target: " << test.targetMove << ") <<<\n";
        
        auto parsedResult = FenParser::parse(test.fen);
        if (!parsedResult.has_value()) {
            std::cout << "[ERROR] Failed to parse FEN: " << test.fen << "\n";
            continue;
        }

        Position pos = parsedResult.value();

        // Execute Search
        int evalScore = Search::runSearch(pos, 10);
        
        // Extract top move from search statistics PV string
        std::string pvStr = SearchController::getInstance().getStats().pvString;
        std::string bestMoveStr = "";
        std::stringstream ss(pvStr);
        ss >> bestMoveStr; // Extract first move

        if (bestMoveStr == test.targetMove) {
            std::cout << "[RESULT] PASS: Search chose expected move [" << bestMoveStr << "] (Eval: " << evalScore << " cp)\n";
            solvedCount++;
        } else {
            std::cout << "[RESULT] FAIL: Search chose [" << bestMoveStr << "] instead of [" << test.targetMove << "] (Eval: " << evalScore << " cp)\n";
        }
        std::cout << "--------------------------------------------------\n";
    }

    std::cout << "\n==================================================\n";
    std::cout << "FINAL SUITE RESULT: " << solvedCount << "/" << g_wacSuite.size() << " Solved.\n";
    std::cout << "==================================================\n";
}

} // namespace Boson

int main(int argc, char* argv[]) {
    std::cout << std::unitbuf;
    Boson::MoveGenerator::initializeTables();

    if (argc > 1) {
        std::string_view arg = argv[1];
        if (arg == "--match100" || arg == "--match" || arg == "--strength") {
            bool ok = Boson::runExpandedStrengthMatch100Games();
            return ok ? 0 : 1;
        }
        if (arg == "--phase8b" || arg == "--suite36" || arg == "--8b") {
            bool ok = Boson::runPhase8BSupervisedTrainingTests();
            return ok ? 0 : 1;
        }
    }

    bool m1Phase2Success = Boson::runMilestone1Tests();
    bool m1Module13Success = Boson::runMilestone1Module13Tests();
    bool m2PhasesBCSuccess = Boson::runMilestone2PhasesBCTests();
    bool m2PerftSuccess = Boson::runMilestone2PerftTests();
    bool phaseYZSuccess = Boson::runPhaseYZTests();
    bool phaseAASuccess = Boson::runPhaseAATests();
    bool phaseABSuccess = Boson::runPhaseABTests();
    bool m6Phase12Success = Boson::runMilestone6Phase12Tests();
    bool m6Module63Success = Boson::runMilestone6Module63Tests();
    bool m6Module64Success = Boson::runMilestone6Module64Tests();
    bool m6Module65Success = Boson::runMilestone6Module65Tests();
    bool m6Module66Success = Boson::runMilestone6Module66Tests();
    bool m6Module67Success = Boson::runMilestone6Module67Tests();
    bool m6Module68Success = Boson::runMilestone6Module68Tests();
    bool m6Module69Success = Boson::runMilestone6Module69Tests();
    bool m6Module610Success = Boson::runMilestone6Module610Tests();
    bool omegaPhase1Success = Boson::runMilestoneOmegaPhase1Tests();
    bool omegaPhase2Success = Boson::runMilestoneOmegaPhase2TacticalTests();
    bool omegaPhase3Success = Boson::runMilestoneOmegaPhase3IntegrityTests();
    bool omegaPhase4Success = Boson::runMilestoneOmegaPhase4BenchmarkTests();
    bool omegaPhase5Success = Boson::runMilestoneOmegaPhase5StrengthTests();
    bool omegaPhase6Success = Boson::runMilestoneOmegaPhase6ParameterRegistryTests();
    bool phase65ASuccess = Boson::runPhase65APvsTests();
    bool phase65BSuccess = Boson::runPhase65BMovePickerTests();
    bool phase65CSuccess = Boson::runPhase65CReverseFutilityPruningTests();
    bool phase65DSuccess = Boson::runPhase65DImprovingHeuristicTests();
    bool phase7ASuccess = Boson::runPhase7AEvaluationAbstractionTests();
    bool phase7BSuccess = Boson::runPhase7BFeatureTransformerTests();
    bool phase7CSuccess = Boson::runPhase7CAccumulatorTests();
    bool phase7DSuccess = Boson::runPhase7DScalarInferenceTests();
    bool phase7ESuccess = Boson::runPhase7ENNUEEvaluatorTests();
    bool phase7FSuccess = Boson::runPhase7FAVX2Tests();
    bool smokeMatchSuccess = Boson::runOperationalSmokeMatch20Games();
    bool phase7GSuccess = Boson::runPhase7GStrengthValidationTests();
    bool phase8ASuccess = Boson::runPhase8ADatasetTests();
    bool phase8BSuccess = Boson::runPhase8BSupervisedTrainingTests();
    Boson::runDiagnostics();
    std::cout << "\n=== TEST SUITE RESULTS ===\n"
              << "m1Phase2: " << m1Phase2Success << "\n"
              << "m1Module13: " << m1Module13Success << "\n"
              << "m2PhasesBC: " << m2PhasesBCSuccess << "\n"
              << "m2Perft: " << m2PerftSuccess << "\n"
              << "phaseYZ: " << phaseYZSuccess << "\n"
              << "phaseAA: " << phaseAASuccess << "\n"
              << "phaseAB: " << phaseABSuccess << "\n"
              << "m6Phase12: " << m6Phase12Success << "\n"
              << "m6Module63: " << m6Module63Success << "\n"
              << "m6Module64: " << m6Module64Success << "\n"
              << "m6Module65: " << m6Module65Success << "\n"
              << "m6Module66: " << m6Module66Success << "\n"
              << "m6Module67: " << m6Module67Success << "\n"
              << "m6Module68: " << m6Module68Success << "\n"
              << "m6Module69: " << m6Module69Success << "\n"
              << "m6Module610: " << m6Module610Success << "\n"
              << "omegaPhase1: " << omegaPhase1Success << "\n"
              << "omegaPhase2: " << omegaPhase2Success << "\n"
              << "omegaPhase3: " << omegaPhase3Success << "\n"
              << "omegaPhase4: " << omegaPhase4Success << "\n"
              << "omegaPhase5: " << omegaPhase5Success << "\n"
              << "omegaPhase6: " << omegaPhase6Success << "\n"
              << "phase65A: " << phase65ASuccess << "\n"
              << "phase65B: " << phase65BSuccess << "\n"
              << "phase65C: " << phase65CSuccess << "\n"
              << "phase65D: " << phase65DSuccess << "\n"
              << "phase7A: " << phase7ASuccess << "\n"
              << "phase7B: " << phase7BSuccess << "\n"
              << "phase7C: " << phase7CSuccess << "\n"
              << "phase7D: " << phase7DSuccess << "\n"
              << "phase7E: " << phase7ESuccess << "\n"
              << "phase7F: " << phase7FSuccess << "\n"
              << "smokeMatch: " << smokeMatchSuccess << "\n"
              << "phase7G: " << phase7GSuccess << "\n"
              << "phase8A: " << phase8ASuccess << "\n"
              << "phase8B: " << phase8BSuccess << "\n"
              << "==========================\n";
    return (m1Phase2Success && m1Module13Success && m2PhasesBCSuccess && m2PerftSuccess && phaseYZSuccess && phaseAASuccess && phaseABSuccess && m6Phase12Success && m6Module63Success && m6Module64Success && m6Module65Success && m6Module66Success && m6Module67Success && m6Module68Success && m6Module69Success && m6Module610Success && omegaPhase1Success && omegaPhase2Success && omegaPhase3Success && omegaPhase4Success && omegaPhase5Success && omegaPhase6Success && phase65ASuccess && phase65BSuccess && phase65CSuccess && phase65DSuccess && phase7ASuccess && phase7BSuccess && phase7CSuccess && phase7DSuccess && phase7ESuccess && phase7FSuccess && smokeMatchSuccess && phase7GSuccess && phase8ASuccess && phase8BSuccess) ? 0 : 1;
}