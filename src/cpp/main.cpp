#include "rust/cxx.h"
#include "rust_bridge/lib.h"
#include <print>

int core_compute();

import math;
import gfx;

int main() {
    std::print("core_compute() = {0}\n", core_compute());

    ffi::Vec3 v{1.0f, 2.0f, 3.0f};
    auto w = ffi::scale(v, 2.0f);
    std::print("scale(Vec3, 2) = ({0}, {1}, {2})\n", w.x, w.y, w.z);

    std::print("math::add_ints(1,2) = {0}\n", math::add_ints(1,2));
    std::print("gfx::forty_two() = {0}\n", gfx::forty_two());
    return 0;
}
