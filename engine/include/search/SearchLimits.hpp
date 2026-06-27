#ifndef BOSON_SEARCH_LIMITS_HPP
#define BOSON_SEARCH_LIMITS_HPP

#include <cstdint>

namespace Boson {

struct SearchLimits {
    int64_t wtime = -1;
    int64_t btime = -1;
    int64_t winc = 0;
    int64_t binc = 0;
    int64_t movetime = -1;
    int depth = -1;
    bool infinite = false;
};

} // namespace Boson

#endif // BOSON_SEARCH_LIMITS_HPP