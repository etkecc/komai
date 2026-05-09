// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

fn main() {
    cxx_build::bridge("src/ffi.rs")
        .include("..")
        .std("c++20")
        // GCC's -Wmaybe-uninitialized fires as a false positive against
        // the Vec<T>::Vec() constructor pattern that cxx-bridge emits in
        // ffi.rs.cc — once per #[cxx::bridge] type, ~30 hits per build.
        // clang and MSVC don't trip. flag_if_supported only injects on
        // compilers that recognise the option, so clang-on-Linux and
        // MSVC-on-Windows just ignore the call.
        .flag_if_supported("-Wno-maybe-uninitialized")
        .compile("komai_rust_cxx");
    println!("cargo:rerun-if-changed=src/ffi.rs");
    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=../matrix/backend/MatrixBackendBridge.h");
    println!("cargo:rerun-if-changed=src/matrix_backend/session_persistence.rs");
    println!("cargo:rerun-if-changed=../../resources/themes");
}
