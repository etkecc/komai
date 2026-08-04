# 🦀 Rust in Komai

Komai's Rust code is now part of the application's core runtime. It owns the
Matrix client/backend layer built on [matrix-sdk](https://github.com/matrix-org/matrix-rust-sdk),
plus settings codecs, syntax highlighting, theme embedding, homeserver
discovery, and the `komai-mcp` automation binary.

## Why Rust?

Rust is a long-game investment. A memory-safe language that is also fast is a
great fit for making a C++ codebase better without throwing it all away and
going "Rust only." Komai started with Rust for Matrix homeserver discovery via
**[resolvematrix](https://crates.io/crates/resolvematrix)**; it now also uses
Rust for the matrix-sdk runtime and other high-churn core infrastructure where
safety and existing crate ecosystems pay off.

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

Representative layout:

```
src/rust/
├── Cargo.toml              # Workspace root for komai-rust
├── Cargo.lock              # Committed for reproducible builds
├── build.rs                # CXX bridge build script
├── komai-mcp/              # Rust MCP server binary crate
├── resolvematrix/          # Vendored Matrix homeserver discovery crate
└── src/
    ├── lib.rs              # Shared Rust library modules + CXX bridge
    ├── ffi.rs              # FFI bridge types/helpers
    ├── logging.rs          # Tracing/logging bridge
    ├── matrix_backend/     # matrix-sdk runtime, room list, timeline, auth
    ├── settings/           # config/state/session/secrets codecs
    ├── syntax_highlight/    # syntect-based code highlighting + contrast-aware palettes
    └── theme/              # built-in theme embedding

src/matrix/
├── MatrixServerResolver.*       # C++ wrapper around resolvematrix
└── backend/
    ├── MatrixAuthService.*      # Login/bootstrap bridge
    ├── MatrixBackendBridge.*    # Rust runtime ownership/lifecycle
    ├── MatrixBackendRuntimeService.* # Blocking worker-thread bridge methods
    ├── MatrixBlockingCall.h     # C++ blocking-call policy
    ├── MatrixFfiBlockingContext.h
    ├── MatrixSdkPaths.*         # Per-profile filesystem layout
    └── MatrixSessionSecrets.*   # Session secret marshalling
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

The Rust crate uses a shared `tokio::Runtime` (created once via `OnceLock`).

There are two distinct patterns:

- Non-blocking runtime work:
  - long-lived room-list / timeline loops and similar internal Rust tasks use the runtime directly
  - these are internal Rust concerns and do not carry a C++ thread policy
- Blocking FFI entrypoints:
  - some exported Rust functions still need to synchronously wait on async runtime work for the
    current C++ call
  - those must not call `runtime().block_on(...)` directly anymore

### Blocking FFI rule

Exported blocking Rust entrypoints take an explicit `MatrixFfiBlockingContext` and must go through
`ffi_block_on(context, operation, future)`.

That context carries two pieces of information from C++:

- thread policy:
  - `RequireWorkerThread`
- caller thread kind:
  - `AppUiThread`
  - `WorkerThread`

`ffi_block_on(...)` is the Rust-side choke point that:

- initializes logging
- validates the context
- panics immediately if a worker-only blocking call claims to come from the app/UI thread
- only then enters `runtime().block_on(future)`

This is deliberate. The goal is to make the blocking policy explicit at the C++ -> Rust seam,
instead of letting future bridge functions silently introduce raw `block_on()` calls.

### C++ side contract

Blocking C++ wrapper/service methods use `BlockingCallContext` from
`src/matrix/backend/MatrixBlockingCall.h`.

The two normal constructors are:

- `blockingCallContext()`
  - worker-thread only
  - logs and aborts immediately if created on the app/UI thread

Current policy:

- there is no longer a UI-thread blocking constructor on the C++ side
- if a caller wants to block into Rust, it must first move to a worker-thread path

`src/matrix/backend/MatrixFfiBlockingContext.h` converts that C++ context into the generated Rust
bridge struct.

### Why this exists

This hardening came from real failures:

- UI-thread `runtime().block_on(...)` paths can deadlock when Rust callbacks need the app thread
- mixed “sometimes blocking inline, sometimes worker-thread” patterns make those deadlocks easy to
  reintroduce accidentally
- qtkeychain/session-persistence fallout made it clear that “just fix one caller” was not enough;
  the seam needed one explicit policy

The design is intentionally scoped to exported blocking FFI entrypoints.

- Internal/background Rust runtime users still use the runtime directly.
- The C++ side now only permits the worker-thread blocking shape for exported blocking entrypoints.

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
