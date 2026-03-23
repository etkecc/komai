// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::io::{BufRead, BufReader, Read, Write};

use serde_json::Value;

use super::protocol;

pub trait Backend {
    fn call(&self, method: &str, params: Value) -> Result<Value, String>;
}

#[derive(Clone, Debug)]
pub struct KomaiIpcClient {
    profile: String,
}

impl KomaiIpcClient {
    pub fn new(profile: impl Into<String>) -> Self {
        Self {
            profile: profile.into(),
        }
    }
}

fn call_over_stream(mut stream: impl Read + Write, method: &str, params: Value) -> Result<Value, String> {
    let request = protocol::build_request(method, &params);
    let mut encoded =
        serde_json::to_vec(&request).map_err(|error| format!("failed to encode request: {error}"))?;
    encoded.push(b'\n');

    stream
        .write_all(&encoded)
        .map_err(|error| format!("write failed: {error}"))?;
    stream
        .flush()
        .map_err(|error| format!("write failed: {error}"))?;

    let mut reader = BufReader::new(stream);
    let mut response_line = String::new();
    let bytes_read = reader
        .read_line(&mut response_line)
        .map_err(|error| format!("read failed: {error}"))?;
    if bytes_read == 0 {
        return Err("read failed: empty response".to_owned());
    }

    let response: Value =
        serde_json::from_str(response_line.trim_end()).map_err(|error| format!("invalid response: {error}"))?;
    protocol::parse_response(response)
}

impl Backend for KomaiIpcClient {
    fn call(&self, method: &str, params: Value) -> Result<Value, String> {
        #[cfg(unix)]
        {
            let stream = super::unix::connect(&self.profile)?;
            return call_over_stream(stream, method, params);
        }

        #[cfg(windows)]
        {
            let stream = super::windows::connect(&self.profile)?;
            return call_over_stream(stream, method, params);
        }

        #[cfg(not(any(unix, windows)))]
        {
            let _ = method;
            let _ = params;
            Err("Komai MCP is not supported on this platform.".to_owned())
        }
    }
}

#[cfg(test)]
mod tests {
    use std::collections::VecDeque;
    use std::io::{self, Cursor, Read, Write};

    use serde_json::json;

    use super::call_over_stream;

    struct MockStream {
        response: Cursor<Vec<u8>>,
        writes: VecDeque<u8>,
    }

    impl MockStream {
        fn new(response: &str) -> Self {
            Self {
                response: Cursor::new(response.as_bytes().to_vec()),
                writes: VecDeque::new(),
            }
        }

        fn written(&self) -> Vec<u8> {
            self.writes.iter().copied().collect()
        }
    }

    impl Read for MockStream {
        fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
            self.response.read(buf)
        }
    }

    impl Write for MockStream {
        fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
            self.writes.extend(buf.iter().copied());
            Ok(buf.len())
        }

        fn flush(&mut self) -> io::Result<()> {
            Ok(())
        }
    }

    #[test]
    fn call_over_stream_writes_json_line_and_parses_result() {
        let mut stream = MockStream::new("{\"result\":{\"ok\":true}}\n");
        let result = call_over_stream(&mut stream, "app.version", json!({"test": true})).unwrap();

        assert_eq!(result, json!({"ok": true}));
        assert_eq!(
            String::from_utf8(stream.written()).unwrap(),
            "{\"method\":\"app.version\",\"params\":{\"test\":true}}\n"
        );
    }

    #[test]
    fn call_over_stream_rejects_empty_response() {
        let error = call_over_stream(MockStream::new(""), "app.version", json!(null)).unwrap_err();
        assert_eq!(error, "read failed: empty response");
    }
}
