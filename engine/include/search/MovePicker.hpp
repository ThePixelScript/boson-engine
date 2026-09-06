#ifndef BOSON_MOVE_PICKER_HPP
#define BOSON_MOVE_PICKER_HPP

#include "board/Position.hpp"
#include "board/Move.hpp"
#include "board/MoveList.hpp"
#include "board/Piece.hpp"
#include <array>
#include <vector>
#include <cstdint>

namespace Boson {

enum class Stage : uint8_t {
    TTMove,
    GenCaptures,
    GoodCaptures,
    Killers,
    CounterMove,
    GenQuiets,
    Quiets,
    BadCaptures,
    Done
};

enum class PickerMode : uint8_t {
    Normal,      // Full stages: TT -> Captures -> Killers -> CMH -> Quiets -> BadCaptures
    Quiescence   // Restricted: TT (if capture) -> GoodCaptures -> Done
};

struct SearchContext {
    int ply{0};
    Move prevMove{Move::none()};
    const std::array<std::array<Move, 2>, 64>* killerMoves{nullptr};
    const std::array<std::array<uint32_t, 64>, 12>* historyTable{nullptr};

    constexpr SearchContext() noexcept = default;
    constexpr SearchContext(int p, Move prev,
                            const std::array<std::array<Move, 2>, 64>* km = nullptr,
                            const std::array<std::array<uint32_t, 64>, 12>* ht = nullptr) noexcept
        : ply(p), prevMove(prev), killerMoves(km), historyTable(ht) {}
};

class MovePicker {
public:
    MovePicker(const Position& pos, Move ttMove, const SearchContext& context, PickerMode mode = PickerMode::Normal) noexcept;

    Move nextMove() noexcept;

    // Accessors for testing and inspection
    [[nodiscard]] Stage getCurrentStage() const noexcept { return currentStage; }
    [[nodiscard]] PickerMode getMode() const noexcept { return mode; }
    [[nodiscard]] const MoveList& getCaptures() const noexcept { return captures; }
    [[nodiscard]] const MoveList& getQuiets() const noexcept { return quiets; }
    [[nodiscard]] const MoveList& getBadCaptures() const noexcept { return badCaptures; }
    [[nodiscard]] uint8_t getYieldedCount() const noexcept { return yieldedCount; }

    void setKillers(Move k1, Move k2) noexcept { killers[0] = k1; killers[1] = k2; }
    void setCounterMove(Move cm) noexcept { counterMove = cm; }

    static bool isCapture(const Position& pos, Move m) noexcept;
    static bool isPseudoLegal(const Position& pos, Move m) noexcept;
    static bool isLegal(const Position& pos, Move m) noexcept;

private:
    void scoreCaptures() noexcept;
    void scoreQuiets() noexcept;

    [[nodiscard]] bool hasYielded(Move m) const noexcept;
    void recordYielded(Move m) noexcept;

    static Piece findPieceAtSquare(const Position& pos, Square sq) noexcept;
    static int getPieceIndex(Piece p) noexcept;

    const Position& pos;
    PickerMode mode;
    Stage currentStage;
    Move ttMove;
    std::array<Move, 2> killers;
    Move counterMove;
    MoveList captures;
    std::vector<int> captureScores;
    size_t captureIndex;
    MoveList quiets;
    std::vector<int> quietScores;
    size_t quietIndex;
    MoveList badCaptures;
    std::array<Move, 8> yieldedMoves;
    uint8_t yieldedCount;

    size_t badCaptureIndex{0};
    uint8_t killerIndex{0};
    SearchContext m_context;
};

} // namespace Boson

#endif // BOSON_MOVE_PICKER_HPP
