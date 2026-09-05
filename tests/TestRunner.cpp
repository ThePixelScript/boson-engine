#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <bit>
#include <iomanip>
#include <type_traits>
#include <utility>
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
#include "search/see/SEE.hpp"
#include "search/LMR.hpp"
#include "evaluation/Evaluator.hpp"
#include "evaluation/PieceSquareTables.hpp"
#include "config/EngineParameters.hpp"
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
    // Startpos evaluated to depth 5: contains dozens of quiet moves where late candidates
    // undergo reduced search, fail high, and trigger full-depth re-searches.
    auto parsed = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!parsed.has_value()) return false;
    Position pos = parsed.value();

    SearchLimits limits;
    limits.depth = 5;
    Search::runSearch(pos, limits);

    const auto& stats = SearchController::getInstance().getStats();
    if (stats.completedDepth != 5) return false;
    if (stats.pvLine.count == 0) return false;

    // 1. Reduced scout searches must have been attempted
    if (stats.lmrAttempts == 0) return false;
    if (stats.lmrReducedNodes == 0) return false;

    // 2. Re-search protocol must have triggered upon score > alpha
    if (stats.lmrResearches == 0) return false;

    // 3. WAC.004 benchmark position where late refutations also verify PV overturns
    auto parsedWac = FenParser::parse("r2qk2r/ppp2ppp/2n5/2b1p3/6b1/3P1N2/PPP1BPPP/R1BQ1RK1 b kq - 0 1");
    if (!parsedWac.has_value()) return false;
    Position posWac = parsedWac.value();

    SearchLimits limitsWac;
    limitsWac.depth = 6;
    Search::runSearch(posWac, limitsWac);

    const auto& statsWac = SearchController::getInstance().getStats();
    if (statsWac.lmrResearches == 0) return false;
    if (statsWac.successfulResearches == 0) return false;

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

int main() {
    std::cout << std::unitbuf;
    Boson::MoveGenerator::initializeTables();
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
    Boson::runDiagnostics();
    return (m1Phase2Success && m1Module13Success && m2PhasesBCSuccess && m2PerftSuccess && phaseYZSuccess && phaseAASuccess && phaseABSuccess && m6Phase12Success && m6Module63Success && m6Module64Success && m6Module65Success && m6Module66Success && m6Module67Success && m6Module68Success && m6Module69Success && m6Module610Success && omegaPhase1Success && omegaPhase2Success && omegaPhase3Success && omegaPhase4Success) ? 0 : 1;
}