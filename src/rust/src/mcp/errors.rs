// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_json::{json, Value};

pub const INVALID_REQUEST: i64 = -32600;
pub const METHOD_NOT_FOUND: i64 = -32601;
pub const INVALID_PARAMS: i64 = -32602;
pub const INTERNAL_ERROR: i64 = -32603;

pub fn error_response(id: Option<Value>, code: i64, message: impl Into<String>, data: Option<Value>) -> Value {
    let mut error = json!({
        "code": code,
        "message": message.into(),
    });
    if let Some(data) = data {
        error
            .as_object_mut()
            .expect("error JSON object")
            .insert("data".to_owned(), data);
    }

    let mut response = json!({
        "jsonrpc": "2.0",
        "error": error,
    });
    if let Some(id) = id {
        response
            .as_object_mut()
            .expect("error response JSON object")
            .insert("id".to_owned(), id);
    }

    response
}

pub fn invalid_request(id: Option<Value>, message: impl Into<String>) -> Value {
    error_response(id, INVALID_REQUEST, message, None)
}

pub fn invalid_params(id: Option<Value>, message: impl Into<String>) -> Value {
    error_response(id, INVALID_PARAMS, message, None)
}

pub fn method_not_found(id: Option<Value>, message: impl Into<String>) -> Value {
    error_response(id, METHOD_NOT_FOUND, message, None)
}

pub fn internal_error(id: Option<Value>, message: impl Into<String>) -> Value {
    error_response(id, INTERNAL_ERROR, message, None)
}
