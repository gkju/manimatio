#include "rust/cxx.h"
#include "rust_bridge/lib.h"

int core_compute() {
    return ffi::rust_add(2, 40);
}
