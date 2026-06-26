#ifndef BOSON_UNDO_STATE_HPP
#define BOSON_UNDO_STATE_HPP

#include "Square.hpp"
#include "Castling.hpp"
#include "Piece.hpp"

namespace Boson {

struct UndoState {
    CastlingRights castlingRights;
    Square enPassantSquare;
    uint16_t halfmoveClock;
    Piece capturedPiece;
};

} // namespace Boson

#endif // BOSON_UNDO_STATE_HPP