#ifndef BOSON_UNDO_STATE_HPP
#define BOSON_UNDO_STATE_HPP

#include "Square.hpp"
#include "Castling.hpp"
#include "Piece.hpp"

namespace Boson {

struct UndoState {
    CastlingRights castlingRights;
    Square enPassantSquare;
    int halfmoveClock;
    Piece capturedPiece;
    Piece movingPiece;
    Square castlingRookFrom{Square::None};
    Square castlingRookTo{Square::None};
    Piece castlingRookPiece{Piece::None};
};

} // namespace Boson

#endif // BOSON_UNDO_STATE_HPP