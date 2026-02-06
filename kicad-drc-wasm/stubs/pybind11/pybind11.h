#pragma once
// Stub pybind11 for WASM build - Python binding not needed
namespace pybind11 {
    class object {};
    class handle : public object {};
}
namespace py = pybind11;
