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

export module smodel:sobj;

namespace impl {
class SObj {
  uuids::uuid id;
  ;
};
} // namespace impl

export namespace smodel {};