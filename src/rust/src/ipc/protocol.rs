// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_json::{json, Value};

pub fn build_request(method: &str, params: &Value) -> Value {
    if params.is_null() {
        json!({
            "method": method,
        })
    } else {
        json!({
            "method": method,
            "params": params,
        })
    }
}

pub fn parse_response(response: Value) -> Result<Value, String> {
    let object = response
        .as_object()
        .ok_or_else(|| "invalid response".to_owned())?;

    if let Some(error) = object.get("error").and_then(Value::as_str) {
        return Err(error.to_owned());
    }

    object
        .get("result")
        .cloned()
        .ok_or_else(|| "invalid response".to_owned())
}
