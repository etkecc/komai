//! `resolvematrix` is a Rust library providing the ability to resolve Matrix server-to-server endpoints from the server name.
//! It conforms to the [Server Discovery section in the Matrix specification](https://spec.matrix.org/v1.15/server-server-api/#server-discovery).
//!
//! The library is tested against the resolvematrix.dev suite and live domains.

#[cfg(feature = "server")]
pub mod server;
