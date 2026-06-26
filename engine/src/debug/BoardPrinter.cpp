#include "debug/BoardPrinter.hpp"
#include <iostream>

namespace Boson {

void BoardPrinter::print(const Position& pos, Mode mode) noexcept {
    if (mode == Mode::Human || mode == Mode::Debug) {
        std::cout << "\n";
        for (int rank = 7; rank >= 0; --rank) {
            std::cout << " " << (rank + 1) << " ";
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

                std::cout << " " << symbol;
            }
            std::cout << "\n";
        }
        std::cout << "     a b c d e f g h\n";
    }

    if (mode == Mode::Debug) {
        std::cout << "\n--- State Metadata ---\n";
        std::cout << "Side To Move : " << (pos.getSideToMove() == Color::White ? "White" : "Black") << "\n";
        
        auto rights = pos.getCastlingRights();
        std::cout << "Castling     : ";
        if (rights == CastlingRights::None) std::cout << "-";
        else {
            if (static_cast<uint8_t>(rights & CastlingRights::WhiteOO))  std::cout << "K";
            if (static_cast<uint8_t>(rights & CastlingRights::WhiteOOO)) std::cout << "Q";
            if (static_cast<uint8_t>(rights & CastlingRights::BlackOO))  std::cout << "k";
            if (static_cast<uint8_t>(rights & CastlingRights::BlackOOO)) std::cout << "q";
        }
        std::cout << "\n";

        std::cout << "En Passant   : " << (pos.getEnPassantSquare() == Square::None ? "-" : "Active") << "\n";
        std::cout << "Halfmove     : " << pos.getHalfmoveClock() << "\n";
        std::cout << "Fullmove     : " << pos.getFullmoveNumber() << "\n";
        std::cout << "Total Occ.   : " << std::hex << pos.getTotalOccupancy() << std::dec << "\n";
        std::cout << "----------------------\n\n";
    }
}

} // namespace Boson