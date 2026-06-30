#include "fen/FenParser.hpp"
#include <vector>
#include <sstream>
#include <charconv>

namespace Boson {

std::string_view to_string(ParseError error) noexcept {
    switch (error) {
        case ParseError::InvalidPiecePlacement: return "Syntax Error: Invalid Piece Placement Field.";
        case ParseError::InvalidActiveColor:    return "Syntax Error: Invalid Active Color Field.";
        case ParseError::InvalidCastlingRights: return "Syntax Error: Invalid Castling Rights Field.";
        case ParseError::InvalidEnPassantSquare: return "Syntax Error: Invalid En Passant Square Field.";
        case ParseError::InvalidHalfmoveClock:  return "Syntax Error: Invalid Halfmove Clock Field.";
        case ParseError::InvalidFullmoveNumber:  return "Syntax Error: Invalid Fullmove Number Field.";
        case ParseError::MalformedFieldCount:   return "Syntax Error: Incorrect Number of FEN Fields.";
    }
    return "Unknown Parser Error.";
}

std::expected<Position, ParseError> FenParser::parse(std::string_view fen) noexcept {
    Position pos;
    pos.clearState();

    std::vector<std::string_view> fields;
    size_t start = 0;
    size_t end = fen.find(' ');
    
    while (end != std::string_view::npos) {
        fields.push_back(fen.substr(start, end - start));
        start = end + 1;
        end = fen.find(' ', start);
    }
    fields.push_back(fen.substr(start));

    if (fields.size() != 6) {
        return std::unexpected(ParseError::MalformedFieldCount);
    }

    if (!parsePiecePlacement(fields[0], pos))  return std::unexpected(ParseError::InvalidPiecePlacement);
    if (!parseActiveColor(fields[1], pos))     return std::unexpected(ParseError::InvalidActiveColor);
    if (!parseCastlingRights(fields[2], pos))  return std::unexpected(ParseError::InvalidCastlingRights);
    if (!parseEnPassant(fields[3], pos))       return std::unexpected(ParseError::InvalidEnPassantSquare);
    if (!parseHalfmoveClock(fields[4], pos))   return std::unexpected(ParseError::InvalidHalfmoveClock);
    if (!parseFullmoveNumber(fields[5], pos))  return std::unexpected(ParseError::InvalidFullmoveNumber);

    pos.updateOccupancy();
    return pos;
}

bool FenParser::parsePiecePlacement(std::string_view field, Position& pos) noexcept {
    int rank = 7;
    int file = 0;

    for (char c : field) {
        if (c == '/') {
            if (file != 8) return false;
            --rank;
            file = 0;
            if (rank < 0) return false;
        } else if (c >= '1' && c <= '8') {
            file += (c - '0');
            if (file > 8) return false;
        } else {
            Square sq = static_cast<Square>(rank * 8 + file);
            Piece piece = Piece::None;

            switch (c) {
                case 'P': piece = Piece::WhitePawn; break;
                case 'N': piece = Piece::WhiteKnight; break;
                case 'B': piece = Piece::WhiteBishop; break;
                case 'R': piece = Piece::WhiteRook; break;
                case 'Q': piece = Piece::WhiteQueen; break;
                case 'K': piece = Piece::WhiteKing; break;
                case 'p': piece = Piece::BlackPawn; break;
                case 'n': piece = Piece::BlackKnight; break;
                case 'b': piece = Piece::BlackBishop; break;
                case 'r': piece = Piece::BlackRook; break;
                case 'q': piece = Piece::BlackQueen; break;
                case 'k': piece = Piece::BlackKing; break;
                default: return false; // Unknown character
            }
            pos.setPiece(sq, piece);
            ++file;
        }
    }
    return (rank == 0 && file == 8);
}

bool FenParser::parseActiveColor(std::string_view field, Position& pos) noexcept {
    if (field.size() != 1) return false;
    if (field[0] == 'w') { pos.setSideToMove(Color::White); return true; }
    if (field[0] == 'b') { pos.setSideToMove(Color::Black); return true; }
    return false;
}

bool FenParser::parseCastlingRights(std::string_view field, Position& pos) noexcept {
    if (field == "-") { pos.setCastlingRights(CastlingRights::None); return true; }
    
    CastlingRights rights = CastlingRights::None;
    for (char c : field) {
        switch (c) {
            case 'K': rights = rights | CastlingRights::WhiteOO; break;
            case 'Q': rights = rights | CastlingRights::WhiteOOO; break;
            case 'k': rights = rights | CastlingRights::BlackOO; break;
            case 'q': rights = rights | CastlingRights::BlackOOO; break;
            default: return false;
        }
    }
    pos.setCastlingRights(rights);
    return true;
}

bool FenParser::parseEnPassant(std::string_view field, Position& pos) noexcept {
    if (field == "-") { pos.setEnPassantSquare(Square::None); return true; }
    if (field.size() != 2) return false;

    int file = field[0] - 'a';
    int rank = field[1] - '1';
    if (file < 0 || file > 7 || rank < 0 || rank > 7) return false;

    pos.setEnPassantSquare(static_cast<Square>(rank * 8 + file));
    return true;
}

bool FenParser::parseHalfmoveClock(std::string_view field, Position& pos) noexcept {
    uint16_t val = 0;
    auto [ptr, ec] = std::from_chars(field.data(), field.data() + field.size(), val);
    if (ec != std::errc{} || ptr != field.data() + field.size()) return false;
    pos.setHalfmoveClock(val);
    return true;
}

bool FenParser::parseFullmoveNumber(std::string_view field, Position& pos) noexcept {
    uint16_t val = 0;
    auto [ptr, ec] = std::from_chars(field.data(), field.data() + field.size(), val);
    if (ec != std::errc{} || ptr != field.data() + field.size() || val == 0) return false;
    pos.setFullmoveNumber(val);
    return true;
}

} // namespace Boson