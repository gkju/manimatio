module;

#include <algorithm>
#include <cmath>
#include <concepts>
#include <functional>
#include <memory>
#include <stduuid/uuid.h>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

export module smodel:util;

export namespace smodel {

inline uuids::uuid generate_uuid() {
  thread_local auto engine = [] {
    std::random_device rd;
    std::array<int, std::mt19937::state_size> seed_data;
    std::generate(seed_data.begin(), seed_data.end(), std::ref(rd));
    std::seed_seq seq(seed_data.begin(), seed_data.end());
    return std::mt19937{seq};
  }();
  return uuids::uuid_random_generator{engine}();
}

}; // namespace smodel