// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::io::{self, BufRead, Write};

use serde_json::{json, Value};

use crate::ipc::client::{Backend, KomaiIpcClient};

use super::errors;
use super::results;
use super::tools::{self, AccessMode, CallToolError};

const SUPPORTED_PROTOCOL_VERSIONS: &[&str] =
    &["2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05"];

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ServerConfig {
    pub profile: String,
    pub access_mode: AccessMode,
}

#[derive(Default)]
struct ConnectionState {
    initialize_seen: bool,
    initialized_notification_seen: bool,
}

pub fn serve_stdio(config: ServerConfig) -> Result<(), String> {
    let backend = KomaiIpcClient::new(config.profile.clone());
    serve_stdio_with_backend(config, &backend)
}

fn serve_stdio_with_backend(config: ServerConfig, backend: &dyn Backend) -> Result<(), String> {
    let stdin = io::stdin();
    let stdout = io::stdout();
    let mut reader = stdin.lock();
    let mut writer = stdout.lock();
    let mut state = ConnectionState::default();
    let mut line = String::new();

    loop {
        line.clear();
        let bytes_read = reader
            .read_line(&mut line)
            .map_err(|error| format!("failed to read MCP request: {error}"))?;
        if bytes_read == 0 {
            return Ok(());
        }

        let request_line = line.trim();
        if request_line.is_empty() {
            continue;
        }

        if let Some(response) = handle_json_line(request_line, &config, &mut state, backend) {
            write_response(&mut writer, &response)
                .map_err(|error| format!("failed to write MCP response: {error}"))?;
        }
    }
}

fn write_response(writer: &mut impl Write, response: &Value) -> io::Result<()> {
    serde_json::to_writer(&mut *writer, response)?;
    writer.write_all(b"\n")?;
    writer.flush()
}

fn handle_json_line(
    request_line: &str,
    config: &ServerConfig,
    state: &mut ConnectionState,
    backend: &dyn Backend,
) -> Option<Value> {
    let request: Value = match serde_json::from_str(request_line) {
        Ok(request) => request,
        Err(error) => {
            return Some(errors::invalid_request(
                None,
                format!("Invalid JSON-RPC payload: {error}"),
            ))
        }
    };

    let Some(object) = request.as_object() else {
        return Some(errors::invalid_request(
            None,
            "JSON-RPC payload must be an object.",
        ));
    };

    let id = object.get("id").cloned();
    if object.get("jsonrpc").and_then(Value::as_str) != Some("2.0") {
        return Some(errors::invalid_request(
            id,
            "JSON-RPC version must be '2.0'.",
        ));
    }

    let Some(method) = object.get("method").and_then(Value::as_str) else {
        return Some(errors::invalid_request(
            id,
            "JSON-RPC request is missing a string method name.",
        ));
    };

    if object.get("id").is_none() {
        handle_notification(method, state);
        return None;
    }

    Some(handle_request(
        id.unwrap_or(Value::Null),
        method,
        object.get("params"),
        config,
        state,
        backend,
    ))
}

fn handle_notification(method: &str, state: &mut ConnectionState) {
    if method == "notifications/initialized" {
        state.initialized_notification_seen = true;
    }
}

fn handle_request(
    id: Value,
    method: &str,
    params: Option<&Value>,
    config: &ServerConfig,
    state: &mut ConnectionState,
    backend: &dyn Backend,
) -> Value {
    match method {
        "initialize" => handle_initialize(id, params, config, state),
        "ping" => result_response(id, results::empty_result()),
        "tools/list" => {
            if !state.initialize_seen {
                return errors::invalid_request(
                    Some(id),
                    "The client must initialize the MCP session before calling tools/list.",
                );
            }

            result_response(
                id,
                json!({
                    "tools": tools::list_tools(config.access_mode),
                }),
            )
        }
        "tools/call" => {
            if !state.initialize_seen {
                return errors::invalid_request(
                    Some(id),
                    "The client must initialize the MCP session before calling tools/call.",
                );
            }

            let Some(params) = params.and_then(Value::as_object) else {
                return errors::invalid_params(
                    Some(id),
                    "tools/call requires an object with 'name' and optional 'arguments'.",
                );
            };
            if params.get("task").is_some() {
                return errors::invalid_params(
                    Some(id),
                    "Task-augmented tools/call is not supported by this server.",
                );
            }

            let Some(name) = params.get("name").and_then(Value::as_str) else {
                return errors::invalid_params(
                    Some(id),
                    "tools/call requires a string 'name'.",
                );
            };

            match tools::call_tool(
                backend,
                config.access_mode,
                name,
                params.get("arguments").cloned(),
            ) {
                Ok(result) => result_response(id, result),
                Err(CallToolError::InvalidParams(message)) => {
                    errors::invalid_params(Some(id), message)
                }
                Err(CallToolError::UnknownTool(name)) => {
                    errors::invalid_params(Some(id), format!("Unknown tool: {name}"))
                }
            }
        }
        _ => errors::method_not_found(Some(id), format!("Method not found: {method}")),
    }
}

fn handle_initialize(
    id: Value,
    params: Option<&Value>,
    config: &ServerConfig,
    state: &mut ConnectionState,
) -> Value {
    let Some(params) = params.and_then(Value::as_object) else {
        return errors::invalid_params(
            Some(id),
            "initialize requires protocolVersion, capabilities, and clientInfo.",
        );
    };

    let Some(requested_protocol_version) =
        params.get("protocolVersion").and_then(Value::as_str)
    else {
        return errors::invalid_params(
            Some(id),
            "initialize requires a string protocolVersion.",
        );
    };

    if !SUPPORTED_PROTOCOL_VERSIONS.contains(&requested_protocol_version) {
        return errors::error_response(
            Some(id),
            errors::INVALID_PARAMS,
            "Unsupported protocol version",
            Some(json!({
                "supported": SUPPORTED_PROTOCOL_VERSIONS,
                "requested": requested_protocol_version,
            })),
        );
    }

    state.initialize_seen = true;

    result_response(
        id,
        json!({
            "protocolVersion": requested_protocol_version,
            "capabilities": {
                "tools": {
                    "listChanged": false
                }
            },
            "serverInfo": {
                "name": "komai-mcp",
                "title": "Komai MCP",
                "version": env!("CARGO_PKG_VERSION"),
                "description": "Model Context Protocol frontend for a running Komai profile."
            },
            "instructions": format!(
                "This server exposes Komai automation tools for the '{}' profile over stdio. Start that Komai profile before calling tools. The current access mode is {}.",
                config.profile,
                config.access_mode.as_str()
            )
        }),
    )
}

fn result_response(id: Value, result: Value) -> Value {
    json!({
        "jsonrpc": "2.0",
        "id": id,
        "result": result,
    })
}

#[cfg(test)]
mod tests {
    use super::{handle_json_line, ConnectionState, ServerConfig};
    use crate::ipc::client::Backend;
    use crate::mcp::tools::AccessMode;
    use serde_json::{json, Value};

    struct MockBackend;

    impl Backend for MockBackend {
        fn call(&self, method: &str, _params: Value) -> Result<Value, String> {
            match method {
                "rooms.list" => Ok(json!([])),
                _ => Ok(Value::Null),
            }
        }
    }

    fn config() -> ServerConfig {
        ServerConfig {
            profile: "default".to_owned(),
            access_mode: AccessMode::ReadOnly,
        }
    }

    #[test]
    fn initialize_returns_server_capabilities() {
        let backend = MockBackend;
        let mut state = ConnectionState::default();

        let response = handle_json_line(
            r#"{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"tester","version":"1.0.0"}}}"#,
            &config(),
            &mut state,
            &backend,
        )
        .unwrap();

        assert_eq!(response["result"]["protocolVersion"].as_str(), Some("2025-11-25"));
        assert!(response["result"]["capabilities"]["tools"].is_object());
        assert_eq!(response["result"]["serverInfo"]["name"].as_str(), Some("komai-mcp"));
    }

    #[test]
    fn tools_list_requires_initialize() {
        let backend = MockBackend;
        let mut state = ConnectionState::default();

        let response = handle_json_line(
            r#"{"jsonrpc":"2.0","id":2,"method":"tools/list"}"#,
            &config(),
            &mut state,
            &backend,
        )
        .unwrap();

        assert_eq!(response["error"]["code"].as_i64(), Some(-32600));
    }

    #[test]
    fn initialized_notification_updates_state_without_response() {
        let backend = MockBackend;
        let mut state = ConnectionState::default();

        let response = handle_json_line(
            r#"{"jsonrpc":"2.0","method":"notifications/initialized"}"#,
            &config(),
            &mut state,
            &backend,
        );

        assert!(response.is_none());
        assert!(state.initialized_notification_seen);
    }
}
