#ifndef BOSON_FEN_PARSER_HPP
#define BOSON_FEN_PARSER_HPP

#include <string>
#include <string_view>
#include <expected>
#include "board/Position.hpp"

namespace Boson {

enum class ParseError : uint8_t {
    InvalidPiecePlacement,
    InvalidActiveColor,
    InvalidCastlingRights,
    InvalidEnPassantSquare,
    InvalidHalfmoveClock,
    InvalidFullmoveNumber,
    MalformedFieldCount
};

class FenParser {
public:
    // Parses a standard FEN string. Returns a populated Position or a ParseError.
    static std::expected<Position, ParseError> parse(std::string_view fen) noexcept;

private:
    static bool parsePiecePlacement(std::string_view field, Position& pos) noexcept;
    static bool parseActiveColor(std::string_view field, Position& pos) noexcept;
    static bool parseCastlingRights(std::string_view field, Position& pos) noexcept;
    static bool parseEnPassant(std::string_view field, Position& pos) noexcept;
    static bool parseHalfmoveClock(std::string_view field, Position& pos) noexcept;
    static bool parseFullmoveNumber(std::string_view field, Position& pos) noexcept;
};

std::string_view to_string(ParseError error) noexcept;

} // namespace Boson

#endif // BOSON_FEN_PARSER_HPP