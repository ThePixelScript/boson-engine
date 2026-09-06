#ifndef BOSON_OPENING_BOOK_HPP
#define BOSON_OPENING_BOOK_HPP

#include <string>
#include <string_view>
#include <vector>
#include <span>

namespace Boson {

struct OpeningEntry {
    std::string_view id{};
    std::string_view family{};
    std::string_view name{};
    std::vector<std::string> moveSequence{};
    std::string_view resultingFen{};
};

class OpeningBook {
public:
    static constexpr std::string_view VERSION = "1.0.0";

    [[nodiscard]] static std::string_view getVersion() noexcept { return VERSION; }
    [[nodiscard]] static std::span<const OpeningEntry> getOpenings() noexcept;
    [[nodiscard]] static const OpeningEntry& getOpening(size_t index) noexcept;
    [[nodiscard]] static size_t size() noexcept;
};

} // namespace Boson

#endif // BOSON_OPENING_BOOK_HPP
