#ifndef BOSON_SEARCH_HPP
#define BOSON_SEARCH_HPP

#include "board/Position.hpp"
#include "board/MoveList.hpp"
#include "search/TranspositionTable.hpp"
#include "search/CounterMoveTable.hpp"
#include "search/ContinuationHistoryTable.hpp" // Include memory layout
#include "search/PVLine.hpp"
#include "search/SearchController.hpp"
#include "eval/IEvaluator.hpp"
#include "eval/ClassicalEvaluator.hpp"
#include <array>

namespace Boson {

class Search {
public:
    static uint64_t perft(Position& pos, int depth) noexcept;
    static void divide(Position& pos, int depth) noexcept;
    static int runSearch(Position& pos, int maxDepth) noexcept;
    static int runSearch(Position& pos, const SearchLimits& limits) noexcept;
    static TranspositionTable s_tt;
    
    static constexpr int INF = 32000;
    static constexpr int MATE = 31000;
    static constexpr int MATE_SCORE = MATE;
    static constexpr int MAX_PLY = 64;
    static constexpr uint64_t NODE_CHECK_PERIOD = 2048;
    static constexpr int ASPIRATION_INITIAL_DELTA = 30;
    static constexpr int ASPIRATION_MAX_DELTA = 400;

    static int searchWithAspiration(Position& pos, int depth, int prevScore, PVLine& pv) noexcept;

    [[nodiscard]] static constexpr int scoreToTT(int score, int ply) noexcept {
        if (score >= MATE - 100) return score + ply;
        if (score <= -MATE + 100) return score - ply;
        return score;
    }

    [[nodiscard]] static constexpr int scoreFromTT(int score, int ply) noexcept {
        if (score >= MATE - 100) return score - ply;
        if (score <= -MATE + 100) return score + ply;
        return score;
    }

    [[nodiscard]] static const CounterMoveTable& getCMH() noexcept { return s_cmTable; }
    [[nodiscard]] static CounterMoveTable& getMutableCMH() noexcept { return s_cmTable; }
    static void clearCMH() noexcept { s_cmTable.clear(); }
    [[nodiscard]] static const ContinuationHistoryTable& getContHist() noexcept { return s_chTable; }
    [[nodiscard]] static ContinuationHistoryTable& getMutableContHist() noexcept { return s_chTable; }
    static void clearContHist() noexcept { s_chTable.clear(); }
    [[nodiscard]] static const PVLine& getLastPV() noexcept {
        return SearchController::getInstance().getStats().pvLine;
    }

    static int quiescence(Position& pos, int alpha, int beta, int ply = 0) noexcept;
    static int evaluate(const Position& pos) noexcept;
    static int negamax(Position& pos, int depth, int alpha, int beta, int ply, PVLine& pv, bool allowNull = true, Move prevMove = Move()) noexcept;

    static void setEvaluator(eval::IEvaluator* evaluator) noexcept;
    [[nodiscard]] static eval::IEvaluator* getEvaluator() noexcept;

    static eval::ClassicalEvaluator m_defaultEvaluator;
    static eval::IEvaluator* m_evaluator;

    struct StackEntry {
        int staticEval = 0;
        bool inCheck = false;
    };

    static std::array<StackEntry, 128> s_searchStack;
    static void clearStack() noexcept { s_searchStack.fill(StackEntry{}); }
    [[nodiscard]] static std::array<StackEntry, 128>& getStack() noexcept { return s_searchStack; }

    [[nodiscard]] static bool isImproving(int ply, bool inCheck, int staticEval) noexcept {
        if (ply < 2 || ply >= 128) return false;
        if (inCheck || s_searchStack[ply - 2].inCheck) return false;
        const int prevEval = s_searchStack[ply - 2].staticEval;
        if (std::abs(staticEval) >= (MATE_SCORE - MAX_PLY) || std::abs(prevEval) >= (MATE_SCORE - MAX_PLY)) {
            return false;
        }
        return staticEval > prevEval;
    }

    [[nodiscard]] static int computeLmrReduction(int baseReduction, bool improving, int improvingBonus, int depth) noexcept {
        int r = baseReduction;
        if (improving) {
            r += improvingBonus;
        } else {
            r = std::max(0, r - 1);
        }
        return std::clamp(r, 0, std::max(0, depth - 2));
    }

private:
    static CounterMoveTable s_cmTable;
    static ContinuationHistoryTable s_chTable; // Managed memory layer
    static std::array<std::array<Move, 2>, 64> s_killerMoves;
    static std::array<std::array<uint32_t, 64>, 12> s_historyTable;
};

} // namespace Boson
#endif // BOSON_SEARCH_HPP