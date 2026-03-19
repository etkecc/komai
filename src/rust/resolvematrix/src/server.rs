use std::collections::HashMap;
use std::net::{IpAddr, SocketAddr};
use std::str::FromStr;
use std::sync::{Arc, RwLock};
use std::time::{Duration, Instant};

use hickory_resolver::TokioResolver;
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
    Dns(#[from] hickory_resolver::ResolveError),

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
    Named(String, String),
}

#[derive(Debug, Clone)]
pub struct Resolution {
    pub destination: ResolvedDestination,
    pub host: String,
}

impl Resolution {
    /// Get the base URL for making requests to this resolution.
    /// Uses the host field for proper SNI.
    pub fn base_url(&self) -> String {
        match &self.destination {
            ResolvedDestination::Literal(addr) => format!("https://{}", addr),
            ResolvedDestination::Named(_dest_host, dest_port) => {
                let port: u16 = dest_port.parse().unwrap_or(8448);
                if self.host.contains(':') {
                    format!("https://{}", self.host)
                } else {
                    format!("https://{}:{}", self.host, port)
                }
            }
        }
    }

    /// Get the hostname (without port) from the host field for DNS mapping.
    fn sni_hostname(&self) -> String {
        if let Some(colon_pos) = self.host.find(':') {
            self.host[..colon_pos].to_string()
        } else {
            self.host.clone()
        }
    }

    /// Get the destination address for DNS resolution mapping.
    async fn destination_addr(&self, resolver: &TokioResolver) -> Option<SocketAddr> {
        match &self.destination {
            ResolvedDestination::Literal(addr) => Some(*addr),
            ResolvedDestination::Named(dest_host, dest_port) => {
                let port: u16 = dest_port.parse().ok()?;

                // Try to parse as IP first
                if let Ok(ip) = dest_host.parse::<IpAddr>() {
                    return Some(SocketAddr::new(ip, port));
                }

                // Resolve via DNS
                match resolver.lookup_ip(dest_host.as_str()).await {
                    Ok(lookup) => {
                        let ip = lookup.iter().next()?;
                        Some(SocketAddr::new(ip, port))
                    }
                    Err(_) => None,
                }
            }
        }
    }
}

/// Simple cache entry with expiry time.
#[derive(Clone, Debug)]
struct CacheEntry {
    resolution: Resolution,
    expires_at: Instant,
    is_override: bool, // If true, this is a Matrix resolution that should be refetched when expired
}

/// Result of a cache lookup.
#[derive(Debug)]
enum CacheLookup {
    /// Valid cached entry found
    Valid(Resolution),
    /// Expired Matrix override - should refetch via Matrix resolution
    ExpiredOverride(String), // Returns the hostname that needs refetching
    /// No entry found or expired non-override
    Miss,
}

/// Simple cache for Matrix server resolutions with TTL-based expiry.
#[derive(Clone)]
pub(crate) struct Cache {
    inner: Arc<RwLock<HashMap<String, CacheEntry>>>,
    hostname_map: Arc<RwLock<HashMap<String, String>>>, // hostname -> server_name
    ttl: Duration,
}

impl Cache {
    fn new(ttl: Duration) -> Self {
        Self {
            inner: Arc::new(RwLock::new(HashMap::new())),
            hostname_map: Arc::new(RwLock::new(HashMap::new())),
            ttl,
        }
    }

    fn get(&self, server_name: &str) -> Option<Resolution> {
        // First try read lock to check if entry exists and is valid
        if let Ok(cache) = self.inner.read()
            && let Some(entry) = cache.get(server_name)
            && Instant::now() < entry.expires_at
        {
            return Some(entry.resolution.clone());
        }

        // If expired or not found, acquire write lock to remove expired entry
        if let Ok(mut cache) = self.inner.write()
            && let Some(entry) = cache.get(server_name)
            && Instant::now() >= entry.expires_at
        {
            cache.remove(server_name);
        }
        None
    }

    fn lookup(&self, hostname: &str) -> CacheLookup {
        // Try direct lookup first with read lock
        let lookup_result = if let Ok(cache) = self.inner.read() {
            if let Some(entry) = cache.get(hostname) {
                if Instant::now() < entry.expires_at {
                    return CacheLookup::Valid(entry.resolution.clone());
                }
                // Entry exists but is expired
                Some(entry.is_override)
            } else {
                None
            }
        } else {
            None
        };

        // If we found an expired entry, remove it with write lock
        if let Some(is_override) = lookup_result {
            if let Ok(mut cache) = self.inner.write() {
                cache.remove(hostname);
            }
            if is_override {
                return CacheLookup::ExpiredOverride(hostname.to_string());
            } else {
                return CacheLookup::Miss;
            }
        }

        // Try hostname mapping
        if let Ok(hostname_map) = self.hostname_map.read()
            && let Some(server_name) = hostname_map.get(hostname)
        {
            if let Some(resolution) = self.get(server_name) {
                return CacheLookup::Valid(resolution);
            }
            // If the mapping exists but the server_name entry is expired/missing,
            // treat it as an expired override
            return CacheLookup::ExpiredOverride(server_name.clone());
        }

        CacheLookup::Miss
    }

    fn set(&self, server_name: String, resolution: Resolution) {
        if let Ok(mut cache) = self.inner.write() {
            cache.insert(
                server_name.clone(),
                CacheEntry {
                    resolution: resolution.clone(),
                    expires_at: Instant::now() + self.ttl,
                    is_override: true, // All Matrix resolutions are overrides
                },
            );

            // Add hostname mapping for DNS lookups
            if let Ok(mut hostname_map) = self.hostname_map.write() {
                let sni_hostname = resolution.sni_hostname();
                if sni_hostname != server_name {
                    hostname_map.insert(sni_hostname, server_name);
                }
            }
        }
    }
}

#[derive(Clone)]
pub struct MatrixDnsResolver {
    resolver: Arc<TokioResolver>,
    cache: Cache,
    matrix_resolver: Arc<MatrixResolver>,
}

impl MatrixDnsResolver {
    pub(crate) fn new(
        resolver: Arc<TokioResolver>,
        cache: Cache,
        matrix_resolver: Arc<MatrixResolver>,
    ) -> Self {
        Self {
            resolver,
            cache,
            matrix_resolver,
        }
    }
}

impl reqwest::dns::Resolve for MatrixDnsResolver {
    fn resolve(&self, name: reqwest::dns::Name) -> reqwest::dns::Resolving {
        let name_str = name.as_str().to_string();
        let resolver = self.resolver.clone();
        let cache = self.cache.clone();
        let matrix_resolver = self.matrix_resolver.clone();

        Box::pin(async move {
            // Check cache and determine what to do
            match cache.lookup(&name_str) {
                CacheLookup::Valid(resolution) => {
                    // Valid cached entry - use it
                    if let Some(addr) = resolution.destination_addr(&resolver).await {
                        tracing::debug!("DNS cache hit for {} -> {}", name_str, addr);
                        return Ok(Box::new(std::iter::once(addr))
                            as Box<dyn Iterator<Item = SocketAddr> + Send>);
                    }
                }
                CacheLookup::ExpiredOverride(server_name) => {
                    // Expired Matrix override - refetch via Matrix resolution
                    tracing::debug!("DNS cache expired override for {}, refetching", name_str);
                    match matrix_resolver.resolve_server(&server_name).await {
                        Ok(resolution) => {
                            if let Some(addr) = resolution.destination_addr(&resolver).await {
                                return Ok(Box::new(std::iter::once(addr))
                                    as Box<dyn Iterator<Item = SocketAddr> + Send>);
                            } else {
                                // Something funky, they should re-resolve the server
                            }
                        }
                        Err(e) => {
                            tracing::warn!(
                                "Failed to refetch Matrix server {}: {}",
                                server_name,
                                e
                            );
                        }
                    }
                }
                CacheLookup::Miss => {
                    // No override - use standard DNS
                }
            }

            // Fallback: standard DNS lookup
            tracing::debug!("DNS fallback for {}, using standard DNS", name_str);
            match resolver.lookup_ip(&name_str).await {
                Ok(lookup) => {
                    let addrs: Vec<SocketAddr> = lookup
                        .iter()
                        .map(|ip| SocketAddr::new(ip, 8448)) // Default Matrix port
                        .collect();
                    Ok(Box::new(addrs.into_iter()) as Box<dyn Iterator<Item = SocketAddr> + Send>)
                }
                Err(e) => Err(Box::new(e) as Box<dyn std::error::Error + Send + Sync>),
            }
        })
    }
}

/// The main resolver struct for Matrix server resolution.
pub struct MatrixResolver {
    client: Client,
    resolver: Arc<TokioResolver>,
    cache: Cache,
}

impl MatrixResolver {
    /// Create a new MatrixResolver with default TTL of 5 minutes.
    pub async fn new() -> Result<Self, ResolveServerError> {
        Self::new_with_ttl(Duration::from_secs(300)).await
    }

    /// Create a new MatrixResolver with a custom cache TTL.
    pub async fn new_with_ttl(cache_ttl: Duration) -> Result<Self, ResolveServerError> {
        let resolver = Arc::new(hickory_resolver::Resolver::builder_tokio()?.build());

        let client = Client::builder()
            .timeout(std::time::Duration::from_secs(10))
            .build()?;

        let cache = Cache::new(cache_ttl);

        Ok(MatrixResolver {
            client,
            resolver,
            cache,
        })
    }

    /// Create a client with custom builder that can be reused for all Matrix servers.
    ///
    /// The client uses a custom DNS resolver that dynamically looks up Matrix servers
    /// from the cache, allowing one client to handle all federation requests.
    pub fn create_client_with_builder(
        self: &Arc<Self>,
        builder: reqwest::ClientBuilder,
    ) -> Result<Client, ResolveServerError> {
        let dns_resolver =
            MatrixDnsResolver::new(self.resolver.clone(), self.cache.clone(), self.clone());

        Ok(builder.dns_resolver(Arc::new(dns_resolver)).build()?)
    }

    /// Create a standard reqwest client that can be reused for all Matrix servers.
    pub fn create_client(self: &Arc<Self>) -> Result<Client, ResolveServerError> {
        let builder = Client::builder().timeout(std::time::Duration::from_secs(10));
        self.create_client_with_builder(builder)
    }

    /// Resolve a Matrix server name and return the Resolution.
    ///
    /// The returned Resolution can be used to construct URLs via `resolution.base_url()`.
    /// Results are cached with TTL for performance.
    ///
    /// After resolving, you should recreate the client to get updated DNS mappings.
    pub async fn resolve_server(
        &self,
        server_name: &str,
    ) -> Result<Resolution, ResolveServerError> {
        // Check cache first
        if let Some(resolution) = self.cache.get(server_name) {
            tracing::debug!("Cache hit for {}", server_name);
            return Ok(resolution);
        }

        // Perform resolution
        let resolution = self.resolve_actual_dest(server_name).await?;

        // Cache the result
        self.cache.set(server_name.to_string(), resolution.clone());

        Ok(resolution)
    }

    /// Resolve the actual destination according to Matrix spec.
    /// <https://matrix.org/docs/spec/server_server/r0.1.4#resolving-server-names>
    #[tracing::instrument(
        name = "actual",
        level = "debug",
        skip(self),
        fields(dest = %dest)
    )]
    async fn resolve_actual_dest(&self, dest: &str) -> Result<Resolution, ResolveServerError> {
        // 1. If the hostname is an IP literal
        if let Some((ip, port)) = get_ip_with_port(dest) {
            tracing::info!(
                ip = %ip,
                port = port,
                step = "ip_literal",
                "Resolved IP literal with port"
            );
            let socket = SocketAddr::new(ip, port.unwrap_or(8448));
            return Ok(Resolution {
                destination: ResolvedDestination::Literal(socket),
                host: dest.to_owned(),
            });
        }

        // 2. Hostname with explicit port
        if let Some(pos) = dest.find(':') {
            let (host_part, port_part) = dest.split_at(pos);
            let port_str = port_part.trim_start_matches(':');
            tracing::info!(
                host = %host_part,
                port = %port_str,
                step = "explicit_port",
                "Resolved hostname with explicit port"
            );
            return Ok(Resolution {
                destination: ResolvedDestination::Named(host_part.to_owned(), port_str.to_owned()),
                host: dest.to_owned(),
            });
        }

        // 3. Well-known delegation
        if let Some(res) = self.resolve_well_known(dest).await {
            tracing::info!(?res, step = "well_known", "Resolved .well-known delegation");
            match res {
                WellKnownServerResult::Ip(ip, port) => {
                    tracing::info!(
                        ip = %ip,
                        port = port.unwrap_or(8448),
                        step = "well_known_ip_literal",
                        "Resolved .well-known IP literal"
                    );
                    let socket = SocketAddr::new(ip, port.unwrap_or(8448));
                    return Ok(Resolution {
                        destination: ResolvedDestination::Literal(socket),
                        host: dest.to_owned(),
                    });
                }
                WellKnownServerResult::Domain(domain, None) => {
                    // 3.3/3.4: Hostname, no port in .well-known
                    if let Some((srv_host, srv_port)) = self.query_srv_record(&domain).await? {
                        tracing::info!(
                            srv_host = %srv_host,
                            srv_port = srv_port,
                            step = "well_known_host_srv",
                            "Resolved SRV from .well-known hostname without port"
                        );
                        return Ok(Resolution {
                            destination: ResolvedDestination::Named(srv_host, srv_port.to_string()),
                            host: domain,
                        });
                    } else {
                        // 3.5: No SRV, fallback to A/AAAA/CNAME + 8448
                        tracing::debug!(
                            delegated = %domain,
                            step = "well_known_fallback",
                            "Fallback to .well-known host with default port"
                        );
                        return Ok(Resolution {
                            destination: ResolvedDestination::Named(domain, "8448".to_owned()),
                            host: dest.to_owned(),
                        });
                    }
                }
                WellKnownServerResult::Domain(domain, Some(port)) => {
                    tracing::info!(
                        domain = %domain,
                        port = port,
                        step = "well_known_domain",
                        "Resolved .well-known domain with port"
                    );
                    return Ok(Resolution {
                        destination: ResolvedDestination::Named(domain, port.to_string()),
                        host: dest.to_owned(),
                    });
                }
            }
        }

        // 4. SRV lookup on original hostname
        if let Some((srv_host, srv_port)) = self.query_srv_record(dest).await? {
            tracing::debug!(
                srv_host = %srv_host,
                srv_port = srv_port,
                step = "srv_lookup",
                "Resolved SRV record on original hostname"
            );
            return Ok(Resolution {
                destination: ResolvedDestination::Named(srv_host, srv_port.to_string()),
                host: dest.to_owned(),
            });
        }

        // 5. Fallback: A/AAAA/CNAME + 8448
        tracing::debug!(
            host = %dest,
            step = "fallback",
            "Fallback to original hostname with default port"
        );
        Ok(Resolution {
            destination: ResolvedDestination::Named(dest.to_owned(), "8448".to_owned()),
            host: dest.to_owned(),
        })
    }

    /// Resolve .well-known delegation for a hostname.
    #[tracing::instrument(
        level = "trace",
        skip(self),
        fields(hostname = %hostname)
    )]
    async fn resolve_well_known(&self, hostname: &str) -> Option<WellKnownServerResult> {
        #[derive(Deserialize)]
        struct WellKnown {
            #[serde(rename = "m.server")]
            m_server: String,
        }
        let url = format!("https://{hostname}/.well-known/matrix/server");
        tracing::debug!(url = %url, "Fetching .well-known matrix server");
        let resp = match self.client.get(&url).send().await {
            Ok(r) => r,
            Err(_) => return None,
        };
        if resp.status() != StatusCode::OK {
            return None;
        }
        let wk: WellKnown = match resp.json().await {
            Ok(wk) => wk,
            Err(e) => {
                tracing::warn!(
                    error = %e,
                    url = %url,
                    "Failed to parse .well-known matrix server JSON"
                );
                return None;
            }
        };
        if let Some((ip, port)) = get_ip_with_port(&wk.m_server) {
            tracing::debug!(
                ip = %ip,
                port = ?port,
                "Parsed .well-known matrix server IP and port"
            );
            return Some(WellKnownServerResult::Ip(ip, port));
        }
        let (host, port) = parse_server_name(&wk.m_server);
        tracing::debug!(
            well_known_host = %host,
            well_known_port = ?port,
            "Parsed .well-known matrix server domain"
        );
        Some(WellKnownServerResult::Domain(host, port))
    }

    /// Query SRV records for a hostname, returning (target, port) if found.
    #[tracing::instrument(
        level = "trace",
        skip(self),
        fields(hostname = %hostname)
    )]
    async fn query_srv_record(
        &self,
        hostname: &str,
    ) -> Result<Option<(String, u16)>, ResolveServerError> {
        let srv_names = [
            format!("_matrix-fed._tcp.{hostname}"),
            format!("_matrix._tcp.{hostname}"),
        ];
        for srv in &srv_names {
            tracing::trace!(srv = %srv, "Querying SRV record");
            let lookup = self.resolver.srv_lookup(srv).await;
            if let Ok(result) = lookup
                && let Some(record) = result.iter().next()
            {
                let target = record.target().to_utf8();
                let port = record.port();
                return Ok(Some((target.trim_end_matches('.').to_owned(), port)));
            }
        }
        tracing::trace!(hostname = %hostname, "No SRV records found for hostname");
        Ok(None)
    }
}

#[derive(Debug)]
enum WellKnownServerResult {
    Ip(IpAddr, Option<u16>),
    Domain(String, Option<u16>),
}

/// Parses a Matrix server name into (hostname, Option<port>)
#[tracing::instrument(
    name = "parse_server_name",
    level = "trace",
    fields(server_name = %server_name)
)]
fn parse_server_name(server_name: &str) -> (String, Option<u16>) {
    if let Some((host, port)) = server_name.rsplit_once(':')
        && let Ok(port) = u16::from_str(port)
    {
        return (host.to_string(), Some(port));
    }
    (server_name.to_string(), None)
}

/// If the string is an IP literal (with optional port), returns (IpAddr, port).
#[tracing::instrument(
    name = "get_ip_with_port",
    level = "trace",
    fields(input = %s)
)]
fn get_ip_with_port(s: &str) -> Option<(IpAddr, Option<u16>)> {
    // Try SocketAddr first (IP:port)
    if let Ok(sock) = SocketAddr::from_str(s) {
        tracing::debug!(
            ip = %sock.ip(),
            port = sock.port(),
            "Parsed SocketAddr from input"
        );
        return Some((sock.ip(), Some(sock.port())));
    }
    // Try IP only
    if let Ok(ip) = IpAddr::from_str(s) {
        tracing::debug!(
            ip = %ip,
            port = 8448,
            "Parsed IpAddr from input, using default port"
        );
        return Some((ip, None));
    }
    tracing::debug!(input = %s, "Input is not an IP literal");
    None
}

#[cfg(test)]
mod tests {
    use rstest::rstest;
    use tracing_subscriber::{layer::SubscriberExt, util::SubscriberInitExt};

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

    #[tokio::test]
    async fn test_resolve() {
        init_tracing();
        let resolver = MatrixResolver::new().await.unwrap();
        let _ = dbg!(resolver.resolve_server("matrix.org").await.unwrap());
        let _ = dbg!(resolver.resolve_server("ellis.link").await.unwrap());
    }

    /// Helper function to initialize tracing for tests
    fn init_tracing() {
        let _ = tracing_subscriber::registry()
            .with(
                tracing_subscriber::fmt::layer()
                    .with_test_writer()
                    .with_target(false),
            )
            .try_init();
    }

    /// Parameterized test for server resolution.
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
    async fn test_server_resolver(#[case] server_name: &str) {
        init_tracing();

        let resolver = Arc::new(MatrixResolver::new().await.unwrap());

        tracing::info!("Testing {}/_matrix/federation/v1/version", server_name);

        // Resolve server
        let resolution = resolver.resolve_server(server_name).await.unwrap();

        // Create client with custom DNS resolver
        let builder = Client::builder()
            .tls_danger_accept_invalid_certs(true)
            .timeout(std::time::Duration::from_secs(10));
        let client = resolver.create_client_with_builder(builder).unwrap();

        // Build URL using the resolution's base_url
        let url = format!("{}/_matrix/federation/v1/version", resolution.base_url());
        let request = client.get(&url).build().unwrap();

        let response = client.execute(request).await;

        match response {
            Ok(resp) => {
                let status = resp.status();
                let text = resp.text().await.unwrap_or_default();
                tracing::debug!("Response status: {}, body: {}", status, text);

                if status == StatusCode::OK {
                    tracing::info!(
                        "✓ Successfully fetched federation version from {}",
                        server_name
                    );
                } else {
                    tracing::warn!(
                        "Server {} returned non-200 status: {}. Body: {}.",
                        server_name,
                        status,
                        text
                    );
                    panic!();
                }
            }
            Err(e) => {
                tracing::warn!(
                    "Failed to fetch federation version from {}: {}",
                    server_name,
                    e
                );
                panic!();
            }
        }
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
        init_tracing();

        let resolver = MatrixResolver::new().await.unwrap();
        let resolution = resolver.resolve_server(server_name).await;

        assert!(
            resolution.is_ok(),
            "Failed to resolve {}: {:?}",
            server_name,
            resolution.err()
        );

        let resolved = resolution.unwrap();
        tracing::info!(
            "Resolved {} to destination: {:?}, host: {}",
            server_name,
            resolved.destination,
            resolved.host
        );

        // Verify the resolution contains valid data
        match &resolved.destination {
            ResolvedDestination::Literal(addr) => {
                assert!(addr.port() > 0, "Port should be greater than 0");
            }
            ResolvedDestination::Named(host, port) => {
                assert!(!host.is_empty(), "Host should not be empty");
                assert!(!port.is_empty(), "Port should not be empty");
                let port_num: u16 = port.parse().expect("Port should be a valid number");
                assert!(port_num > 0, "Port should be greater than 0");
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
        init_tracing();

        let resolver = MatrixResolver::new().await.unwrap();
        let resolution = resolver.resolve_server(server_name).await;

        assert!(
            resolution.is_ok(),
            "Failed to resolve {}: {:?}",
            server_name,
            resolution.err()
        );

        let resolved = resolution.unwrap();

        // When a port is explicitly specified, it should be preserved
        match &resolved.destination {
            ResolvedDestination::Named(_, port) => {
                let expected_port = server_name.split(':').nth(1).unwrap();
                assert_eq!(
                    port, expected_port,
                    "Port should match the explicit port in server name"
                );
            }
            ResolvedDestination::Literal(addr) => {
                let expected_port: u16 = server_name.split(':').nth(1).unwrap().parse().unwrap();
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
        init_tracing();

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
                // If no port specified, should default to 8448
                if !server_name.contains(':') {
                    assert_eq!(addr.port(), 8448, "Should default to port 8448");
                }
            }
            _ => panic!("IP literal should resolve to Literal variant"),
        }
    }

    /// Demonstrate reuse of the same client across different resolutions
    #[tokio::test]
    async fn test_client_reuse() {
        init_tracing();

        let resolver = Arc::new(MatrixResolver::new().await.unwrap());

        // Create ONE client that will be reused for all servers
        let builder = Client::builder()
            .tls_danger_accept_invalid_certs(true)
            .timeout(std::time::Duration::from_secs(10));
        let client = resolver.create_client_with_builder(builder).unwrap();

        // Test multiple servers with the SAME client
        let servers = vec!["matrix.org", "nexy7574.co.uk"];

        for server_name in servers {
            tracing::info!("Testing {} with reused client", server_name);

            // Resolve the server
            let resolution = resolver.resolve_server(server_name).await.unwrap();

            // Make a request
            let url = format!("{}/_matrix/federation/v1/version", resolution.base_url());
            let response = client.get(&url).send().await;

            match response {
                Ok(resp) => {
                    let status = resp.status();
                    tracing::info!("✓ {} returned status {}", server_name, status);
                    assert_eq!(status, StatusCode::OK);
                }
                Err(e) => {
                    tracing::warn!("Failed to fetch from {}: {}", server_name, e);
                    panic!("Request failed");
                }
            }
        }
    }
}
