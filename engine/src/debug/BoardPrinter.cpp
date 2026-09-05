#include "debug/BoardPrinter.hpp"
#include <iostream>

namespace Boson {

void BoardPrinter::print(const Position& pos, Mode mode, std::ostream& os) noexcept {
    if (mode == Mode::Human || mode == Mode::Debug) {
        os << "\n";
        for (int rank = 7; rank >= 0; --rank) {
            os << " " << (rank + 1) << " ";
            for (int file = 0; file < 8; ++file) {
                Square sq = static_cast<Square>(rank * 8 + file);
                Bitboard mask = Bitboards::getSquareBit(sq);
                char symbol = '.';

                if (pos.getPieceBitboard(Piece::WhitePawn) & mask) symbol = 'P';
                else if (pos.getPieceBitboard(Piece::WhiteKnight) & mask) symbol = 'N';
                else if (pos.getPieceBitboard(Piece::WhiteBishop) & mask) symbol = 'B';
                else if (pos.getPieceBitboard(Piece::WhiteRook) & mask) symbol = 'R';
                else if (pos.getPieceBitboard(Piece::WhiteQueen) & mask) symbol = 'Q';
                else if (pos.getPieceBitboard(Piece::WhiteKing) & mask) symbol = 'K';
                else if (pos.getPieceBitboard(Piece::BlackPawn) & mask) symbol = 'p';
                else if (pos.getPieceBitboard(Piece::BlackKnight) & mask) symbol = 'n';
                else if (pos.getPieceBitboard(Piece::BlackBishop) & mask) symbol = 'b';
                else if (pos.getPieceBitboard(Piece::BlackRook) & mask) symbol = 'r';
                else if (pos.getPieceBitboard(Piece::BlackQueen) & mask) symbol = 'q';
                else if (pos.getPieceBitboard(Piece::BlackKing) & mask) symbol = 'k';

                os << " " << symbol;
            }
            os << "\n";
        }
        os << "     a b c d e f g h\n";
    }

    if (mode == Mode::Debug) {
        os << "\n--- State Metadata ---\n";
        os << "Side To Move : " << (pos.getSideToMove() == Color::White ? "White" : "Black") << "\n";
        
        auto rights = pos.getCastlingRights();
        os << "Castling     : ";
        if (rights == CastlingRights::None) os << "-";
        else {
            if (static_cast<uint8_t>(rights & CastlingRights::WhiteOO))  os << "K";
            if (static_cast<uint8_t>(rights & CastlingRights::WhiteOOO)) os << "Q";
            if (static_cast<uint8_t>(rights & CastlingRights::BlackOO))  os << "k";
            if (static_cast<uint8_t>(rights & CastlingRights::BlackOOO)) os << "q";
        }
        os << "\n";

        os << "En Passant   : " << (pos.getEnPassantSquare() == Square::None ? "-" : "Active") << "\n";
        os << "Halfmove     : " << pos.getHalfmoveClock() << "\n";
        os << "Fullmove     : " << pos.getFullmoveNumber() << "\n";
        os << "Total Occ.   : " << std::hex << pos.getTotalOccupancy() << std::dec << "\n";
        os << "----------------------\n\n";
    }
}

} // namespace Boson