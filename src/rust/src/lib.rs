#[cxx::bridge(namespace = "ffi")]
mod ffi {
    struct Vec3 {
        x: f32,
        y: f32,
        z: f32,
    }

    extern "Rust" {
        fn rust_add(a: i32, b: i32) -> i32;
        fn scale(v: Vec3, s: f32) -> Vec3;
    }
}

pub fn rust_add(a: i32, b: i32) -> i32 {
    a + b
}

pub fn scale(v: ffi::Vec3, s: f32) -> ffi::Vec3 {
    ffi::Vec3 { x: v.x * s, y: v.y * s, z: v.z * s }
}
