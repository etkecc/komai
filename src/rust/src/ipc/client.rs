// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::io::{BufRead, BufReader, Write};

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

impl Backend for KomaiIpcClient {
    fn call(&self, method: &str, params: Value) -> Result<Value, String> {
        #[cfg(unix)]
        {
            let mut stream = super::unix::connect(&self.profile)?;

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

            let response: Value = serde_json::from_str(response_line.trim_end())
                .map_err(|error| format!("invalid response: {error}"))?;
            protocol::parse_response(response)
        }

        #[cfg(not(unix))]
        {
            let _ = method;
            let _ = params;
            Err("Komai MCP is not supported on this platform yet. Windows named-pipe IPC support is a follow-up milestone.".to_owned())
        }
    }
}
