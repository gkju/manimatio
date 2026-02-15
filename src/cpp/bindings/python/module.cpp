#include <nanobind/nanobind.h>
namespace nb = nanobind;

extern int core_compute();

NB_MODULE(cpp_rust_template, m) {
    m.def("core_compute", &core_compute, "Call core C++/Rust");
}
