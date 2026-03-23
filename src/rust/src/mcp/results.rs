// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_json::{json, Value};

pub fn empty_result() -> Value {
    json!({})
}

pub fn text_content(text: impl Into<String>) -> Value {
    json!({
        "type": "text",
        "text": text.into(),
    })
}

pub fn image_content(mime_type: &str, data: impl Into<String>) -> Value {
    json!({
        "type": "image",
        "mimeType": mime_type,
        "data": data.into(),
    })
}

pub fn tool_success(structured_content: Value, content: Vec<Value>) -> Value {
    json!({
        "structuredContent": structured_content,
        "content": content,
    })
}

pub fn tool_error(message: impl Into<String>) -> Value {
    json!({
        "content": [text_content(message)],
        "isError": true,
    })
}
