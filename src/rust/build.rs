// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

fn main() {
    cxx_build::bridge("src/ffi.rs")
        .include("..")
        .std("c++20")
        .compile("komai_rust_cxx");
    println!("cargo:rerun-if-changed=src/ffi.rs");
    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=../matrix/backend/MatrixBackendBridge.h");
    println!("cargo:rerun-if-changed=src/matrix_backend/session_persistence.rs");
    println!("cargo:rerun-if-changed=../../resources/themes");
}
