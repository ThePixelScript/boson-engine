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
    MalformedFieldCount,
    MissingKing,
    PawnsOnFirstOrLastRank
};

class FenParser {
public:
    // Performs syntactic parsing of a standard FEN string into a Position.
    static std::expected<Position, ParseError> parse(std::string_view fen) noexcept;

    // Performs semantic validation of game-rule invariants on an instantiated Position.
    static std::expected<void, ParseError> validateSemantics(const Position& pos) noexcept;

    // Executes syntactic parsing followed by semantic validation.
    static std::expected<Position, ParseError> parseStrict(std::string_view fen) noexcept;

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