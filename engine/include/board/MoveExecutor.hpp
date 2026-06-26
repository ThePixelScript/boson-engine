#ifndef BOSON_MOVE_EXECUTOR_HPP
#define BOSON_MOVE_EXECUTOR_HPP

#include "Position.hpp"
#include "Move.hpp"
#include "UndoState.hpp"

namespace Boson {

class MoveExecutor {
public:
    // Executes a move on the Position and records tracking information into undoState
    static void makeMove(Position& pos, const Move& move, UndoState& undoState) noexcept;
    
    // Inverts a move, restoring the Position to its exact prior state
    static void undoMove(Position& pos, const Move& move, const UndoState& undoState) noexcept;
};

} // namespace Boson

#endif // BOSON_MOVE_EXECUTOR_HPP