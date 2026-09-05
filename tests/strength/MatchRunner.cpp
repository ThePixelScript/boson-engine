#include "strength/MatchRunner.hpp"
#include "fen/FenParser.hpp"
#include "board/Castling.hpp"
#include "board/MoveGenerator.hpp"
#include "board/MoveExecutor.hpp"
#include "board/UndoState.hpp"
#include "search/Search.hpp"
#include "search/SearchController.hpp"
#include "search/SearchLimits.hpp"
#include "benchmark/BenchmarkRunner.hpp"
#include <chrono>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <bit>

namespace Boson {

namespace {

struct SilentStreamBuffer {
    std::streambuf* orig;
    std::ostringstream dummy;
    SilentStreamBuffer() : orig(std::cout.rdbuf(dummy.rdbuf())) {}
    ~SilentStreamBuffer() {
        std::cout.rdbuf(orig);
    }
};

std::string fenFromPosition(const Position& pos) noexcept {
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
    if (ep != Square::None) {
        int file = static_cast<int>(ep) % 8;
        int rank = static_cast<int>(ep) / 8;
        fen += static_cast<char>('a' + file);
        fen += static_cast<char>('1' + rank);
    } else {
        fen += "-";
    }

    fen += " " + std::to_string(pos.getHalfmoveClock()) + " " + std::to_string(pos.getFullmoveNumber());
    return fen;
}

bool isInsufficientMaterial(const Position& pos) noexcept {
    bool hasPawns = (pos.getPieceBitboard(Piece::WhitePawn) | pos.getPieceBitboard(Piece::BlackPawn)) != 0;
    bool hasMajors = (pos.getPieceBitboard(Piece::WhiteRook) | pos.getPieceBitboard(Piece::BlackRook) |
                      pos.getPieceBitboard(Piece::WhiteQueen) | pos.getPieceBitboard(Piece::BlackQueen)) != 0;
    if (hasPawns || hasMajors) return false;

    uint32_t wKnights = static_cast<uint32_t>(std::popcount(pos.getPieceBitboard(Piece::WhiteKnight)));
    uint32_t bKnights = static_cast<uint32_t>(std::popcount(pos.getPieceBitboard(Piece::BlackKnight)));
    uint32_t wBishops = static_cast<uint32_t>(std::popcount(pos.getPieceBitboard(Piece::WhiteBishop)));
    uint32_t bBishops = static_cast<uint32_t>(std::popcount(pos.getPieceBitboard(Piece::BlackBishop)));

    // King vs King
    if (wKnights + bKnights + wBishops + bBishops == 0) return true;

    // King + minor vs King
    if ((wKnights + wBishops == 1 && bKnights + bBishops == 0) ||
        (bKnights + bBishops == 1 && wKnights + wBishops == 0)) {
        return true;
    }

    return false;
}

} // anonymous namespace

InProcessUciEngine::InProcessUciEngine(std::string name, const EngineParameters& params, size_t hashMb)
    : m_name(std::move(name)), m_params(params), m_hashMb(hashMb) {
    auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (opt) m_pos = *opt;
}

void InProcessUciEngine::sendCommand(std::string_view cmd) {
    if (cmd == "ucinewgame") {
        BenchmarkRunner::resetSearchState(m_hashMb);
        auto opt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        if (opt) m_pos = *opt;
        m_lastBestMove.clear();
        return;
    }

    if (cmd.starts_with("position ")) {
        std::string_view rest = cmd.substr(9);
        std::string baseFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        if (rest.starts_with("startpos")) {
            rest = rest.substr(8);
            auto opt = FenParser::parse(baseFen);
            if (opt) m_pos = *opt;
        } else if (rest.starts_with("fen ")) {
            rest = rest.substr(4);
            size_t movesPos = rest.find(" moves ");
            std::string fenStr = (movesPos != std::string_view::npos) ? std::string(rest.substr(0, movesPos)) : std::string(rest);
            auto opt = FenParser::parse(fenStr);
            if (opt) m_pos = *opt;
            if (movesPos != std::string_view::npos) {
                rest = rest.substr(movesPos);
            } else {
                rest = "";
            }
        }

        // Parse optional "moves m1 m2 ..."
        size_t movesIdx = rest.find("moves ");
        if (movesIdx != std::string_view::npos) {
            std::string movesStr = std::string(rest.substr(movesIdx + 6));
            std::istringstream iss(movesStr);
            std::string moveToken;
            while (iss >> moveToken) {
                MoveList legalMoves;
                MoveGenerator::generateLegalMoves(m_pos, legalMoves);
                bool applied = false;
                for (size_t i = 0; i < legalMoves.size(); ++i) {
                    if (legalMoves[i].toString() == moveToken) {
                        UndoState undo;
                        MoveExecutor::makeMove(m_pos, legalMoves[i], undo);
                        applied = true;
                        break;
                    }
                }
                if (!applied) {
                    break;
                }
            }
        }
        return;
    }

    if (cmd.starts_with("go ")) {
        std::string_view rest = cmd.substr(3);
        SearchLimits limits;
        limits.clearTables = true;

        if (rest.starts_with("movetime ")) {
            int ms = std::stoi(std::string(rest.substr(9)));
            limits.movetime = ms;
        } else if (rest.starts_with("depth ")) {
            int d = std::stoi(std::string(rest.substr(6)));
            limits.depth = d;
        } else {
            limits.depth = 4; // fallback
        }

        SearchController::getInstance().setParams(m_params);

        {
            SilentStreamBuffer silencer;
            Search::runSearch(m_pos, limits);
        }

        const auto& stats = SearchController::getInstance().getStats();
        if (stats.pvLine.count > 0) {
            m_lastBestMove = stats.pvLine.moves[0].toString();
        } else {
            std::stringstream ss(stats.pvString);
            ss >> m_lastBestMove;
        }
        return;
    }
}

std::string InProcessUciEngine::getBestMove(int timeoutMs) {
    (void)timeoutMs;
    return m_lastBestMove;
}

void InProcessUciEngine::setParameters(const EngineParameters& params) {
    m_params = params;
}

GameRecord MatchRunner::playGame(uint32_t gameId, IUciEngine& whiteEngine, IUciEngine& blackEngine, const OpeningEntry& opening, const MatchConfig& config) {
    auto tStart = std::chrono::high_resolution_clock::now();

    GameRecord record;
    record.gameId = gameId;
    record.openingId = std::string(opening.id);
    record.openingVersion = std::string(OpeningBook::VERSION);
    record.whiteEngine = whiteEngine.getName();
    record.blackEngine = blackEngine.getName();

    whiteEngine.sendCommand("ucinewgame");
    blackEngine.sendCommand("ucinewgame");

    auto startOpt = FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (!startOpt) {
        record.termination = TerminationType::ProtocolError;
        record.result = GameResult::Draw;
        return record;
    }

    Position pos = *startOpt;
    std::vector<std::string> gameMoves;
    std::vector<uint64_t> positionHashHistory;
    positionHashHistory.push_back(pos.getHashKey());

    // Play opening moves
    for (const auto& opMove : opening.moveSequence) {
        MoveList legalMoves;
        MoveGenerator::generateLegalMoves(pos, legalMoves);
        bool found = false;
        for (size_t i = 0; i < legalMoves.size(); ++i) {
            if (legalMoves[i].toString() == opMove) {
                UndoState undo;
                MoveExecutor::makeMove(pos, legalMoves[i], undo);
                gameMoves.push_back(opMove);
                positionHashHistory.push_back(pos.getHashKey());
                found = true;
                break;
            }
        }
        if (!found) {
            record.termination = TerminationType::ProtocolError;
            record.result = GameResult::Draw;
            return record;
        }
    }

    uint32_t plyCount = static_cast<uint32_t>(gameMoves.size());

    // Game loop
    while (plyCount < config.maxPlies) {
        MoveList legalMoves;
        MoveGenerator::generateLegalMoves(pos, legalMoves);

        // Terminal position detection
        if (legalMoves.size() == 0) {
            if (MoveGenerator::inCheck(pos, pos.getSideToMove())) {
                record.termination = TerminationType::Checkmate;
                record.result = (pos.getSideToMove() == Color::White) ? GameResult::BlackWin : GameResult::WhiteWin;
            } else {
                record.termination = TerminationType::Stalemate;
                record.result = GameResult::Draw;
            }
            break;
        }

        // Fifty move rule
        if (pos.getHalfmoveClock() >= 100) {
            record.termination = TerminationType::FiftyMoveRule;
            record.result = GameResult::Draw;
            break;
        }

        // Threefold repetition
        uint64_t currentKey = pos.getHashKey();
        size_t repCount = 0;
        for (uint64_t key : positionHashHistory) {
            if (key == currentKey) {
                repCount++;
            }
        }
        if (repCount >= 3) {
            record.termination = TerminationType::ThreefoldRepetition;
            record.result = GameResult::Draw;
            break;
        }

        // Insufficient material
        if (isInsufficientMaterial(pos)) {
            record.termination = TerminationType::InsufficientMaterial;
            record.result = GameResult::Draw;
            break;
        }

        // Engine query
        IUciEngine& activeEngine = (pos.getSideToMove() == Color::White) ? whiteEngine : blackEngine;

        std::string posCmd = "position startpos moves";
        for (const auto& m : gameMoves) {
            posCmd += " ";
            posCmd += m;
        }
        activeEngine.sendCommand(posCmd);

        if (config.fixedDepth > 0) {
            activeEngine.sendCommand("go depth " + std::to_string(config.fixedDepth));
        } else {
            activeEngine.sendCommand("go movetime " + std::to_string(config.timeControlMs));
        }

        std::string bestMoveStr = activeEngine.getBestMove(config.timeControlMs * 5 + 1000);

        if (bestMoveStr.empty()) {
            record.termination = TerminationType::Timeout;
            record.result = (pos.getSideToMove() == Color::White) ? GameResult::BlackWin : GameResult::WhiteWin;
            break;
        }

        bool isLegal = false;
        Move chosenMove;
        for (size_t i = 0; i < legalMoves.size(); ++i) {
            if (legalMoves[i].toString() == bestMoveStr) {
                isLegal = true;
                chosenMove = legalMoves[i];
                break;
            }
        }

        if (!isLegal) {
            record.termination = TerminationType::IllegalMove;
            record.result = (pos.getSideToMove() == Color::White) ? GameResult::BlackWin : GameResult::WhiteWin;
            break;
        }

        UndoState undo;
        MoveExecutor::makeMove(pos, chosenMove, undo);
        gameMoves.push_back(bestMoveStr);
        positionHashHistory.push_back(pos.getHashKey());
        plyCount++;
    }

    if (plyCount >= config.maxPlies && record.result == GameResult::Draw && record.termination == TerminationType::Stalemate) {
        record.termination = TerminationType::FiftyMoveRule;
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    record.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();
    record.plyCount = plyCount;
    record.moves = gameMoves;
    record.finalFen = fenFromPosition(pos);

    return record;
}

MatchRecord MatchRunner::runMatch(const MatchConfig& config, IUciEngine* customEngineA, IUciEngine* customEngineB) {
    auto tStart = std::chrono::high_resolution_clock::now();

    std::unique_ptr<IUciEngine> ownedA;
    std::unique_ptr<IUciEngine> ownedB;

    IUciEngine* engineA = customEngineA;
    IUciEngine* engineB = customEngineB;

    if (!engineA) {
        ownedA = std::make_unique<InProcessUciEngine>(config.engineA, config.paramsA, config.hashMb);
        engineA = ownedA.get();
    }
    if (!engineB) {
        ownedB = std::make_unique<InProcessUciEngine>(config.engineB, config.paramsB, config.hashMb);
        engineB = ownedB.get();
    }

    MatchRecord matchRecord;
    matchRecord.schemaVersion = "1.0.0";
    matchRecord.config = config;

    uint32_t totalPairs = (config.totalGames + 1) / 2;
    uint32_t gameNumber = 1;
    uint32_t winsA = 0;
    uint32_t drawsA = 0;
    uint32_t lossesA = 0;

    for (uint32_t p = 0; p < totalPairs && gameNumber <= config.totalGames; ++p) {
        const auto& opening = OpeningBook::getOpening(p);

        // Game 2k - 1: Engine A White, Engine B Black
        {
            GameRecord g1 = playGame(gameNumber++, *engineA, *engineB, opening, config);
            matchRecord.games.push_back(g1);

            if (g1.result == GameResult::WhiteWin) {
                winsA++;
            } else if (g1.result == GameResult::BlackWin) {
                lossesA++;
            } else {
                drawsA++;
            }
        }

        if (gameNumber > config.totalGames) break;

        // Game 2k: Engine B White, Engine A Black
        {
            GameRecord g2 = playGame(gameNumber++, *engineB, *engineA, opening, config);
            matchRecord.games.push_back(g2);

            if (g2.result == GameResult::BlackWin) {
                winsA++;
            } else if (g2.result == GameResult::WhiteWin) {
                lossesA++;
            } else {
                drawsA++;
            }
        }
    }

    matchRecord.stats = Statistics::computeStatistics(winsA, drawsA, lossesA);

    auto tEnd = std::chrono::high_resolution_clock::now();
    matchRecord.totalElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();

    return matchRecord;
}

} // namespace Boson
