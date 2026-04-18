// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Original author: Jade Ellis <jade@ellis.link>
// Original source: https://forgejo.ellis.link/continuwuation/resolvematrix.git

use std::net::{IpAddr, SocketAddr};
use std::str::FromStr;

use hickory_resolver::TokioResolver;
use hickory_resolver::proto::rr::RData;
use reqwest::{Client, StatusCode};
use serde::Deserialize;

use thiserror::Error;

/// Error type for Matrix server resolution.
#[derive(Debug, Error)]
pub enum ResolveServerError {
    #[error("Failed to parse address: {0}")]
    AddrParse(#[from] std::net::AddrParseError),

    #[error("HTTP client error: {0}")]
    Http(#[from] reqwest::Error),

    #[error("DNS resolution error: {0}")]
    Dns(#[from] hickory_resolver::net::NetError),

    #[error("Invalid port number: {0}")]
    InvalidPort(#[from] std::num::ParseIntError),

    #[error("Malformed .well-known response")]
    MalformedWellKnown,

    #[error("Unexpected error: {0}")]
    Other(String),
}

/// Represents the resolved destination for a Matrix server.
#[derive(Debug, Clone)]
pub enum ResolvedDestination {
    /// A literal IP address and port (e.g., 1.2.3.4:8448)
    Literal(SocketAddr),
    /// A named host and port (e.g., "matrix.org", "8448")
    Named(String, u16),
}

/// Result of resolving a Matrix server name.
///
/// Contains the actual destination to connect to and the original server name
/// (useful for SNI / Host header in federation requests).
#[derive(Debug, Clone)]
pub struct Resolution {
    /// The resolved destination (where to actually connect).
    pub destination: ResolvedDestination,
    /// The original server name or the delegated hostname from .well-known.
    /// Useful for setting the SNI / Host header in federation requests.
    pub server_name: String,
}

impl Resolution {
    /// Get the base URL for making HTTP requests to this resolved destination.
    pub fn base_url(&self) -> String {
        match &self.destination {
            ResolvedDestination::Literal(addr) => format!("https://{}", addr),
            ResolvedDestination::Named(host, port) => format!("https://{}:{}", host, port),
        }
    }
}

/// The main resolver struct for Matrix server resolution.
pub struct MatrixResolver {
    client: Client,
    resolver: TokioResolver,
}

impl MatrixResolver {
    /// Create a new MatrixResolver.
    pub async fn new() -> Result<Self, ResolveServerError> {
        let resolver = hickory_resolver::Resolver::builder_tokio()?.build()?;

        let client = Client::builder()
            .timeout(std::time::Duration::from_secs(10))
            .build()?;

        Ok(MatrixResolver { client, resolver })
    }

    /// Resolve a Matrix server name and return the Resolution.
    ///
    /// Follows the [Server Discovery](https://spec.matrix.org/v1.15/server-server-api/#server-discovery)
    /// section of the Matrix specification.
    pub async fn resolve_server(
        &self,
        server_name: &str,
    ) -> Result<Resolution, ResolveServerError> {
        // 1. If the hostname is an IP literal
        if let Some((ip, port)) = get_ip_with_port(server_name) {
            let socket = SocketAddr::new(ip, port.unwrap_or(8448));
            return Ok(Resolution {
                destination: ResolvedDestination::Literal(socket),
                server_name: server_name.to_owned(),
            });
        }

        // 2. Hostname with explicit port
        if let Some(pos) = server_name.find(':') {
            let (host_part, port_part) = server_name.split_at(pos);
            let port: u16 = port_part.trim_start_matches(':').parse()?;
            return Ok(Resolution {
                destination: ResolvedDestination::Named(host_part.to_owned(), port),
                server_name: server_name.to_owned(),
            });
        }

        // 3. Well-known delegation
        if let Some(res) = self.resolve_well_known(server_name).await {
            match res {
                WellKnownServerResult::Ip(ip, port) => {
                    let socket = SocketAddr::new(ip, port.unwrap_or(8448));
                    return Ok(Resolution {
                        destination: ResolvedDestination::Literal(socket),
                        server_name: server_name.to_owned(),
                    });
                }
                WellKnownServerResult::Domain(domain, None) => {
                    // 3.3/3.4: Delegated hostname, no port — try SRV
                    if let Some((srv_host, srv_port)) = self.query_srv_record(&domain).await? {
                        return Ok(Resolution {
                            destination: ResolvedDestination::Named(srv_host, srv_port),
                            server_name: domain,
                        });
                    }
                    // 3.5: No SRV, fallback to A/AAAA/CNAME + 8448
                    return Ok(Resolution {
                        destination: ResolvedDestination::Named(domain, 8448),
                        server_name: server_name.to_owned(),
                    });
                }
                WellKnownServerResult::Domain(domain, Some(port)) => {
                    return Ok(Resolution {
                        destination: ResolvedDestination::Named(domain, port),
                        server_name: server_name.to_owned(),
                    });
                }
            }
        }

        // 4. SRV lookup on original hostname
        if let Some((srv_host, srv_port)) = self.query_srv_record(server_name).await? {
            return Ok(Resolution {
                destination: ResolvedDestination::Named(srv_host, srv_port),
                server_name: server_name.to_owned(),
            });
        }

        // 5. Fallback: A/AAAA/CNAME + 8448
        Ok(Resolution {
            destination: ResolvedDestination::Named(server_name.to_owned(), 8448),
            server_name: server_name.to_owned(),
        })
    }

    /// Resolve .well-known delegation for a hostname.
    async fn resolve_well_known(&self, hostname: &str) -> Option<WellKnownServerResult> {
        #[derive(Deserialize)]
        struct WellKnown {
            #[serde(rename = "m.server")]
            m_server: String,
        }
        let url = format!("https://{hostname}/.well-known/matrix/server");
        let resp = match self.client.get(&url).send().await {
            Ok(r) => r,
            Err(_) => return None,
        };
        if resp.status() != StatusCode::OK {
            return None;
        }
        let wk: WellKnown = resp.json().await.ok()?;
        if let Some((ip, port)) = get_ip_with_port(&wk.m_server) {
            return Some(WellKnownServerResult::Ip(ip, port));
        }
        let (host, port) = parse_server_name(&wk.m_server);
        Some(WellKnownServerResult::Domain(host, port))
    }

    /// Query SRV records for a hostname, returning (target, port) if found.
    async fn query_srv_record(
        &self,
        hostname: &str,
    ) -> Result<Option<(String, u16)>, ResolveServerError> {
        let srv_names = [
            format!("_matrix-fed._tcp.{hostname}"),
            format!("_matrix._tcp.{hostname}"),
        ];
        for srv in &srv_names {
            let lookup = self.resolver.srv_lookup(srv).await;
            if let Ok(result) = lookup
                && let Some(srv_data) = result.answers().iter().find_map(|r| match &r.data {
                    RData::SRV(srv) => Some(srv),
                    _ => None,
                })
            {
                let target = srv_data.target.to_utf8();
                let port = srv_data.port;
                return Ok(Some((target.trim_end_matches('.').to_owned(), port)));
            }
        }
        Ok(None)
    }
}

#[derive(Debug)]
enum WellKnownServerResult {
    Ip(IpAddr, Option<u16>),
    Domain(String, Option<u16>),
}

/// Parses a Matrix server name into (hostname, Option<port>).
fn parse_server_name(server_name: &str) -> (String, Option<u16>) {
    if let Some((host, port)) = server_name.rsplit_once(':')
        && let Ok(port) = u16::from_str(port)
    {
        return (host.to_string(), Some(port));
    }
    (server_name.to_string(), None)
}

/// If the string is an IP literal (with optional port), returns (IpAddr, port).
fn get_ip_with_port(s: &str) -> Option<(IpAddr, Option<u16>)> {
    // Try SocketAddr first (IP:port)
    if let Ok(sock) = SocketAddr::from_str(s) {
        return Some((sock.ip(), Some(sock.port())));
    }
    // Try IP only
    if let Ok(ip) = IpAddr::from_str(s) {
        return Some((ip, None));
    }
    None
}

#[cfg(test)]
mod tests {
    use rstest::rstest;

    use super::*;

    #[test]
    fn test_get_ip_with_port() {
        assert_eq!(
            get_ip_with_port("127.0.0.1:8080"),
            Some((IpAddr::from([127, 0, 0, 1]), Some(8080)))
        );
        assert_eq!(
            get_ip_with_port("[::1]:8080"),
            Some((IpAddr::from([0, 0, 0, 0, 0, 0, 0, 1]), Some(8080)))
        );
        assert_eq!(
            get_ip_with_port("127.0.0.1"),
            Some((IpAddr::from([127, 0, 0, 1]), None))
        );
        assert_eq!(
            get_ip_with_port("::1"),
            Some((IpAddr::from([0, 0, 0, 0, 0, 0, 0, 1]), None))
        );
        assert_eq!(get_ip_with_port("example.com"), None);
    }

    #[test]
    fn test_get_ip_with_port_invalid() {
        assert_eq!(get_ip_with_port("invalid"), None);
        assert_eq!(get_ip_with_port("127.0.0.1:invalid"), None);
        assert_eq!(get_ip_with_port("::1:invalid"), None);
        assert_eq!(get_ip_with_port("127.0.0.1:8080:invalid"), None);
        assert_eq!(get_ip_with_port("::1:8080:invalid"), None);
    }

    /// Test parse_server_name function with various inputs
    #[rstest]
    #[case::no_port("matrix.org", "matrix.org", None)]
    #[case::with_port("matrix.org:8448", "matrix.org", Some(8448))]
    #[case::high_port("server.com:9999", "server.com", Some(9999))]
    #[case::low_port("localhost:80", "localhost", Some(80))]
    #[case::ipv4_with_port("192.168.1.1:8008", "192.168.1.1", Some(8008))]
    fn test_parse_server_name(
        #[case] input: &str,
        #[case] expected_host: &str,
        #[case] expected_port: Option<u16>,
    ) {
        let (host, port) = parse_server_name(input);
        assert_eq!(host, expected_host);
        assert_eq!(port, expected_port);
    }

    /// Test IP literal detection with parameterized cases
    #[rstest]
    #[case::ipv4_with_port("127.0.0.1:8080", Some((IpAddr::from([127, 0, 0, 1]), Some(8080))))]
    #[case::ipv4_no_port("127.0.0.1", Some((IpAddr::from([127, 0, 0, 1]), None)))]
    #[case::ipv6_with_port("[::1]:8080", Some((IpAddr::from([0, 0, 0, 0, 0, 0, 0, 1]), Some(8080))))]
    #[case::ipv6_no_port("::1", Some((IpAddr::from([0, 0, 0, 0, 0, 0, 0, 1]), None)))]
    #[case::hostname("example.com", None)]
    #[case::hostname_with_port("example.com:8448", None)]
    #[case::invalid("not-an-ip", None)]
    fn test_get_ip_with_port_parameterized(
        #[case] input: &str,
        #[case] expected: Option<(IpAddr, Option<u16>)>,
    ) {
        assert_eq!(get_ip_with_port(input), expected);
    }

    /// Test resolution of well-known servers
    #[rstest]
    #[case::maunium("maunium.net")]
    #[case::nexy("nexy7574.co.uk")]
    #[tokio::test]
    async fn test_well_known_resolution(#[case] server_name: &str) {
        let resolver = MatrixResolver::new().await.unwrap();
        let resolution = resolver.resolve_server(server_name).await;

        assert!(
            resolution.is_ok(),
            "Failed to resolve {}: {:?}",
            server_name,
            resolution.err()
        );

        let resolved = resolution.unwrap();

        // Verify the resolution contains valid data
        match &resolved.destination {
            ResolvedDestination::Literal(addr) => {
                assert!(addr.port() > 0, "Port should be greater than 0");
            }
            ResolvedDestination::Named(host, port) => {
                assert!(!host.is_empty(), "Host should not be empty");
                assert!(*port > 0, "Port should be greater than 0");
            }
        }
    }

    /// Test servers with explicit ports
    #[rstest]
    #[case::standard_port("matrix.org:8448")]
    #[case::custom_port("timedout.uk:69")]
    #[case::high_port("test.server:9999")]
    #[tokio::test]
    async fn test_explicit_port_resolution(#[case] server_name: &str) {
        let resolver = MatrixResolver::new().await.unwrap();
        let resolution = resolver.resolve_server(server_name).await;

        assert!(
            resolution.is_ok(),
            "Failed to resolve {}: {:?}",
            server_name,
            resolution.err()
        );

        let resolved = resolution.unwrap();
        let expected_port: u16 = server_name.split(':').nth(1).unwrap().parse().unwrap();

        match &resolved.destination {
            ResolvedDestination::Named(_, port) => {
                assert_eq!(
                    *port, expected_port,
                    "Port should match the explicit port in server name"
                );
            }
            ResolvedDestination::Literal(addr) => {
                assert_eq!(
                    addr.port(),
                    expected_port,
                    "Port should match the explicit port in server name"
                );
            }
        }
    }

    /// Test IP literal resolution
    #[rstest]
    #[case::ipv4_default("192.168.1.1")]
    #[case::ipv4_custom_port("192.168.1.1:8008")]
    #[case::ipv6_default("::1")]
    #[case::ipv6_custom_port("[::1]:8008")]
    #[tokio::test]
    async fn test_ip_literal_resolution(#[case] server_name: &str) {
        let resolver = MatrixResolver::new().await.unwrap();
        let resolution = resolver.resolve_server(server_name).await;

        assert!(
            resolution.is_ok(),
            "Failed to resolve {}: {:?}",
            server_name,
            resolution.err()
        );

        let resolved = resolution.unwrap();

        // IP literals should always resolve to Literal variant
        match &resolved.destination {
            ResolvedDestination::Literal(addr) => {
                assert!(addr.port() > 0, "Port should be greater than 0");
                if !server_name.contains(':') {
                    assert_eq!(addr.port(), 8448, "Should default to port 8448");
                }
            }
            _ => panic!("IP literal should resolve to Literal variant"),
        }
    }

    /// Test that base_url() returns the actual destination, not the SNI host
    #[tokio::test]
    async fn test_base_url_uses_destination() {
        let resolver = MatrixResolver::new().await.unwrap();

        // matrixrooms.info delegates to api.matrixrooms.info:443 via .well-known
        let resolution = resolver.resolve_server("matrixrooms.info").await.unwrap();
        let url = resolution.base_url();

        assert!(
            url.contains("api.matrixrooms.info"),
            "base_url() should use the delegated host, got: {}",
            url
        );
        assert!(
            !url.starts_with("https://matrixrooms.info:"),
            "base_url() should NOT use the original server name, got: {}",
            url
        );
    }

    /// Test that server_name preserves the original name for SNI
    #[tokio::test]
    async fn test_server_name_preserved() {
        let resolver = MatrixResolver::new().await.unwrap();

        let resolution = resolver.resolve_server("matrixrooms.info").await.unwrap();
        assert_eq!(resolution.server_name, "matrixrooms.info");
    }

    /// Test that resolution produces valid results for servers using various
    /// discovery methods (well-known, SRV, explicit port, fallback).
    #[rstest]
    #[case::maunium_net("maunium.net")]
    #[case::timedout_uk_port("timedout.uk:69")]
    #[case::nexy7574_co_uk("nexy7574.co.uk")]
    #[case::matrix_org("matrix.org")]
    #[case::resolvematrix_2_port("2.s.resolvematrix.dev:7652")]
    #[case::resolvematrix_3b("3b.s.resolvematrix.dev")]
    #[case::resolvematrix_3c("3c.s.resolvematrix.dev")]
    #[case::resolvematrix_3d("3d.s.resolvematrix.dev")]
    #[case::resolvematrix_4("4.s.resolvematrix.dev")]
    #[case::resolvematrix_5("5.s.resolvematrix.dev")]
    #[case::resolvematrix_3c_msc4040("3c.msc4040.s.resolvematrix.dev")]
    #[case::resolvematrix_4_msc4040("4.msc4040.s.resolvematrix.dev")]
    #[tokio::test]
    async fn test_server_resolution(#[case] server_name: &str) {
        let resolver = MatrixResolver::new().await.unwrap();
        let resolution = resolver.resolve_server(server_name).await;

        assert!(
            resolution.is_ok(),
            "Failed to resolve {}: {:?}",
            server_name,
            resolution.err()
        );

        let resolved = resolution.unwrap();
        let url = resolved.base_url();
        assert!(url.starts_with("https://"), "base_url should be HTTPS: {}", url);

        match &resolved.destination {
            ResolvedDestination::Literal(addr) => {
                assert!(addr.port() > 0, "Port should be greater than 0");
            }
            ResolvedDestination::Named(host, port) => {
                assert!(!host.is_empty(), "Host should not be empty");
                assert!(*port > 0, "Port should be greater than 0");
            }
        }
    }

    /// Test HTTP connectivity using base_url() with a plain HTTP client.
    /// Only includes servers where a plain client works (no SNI tricks needed).
    #[rstest]
    #[case::nexy7574_co_uk("nexy7574.co.uk")]
    #[case::resolvematrix_3b("3b.s.resolvematrix.dev")]
    #[case::resolvematrix_3c("3c.s.resolvematrix.dev")]
    #[case::resolvematrix_3d("3d.s.resolvematrix.dev")]
    #[case::resolvematrix_4("4.s.resolvematrix.dev")]
    #[case::resolvematrix_5("5.s.resolvematrix.dev")]
    #[case::resolvematrix_3c_msc4040("3c.msc4040.s.resolvematrix.dev")]
    #[case::resolvematrix_4_msc4040("4.msc4040.s.resolvematrix.dev")]
    #[tokio::test]
    async fn test_resolve_and_connect(#[case] server_name: &str) {
        let resolver = MatrixResolver::new().await.unwrap();
        let resolution = resolver.resolve_server(server_name).await.unwrap();

        let client = Client::builder()
            .danger_accept_invalid_certs(true)
            .timeout(std::time::Duration::from_secs(10))
            .build()
            .unwrap();

        let url = format!("{}/_matrix/federation/v1/version", resolution.base_url());
        let resp = client.get(&url).send().await;

        match resp {
            Ok(r) => {
                assert_eq!(
                    r.status(),
                    StatusCode::OK,
                    "Server {} returned non-200 from {}",
                    server_name,
                    url
                );
            }
            Err(e) => {
                panic!(
                    "Failed to connect to {} at {}: {}",
                    server_name, url, e
                );
            }
        }
    }
}
