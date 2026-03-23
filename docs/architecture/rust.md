# 🦀 Rust in Komai

Komai includes Rust code for functionality best served by existing Rust crates. The first integration is **[resolvematrix](https://crates.io/crates/resolvematrix)** for Matrix server discovery (`.well-known/matrix/server` + SRV records).

## Why Rust?

Rust is a long-game investment. A memory-safe language that is also fast is a great fit for making a C++ codebase better — without throwing it all away and going "Rust only." We bridge to Rust where it adds value: leveraging existing crates (like `resolvematrix` for Matrix server discovery) and gradually writing new functionality in a safer language. The first integration is Matrix server-to-server endpoint discovery for direct MRS API calls.

## Interop Pattern

### CXX bridge (primary)

All internal Rust ↔ C++ interop uses [CXX](https://cxx.rs/). CXX provides type-safe bridging for strings, structs, enums, and vectors at compile time. `Result<T>` on the Rust side maps to C++ exceptions.

### C++ wrapper classes (insulation layer)

Each CXX bridge is wrapped in a plain C++ class (e.g., `MatrixServerResolver`) that:
- Converts between CXX types (`rust::String`) and Qt types (`QString`)
- Catches CXX exceptions and returns `std::optional` + error string
- Hides the `rust::` namespace from the rest of the codebase

This means the rest of Komai never sees CXX types. If we ever change interop mechanism, only the wrapper changes.

### When to use other patterns

- **cbindgen**: only if we need a real C API for external consumers or platform packaging.
- **Raw `extern "C"`**: only for isolated escape hatches where CXX overhead isn't justified.

## Directory Layout

```
src/rust/
├── Cargo.toml          # Package manifest (staticlib + rlib + binaries)
├── Cargo.lock          # Committed for reproducible builds
├── build.rs            # CXX build script
└── src/
    ├── lib.rs          # Shared Rust library modules + CXX bridge
    ├── bin/
    │   └── komai-mcp.rs
    ├── ipc/
    │   ├── client.rs
    │   ├── mod.rs
    │   ├── protocol.rs
    │   ├── unix.rs
    │   └── windows.rs
    └── mcp/
        ├── errors.rs
        ├── mod.rs
        ├── results.rs
        ├── server.rs
        └── tools.rs

src/matrix/
├── MatrixServerResolver.h    # C++ wrapper (public API)
└── MatrixServerResolver.cpp  # Wrapper implementation
```

## Build Integration

[Corrosion](https://github.com/corrosion-rs/corrosion) (via CPM) drives the Rust build from CMake:

```cmake
CPMAddPackage(NAME Corrosion ...)
corrosion_import_crate(MANIFEST_PATH src/rust/Cargo.toml)
target_link_libraries(komai PRIVATE komai-rust)
```

Corrosion invokes `cargo build` and produces both:

- the Rust library that links into the `komai` binary
- the sibling `komai-mcp` executable used by `komai mcp serve`

The `komai` CLI does not embed the MCP protocol implementation directly. It
dispatches to `komai-mcp`, which then talks back to the running app over the
existing IPC transport.

## Async Strategy

The Rust crate uses a `tokio::Runtime` (created once via `OnceLock`) with `block_on()` to run async resolver calls synchronously. C++ callers are expected to invoke the resolver from worker threads (matching the existing `FetchRoomsChunkFromDirectoryJob` pattern) to avoid blocking the UI thread.

## Packaging

### Flatpak

Flatpak builds have no network access. Rust crates are vendored via `flatpak-cargo-generator.py`:

1. `just flatpak-cargo-sources` (generates `var/build/flatpak/cargo-sources.json` from `src/rust/Cargo.lock`)
2. The generated JSON is added as a Flatpak source that populates `CARGO_HOME`
3. The `org.freedesktop.Sdk.Extension.rust-stable` SDK extension provides the toolchain

### Native / AppImage / Arch

These environments have network access during build, so `cargo` fetches crates normally. Only requirement: `rustc` + `cargo` installed.

## Adding New Rust Code

1. Add dependencies to `src/rust/Cargo.toml`
2. Add bridge functions/types in `src/rust/src/lib.rs`
3. Create a C++ wrapper class in `src/matrix/` (or appropriate directory)
4. Add the wrapper `.cpp` to `SRC_FILES` in `CMakeLists.txt`
5. Regenerate `cargo-sources.json` for Flatpak: `just flatpak-cargo-sources`
6. Update this document if the pattern changes
