// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

fn main() {
    cxx_build::bridge("src/lib.rs")
        .include("..")
        .std("c++20")
        .compile("komai_rust_cxx");
    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=../matrix/backend/MatrixBackendBridge.h");
}
