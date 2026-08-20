// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_json::{json, Map, Value};

use crate::ipc::client::Backend;

use super::results;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum AccessMode {
    ReadOnly,
    ReadWrite,
}

impl AccessMode {
    pub fn parse(value: &str) -> Option<Self> {
        match value {
            "read_only" => Some(Self::ReadOnly),
            "read_write" => Some(Self::ReadWrite),
            _ => None,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
            Self::ReadOnly => "read_only",
            Self::ReadWrite => "read_write",
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum ToolAccess {
    Read,
    Write,
}

#[derive(Debug, Eq, PartialEq)]
pub enum CallToolError {
    InvalidParams(String),
    UnknownTool(String),
}

#[derive(Clone, Copy)]
struct ToolDefinition {
    name: &'static str,
    title: &'static str,
    description: &'static str,
    access: ToolAccess,
    destructive: bool,
    idempotent: bool,
    open_world: bool,
    input_schema: fn() -> Value,
    output_schema: fn() -> Value,
    handler: fn(&dyn Backend, &Map<String, Value>) -> Result<ToolSuccess, ToolFailure>,
}

struct ToolSuccess {
    structured_content: Value,
    content: Vec<Value>,
}

enum ToolFailure {
    InvalidParams(String),
    ToolError(String),
}

impl ToolDefinition {
    fn visible_in(self, access_mode: AccessMode) -> bool {
        match (access_mode, self.access) {
            (AccessMode::ReadOnly, ToolAccess::Write) => false,
            _ => true,
        }
    }
}

pub fn list_tools(access_mode: AccessMode) -> Vec<Value> {
    TOOLS
        .iter()
        .copied()
        .filter(|tool| tool.visible_in(access_mode))
        .map(tool_descriptor)
        .collect()
}

pub fn call_tool(
    backend: &dyn Backend,
    access_mode: AccessMode,
    name: &str,
    arguments: Option<Value>,
) -> Result<Value, CallToolError> {
    let tool = TOOLS
        .iter()
        .copied()
        .find(|tool| tool.name == name)
        .ok_or_else(|| CallToolError::UnknownTool(name.to_owned()))?;

    let arguments = match arguments {
        None => Map::new(),
        Some(Value::Object(object)) => object,
        Some(_) => {
            return Err(CallToolError::InvalidParams(
                "Tool arguments must be an object.".to_owned(),
            ))
        }
    };

    if access_mode == AccessMode::ReadOnly && tool.access == ToolAccess::Write {
        return Ok(results::tool_error(format!(
            "Tool '{name}' is unavailable while the server is running in read_only mode."
        )));
    }

    match (tool.handler)(backend, &arguments) {
        Ok(success) => Ok(results::tool_success(
            success.structured_content,
            success.content,
        )),
        Err(ToolFailure::InvalidParams(message)) => Err(CallToolError::InvalidParams(message)),
        Err(ToolFailure::ToolError(message)) => Ok(results::tool_error(message)),
    }
}

fn tool_descriptor(tool: ToolDefinition) -> Value {
    let mut annotations = json!({
        "title": tool.title,
        "readOnlyHint": tool.access == ToolAccess::Read,
        "openWorldHint": tool.open_world,
    });

    if tool.access == ToolAccess::Write {
        annotations
            .as_object_mut()
            .expect("tool annotations JSON object")
            .insert("destructiveHint".to_owned(), Value::Bool(tool.destructive));
        annotations
            .as_object_mut()
            .expect("tool annotations JSON object")
            .insert("idempotentHint".to_owned(), Value::Bool(tool.idempotent));
    }

    json!({
        "name": tool.name,
        "title": tool.title,
        "description": tool.description,
        "inputSchema": (tool.input_schema)(),
        "outputSchema": (tool.output_schema)(),
        "annotations": annotations,
    })
}

fn success(structured_content: Value, content: Vec<Value>) -> Result<ToolSuccess, ToolFailure> {
    Ok(ToolSuccess {
        structured_content,
        content,
    })
}

fn reject_unknown_keys(arguments: &Map<String, Value>, allowed: &[&str]) -> Result<(), ToolFailure> {
    let unknown: Vec<&str> = arguments
        .keys()
        .filter(|key| !allowed.iter().any(|allowed_key| allowed_key == &key.as_str()))
        .map(|key| key.as_str())
        .collect();

    if unknown.is_empty() {
        Ok(())
    } else {
        Err(ToolFailure::InvalidParams(format!(
            "Unexpected arguments: {}.",
            unknown.join(", ")
        )))
    }
}

fn require_string(arguments: &Map<String, Value>, key: &str) -> Result<String, ToolFailure> {
    match arguments.get(key) {
        Some(Value::String(value)) => Ok(value.clone()),
        Some(_) => Err(ToolFailure::InvalidParams(format!(
            "Argument '{key}' must be a string."
        ))),
        None => Err(ToolFailure::InvalidParams(format!(
            "Missing required argument '{key}'."
        ))),
    }
}

fn require_non_empty_string(
    arguments: &Map<String, Value>,
    key: &str,
) -> Result<String, ToolFailure> {
    let value = require_string(arguments, key)?;
    if value.trim().is_empty() {
        return Err(ToolFailure::InvalidParams(format!(
            "Argument '{key}' must not be empty."
        )));
    }
    Ok(value)
}

fn optional_string(arguments: &Map<String, Value>, key: &str) -> Result<Option<String>, ToolFailure> {
    match arguments.get(key) {
        Some(Value::String(value)) => Ok(Some(value.clone())),
        Some(Value::Null) => Ok(None),
        Some(_) => Err(ToolFailure::InvalidParams(format!(
            "Argument '{key}' must be a string when provided."
        ))),
        None => Ok(None),
    }
}

fn optional_bool(arguments: &Map<String, Value>, key: &str) -> Result<Option<bool>, ToolFailure> {
    match arguments.get(key) {
        Some(Value::Bool(value)) => Ok(Some(*value)),
        Some(Value::Null) => Ok(None),
        Some(_) => Err(ToolFailure::InvalidParams(format!(
            "Argument '{key}' must be a boolean when provided."
        ))),
        None => Ok(None),
    }
}

fn optional_integer(arguments: &Map<String, Value>, key: &str) -> Result<Option<i64>, ToolFailure> {
    match arguments.get(key) {
        Some(Value::Number(value)) => value.as_i64().map(Some).ok_or_else(|| {
            ToolFailure::InvalidParams(format!(
                "Argument '{key}' must be an integer when provided."
            ))
        }),
        Some(Value::Null) => Ok(None),
        Some(_) => Err(ToolFailure::InvalidParams(format!(
            "Argument '{key}' must be an integer when provided."
        ))),
        None => Ok(None),
    }
}

fn optional_object(arguments: &Map<String, Value>, key: &str) -> Result<Option<Map<String, Value>>, ToolFailure> {
    match arguments.get(key) {
        Some(Value::Object(value)) => Ok(Some(value.clone())),
        Some(Value::Null) => Ok(None),
        Some(_) => Err(ToolFailure::InvalidParams(format!(
            "Argument '{key}' must be an object when provided."
        ))),
        None => Ok(None),
    }
}

fn backend_call(backend: &dyn Backend, method: &str, params: Value) -> Result<Value, ToolFailure> {
    backend.call(method, params).map_err(ToolFailure::ToolError)
}

fn backend_string(backend: &dyn Backend, method: &str, params: Value) -> Result<String, ToolFailure> {
    let result = backend_call(backend, method, params)?;
    result.as_str().map(str::to_owned).ok_or_else(|| {
        ToolFailure::ToolError(format!(
            "Komai IPC returned an invalid string result for '{method}'."
        ))
    })
}

fn backend_bool_true(backend: &dyn Backend, method: &str, params: Value) -> Result<(), ToolFailure> {
    let result = backend_call(backend, method, params)?;
    if result == Value::Bool(true) {
        Ok(())
    } else {
        Err(ToolFailure::ToolError(format!(
            "Komai IPC returned an invalid success result for '{method}'."
        )))
    }
}

fn backend_object(backend: &dyn Backend, method: &str, params: Value) -> Result<Map<String, Value>, ToolFailure> {
    let result = backend_call(backend, method, params)?;
    result.as_object().cloned().ok_or_else(|| {
        ToolFailure::ToolError(format!(
            "Komai IPC returned an invalid object result for '{method}'."
        ))
    })
}

fn backend_array(backend: &dyn Backend, method: &str, params: Value) -> Result<Vec<Value>, ToolFailure> {
    let result = backend_call(backend, method, params)?;
    result.as_array().cloned().ok_or_else(|| {
        ToolFailure::ToolError(format!(
            "Komai IPC returned an invalid array result for '{method}'."
        ))
    })
}

fn object_schema(properties: Vec<(&str, Value)>, required: &[&str]) -> Value {
    let mut property_map = Map::new();
    for (name, schema) in properties {
        property_map.insert(name.to_owned(), schema);
    }

    let mut schema = json!({
        "type": "object",
        "properties": property_map,
        "additionalProperties": false,
    });
    if !required.is_empty() {
        schema
            .as_object_mut()
            .expect("JSON schema object")
            .insert("required".to_owned(), json!(required));
    }

    schema
}

fn string_schema(description: &'static str) -> Value {
    json!({
        "type": "string",
        "description": description,
    })
}

fn boolean_schema(description: &'static str) -> Value {
    json!({
        "type": "boolean",
        "description": description,
    })
}

fn enum_string_schema(description: &'static str, values: &[&str]) -> Value {
    json!({
        "type": "string",
        "description": description,
        "enum": values,
    })
}

fn integer_schema(description: &'static str) -> Value {
    json!({
        "type": "integer",
        "description": description,
    })
}

fn generic_object_schema(description: &'static str) -> Value {
    json!({
        "type": "object",
        "description": description,
    })
}

fn string_array_schema(description: &'static str) -> Value {
    json!({
        "type": "array",
        "description": description,
        "items": {
            "type": "string",
        },
    })
}

fn membership_input_schema(reason_description: &'static str) -> Value {
    object_schema(
        vec![
            ("roomIdOrAlias", string_schema("Room ID or room alias.")),
            ("userId", string_schema("Matrix user ID to act on.")),
            ("reason", string_schema(reason_description)),
        ],
        &["roomIdOrAlias", "userId"],
    )
}

fn membership_output_schema() -> Value {
    object_schema(
        vec![
            ("ok", json!({"type": "boolean"})),
            ("roomIdOrAlias", string_schema("Room ID or alias that was acted on.")),
            ("userId", string_schema("Matrix user ID that was acted on.")),
        ],
        &["ok", "roomIdOrAlias", "userId"],
    )
}

fn room_info_schema() -> Value {
    object_schema(
        vec![
            ("id", string_schema("Matrix room ID.")),
            ("alias", string_schema("Primary room alias, if any.")),
            ("name", string_schema("Current room display name.")),
            ("avatarUrl", string_schema("Room avatar MXC URI, if any.")),
            (
                "read",
                boolean_schema(
                    "Whether Komai currently considers the room locally read.",
                ),
            ),
            (
                "unreadCount",
                integer_schema(
                    "Locally-tracked unread message count for the room.",
                ),
            ),
            ("memberCount", integer_schema("Joined member count for the room.")),
            (
                "mostRecentEventTimestampMs",
                integer_schema(
                    "Best-known most recent room event timestamp in Unix milliseconds.",
                ),
            ),
            ("highlighted", boolean_schema("Whether the room currently has a highlight.")),
            (
                "categories",
                string_array_schema(
                    "Derived room categories such as direct, person, bot, group, space, or encrypted.",
                ),
            ),
            ("tags", string_array_schema("Matrix room tags, including custom tags.")),
            (
                "parentSpaces",
                string_array_schema("Parent Matrix space room IDs for this room."),
            ),
            (
                "dmUserId",
                string_schema("Direct-chat partner user ID when the room is a DM, otherwise empty."),
            ),
            ("encrypted", boolean_schema("Whether the room is end-to-end encrypted.")),
        ],
        &[
            "id",
            "alias",
            "name",
            "avatarUrl",
            "read",
            "unreadCount",
            "memberCount",
            "mostRecentEventTimestampMs",
            "highlighted",
            "categories",
            "tags",
            "parentSpaces",
            "dmUserId",
            "encrypted",
        ],
    )
}

const TIMELINE_FETCH_MODE_CACHED_ONLY: &str = "cached_only";
const TIMELINE_FETCH_MODE_SERVER_IF_NEEDED: &str = "server_fetch_if_needed";
const DEFAULT_TIMELINE_LIMIT: i64 = 10;
const MAX_TIMELINE_LIMIT: i64 = 500;

fn nullable_string_schema(description: &'static str) -> Value {
    json!({
        "type": ["string", "null"],
        "description": description,
    })
}

fn timeline_event_schema() -> Value {
    json!({
        "type": "object",
        "description": "Serialized Matrix timeline event with original content and selected envelope fields.",
        "additionalProperties": true,
        "properties": {
            "content": {
                "type": "object",
                "description": "Original Matrix event content object.",
            },
            "event_id": {
                "type": "string",
                "description": "Matrix event ID.",
            },
            "origin_server_ts": {
                "type": "integer",
                "description": "Origin server timestamp in milliseconds.",
            },
            "sender": {
                "type": "string",
                "description": "Matrix user ID of the sender.",
            },
            "state_key": {
                "type": "string",
                "description": "Matrix state key when the event is a state event.",
            },
            "type": {
                "type": "string",
                "description": "Matrix event type.",
            }
        },
        "required": ["content", "event_id", "origin_server_ts", "sender", "type"],
    })
}

fn ok_with_key_output_schema(key: &'static str, description: &'static str) -> Value {
    object_schema(
        vec![
            ("ok", json!({"type": "boolean"})),
            (key, string_schema(description)),
        ],
        &["ok", key],
    )
}

fn getter_output_schema(key: &'static str, description: &'static str) -> Value {
    object_schema(vec![(key, string_schema(description))], &[key])
}

fn parse_timeline_limit(arguments: &Map<String, Value>) -> Result<i64, ToolFailure> {
    match optional_integer(arguments, "limit")? {
        None => Ok(DEFAULT_TIMELINE_LIMIT),
        Some(limit) if (1..=MAX_TIMELINE_LIMIT).contains(&limit) => Ok(limit),
        Some(_) => Err(ToolFailure::InvalidParams(format!(
            "Argument 'limit' must be between 1 and {MAX_TIMELINE_LIMIT}."
        ))),
    }
}

fn parse_timeline_fetch_mode(arguments: &Map<String, Value>) -> Result<String, ToolFailure> {
    match optional_string(arguments, "fetchMode")?.as_deref() {
        None | Some(TIMELINE_FETCH_MODE_CACHED_ONLY) => {
            Ok(TIMELINE_FETCH_MODE_CACHED_ONLY.to_owned())
        }
        Some(TIMELINE_FETCH_MODE_SERVER_IF_NEEDED) => {
            Ok(TIMELINE_FETCH_MODE_SERVER_IF_NEEDED.to_owned())
        }
        Some(_) => Err(ToolFailure::InvalidParams(format!(
            "Argument 'fetchMode' must be one of: {TIMELINE_FETCH_MODE_CACHED_ONLY}, {TIMELINE_FETCH_MODE_SERVER_IF_NEEDED}."
        ))),
    }
}

fn parse_msgtype(arguments: &Map<String, Value>) -> Result<String, ToolFailure> {
    match optional_string(arguments, "msgtype")?.as_deref() {
        None | Some("text") | Some("m.text") => Ok("m.text".to_owned()),
        Some("notice") | Some("m.notice") => Ok("m.notice".to_owned()),
        Some(_) => Err(ToolFailure::InvalidParams(
            "Argument 'msgtype' must be one of: text, notice, m.text, m.notice."
                .to_owned(),
        )),
    }
}

fn parse_format(arguments: &Map<String, Value>) -> Result<String, ToolFailure> {
    match optional_string(arguments, "format")?.as_deref() {
        None | Some("auto") => Ok("auto".to_owned()),
        Some("plain") => Ok("plain".to_owned()),
        Some("html") => Ok("html".to_owned()),
        Some(_) => Err(ToolFailure::InvalidParams(
            "Argument 'format' must be one of: auto, plain, html.".to_owned(),
        )),
    }
}

fn handle_app_get_version(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &[])?;
    let version = backend_string(backend, "app.version", Value::Null)?;
    success(
        json!({ "version": version }),
        vec![results::text_content(format!("Komai version: {version}."))],
    )
}

fn handle_app_get_api_version(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &[])?;
    let api_version = backend_string(backend, "app.apiVersion", Value::Null)?;
    success(
        json!({ "apiVersion": api_version }),
        vec![results::text_content(format!(
            "Komai automation API version: {api_version}."
        ))],
    )
}

fn handle_rooms_list(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &[])?;
    let rooms = backend_array(backend, "rooms.list", Value::Null)?;
    success(
        json!({ "rooms": rooms }),
        vec![results::text_content(format!(
            "Listed {} rooms.",
            rooms.len()
        ))],
    )
}

fn handle_rooms_get_timeline(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(
        arguments,
        &[
            "roomIdOrAlias",
            "limit",
            "beforeEventId",
            "includeUnsignedFields",
            "fetchMode",
        ],
    )?;
    let room_id_or_alias = require_non_empty_string(arguments, "roomIdOrAlias")?;
    let limit = parse_timeline_limit(arguments)?;
    let before_event_id = optional_string(arguments, "beforeEventId")?;
    let include_unsigned_fields = optional_bool(arguments, "includeUnsignedFields")?.unwrap_or(false);
    let fetch_mode = parse_timeline_fetch_mode(arguments)?;

    let mut params = Map::new();
    params.insert(
        "roomIdOrAlias".to_owned(),
        Value::String(room_id_or_alias.clone()),
    );
    params.insert("limit".to_owned(), Value::Number(limit.into()));
    if let Some(before_event_id) = before_event_id {
        params.insert("beforeEventId".to_owned(), Value::String(before_event_id));
    }
    if include_unsigned_fields {
        params.insert(
            "includeUnsignedFields".to_owned(),
            Value::Bool(include_unsigned_fields),
        );
    }
    params.insert("fetchMode".to_owned(), Value::String(fetch_mode));

    let result = backend_object(backend, "rooms.timeline", Value::Object(params))?;
    let event_count = result
        .get("events")
        .and_then(Value::as_array)
        .map(Vec::len)
        .unwrap_or(0);

    success(
        Value::Object(result),
        vec![results::text_content(format!(
            "Retrieved {event_count} timeline event(s) from {room_id_or_alias}."
        ))],
    )
}

fn handle_rooms_join(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &["roomIdOrAlias"])?;
    let room_id_or_alias = require_non_empty_string(arguments, "roomIdOrAlias")?;
    backend_bool_true(
        backend,
        "rooms.join",
        json!({ "roomIdOrAlias": room_id_or_alias }),
    )?;

    success(
        json!({
            "ok": true,
            "roomIdOrAlias": room_id_or_alias,
        }),
        vec![results::text_content(format!(
            "Requested a room join for {room_id_or_alias}."
        ))],
    )
}

fn handle_rooms_new_direct_chat(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &["userId"])?;
    let user_id = require_non_empty_string(arguments, "userId")?;
    backend_bool_true(
        backend,
        "rooms.newDirectChat",
        json!({ "userId": user_id }),
    )?;

    success(
        json!({
            "ok": true,
            "userId": user_id,
        }),
        vec![results::text_content(format!(
            "Opened or created a direct chat with {user_id}."
        ))],
    )
}

fn handle_rooms_send(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &["roomIdOrAlias", "body", "msgtype", "format"])?;
    let room_id_or_alias = require_non_empty_string(arguments, "roomIdOrAlias")?;
    let body = require_non_empty_string(arguments, "body")?;
    let msgtype = parse_msgtype(arguments)?;
    let format = parse_format(arguments)?;

    let result = backend_object(
        backend,
        "rooms.send",
        json!({
            "roomIdOrAlias": room_id_or_alias,
            "body": body,
            "msgtype": msgtype,
            "format": format,
        }),
    )?;

    success(
        Value::Object(result),
        vec![results::text_content(format!(
            "Sent a message to {room_id_or_alias}."
        ))],
    )
}

fn handle_rooms_send_image_file(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &["roomIdOrAlias", "path", "body"])?;
    let room_id_or_alias = require_non_empty_string(arguments, "roomIdOrAlias")?;
    let path = require_non_empty_string(arguments, "path")?;
    let body = optional_string(arguments, "body")?;

    let mut params = Map::new();
    params.insert("roomIdOrAlias".to_owned(), Value::String(room_id_or_alias.clone()));
    params.insert("path".to_owned(), Value::String(path.clone()));
    if let Some(body) = body {
        params.insert("body".to_owned(), Value::String(body));
    }

    let result = backend_object(backend, "rooms.sendImageFile", Value::Object(params))?;
    success(
        Value::Object(result),
        vec![results::text_content(format!(
            "Sent an image file from {path} to {room_id_or_alias}."
        ))],
    )
}

fn handle_rooms_send_image(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &["roomIdOrAlias", "mxcUri", "body", "filename", "info"])?;
    let room_id_or_alias = require_non_empty_string(arguments, "roomIdOrAlias")?;
    let mxc_uri = require_non_empty_string(arguments, "mxcUri")?;
    let body = optional_string(arguments, "body")?;
    let filename = optional_string(arguments, "filename")?;
    let info = optional_object(arguments, "info")?;

    let mut params = Map::new();
    params.insert("roomIdOrAlias".to_owned(), Value::String(room_id_or_alias.clone()));
    params.insert("mxcUri".to_owned(), Value::String(mxc_uri.clone()));
    if let Some(body) = body {
        params.insert("body".to_owned(), Value::String(body));
    }
    if let Some(filename) = filename {
        params.insert("filename".to_owned(), Value::String(filename));
    }
    if let Some(info) = info {
        params.insert("info".to_owned(), Value::Object(info));
    }

    let result = backend_object(backend, "rooms.sendImage", Value::Object(params))?;
    success(
        Value::Object(result),
        vec![results::text_content(format!(
            "Sent uploaded image {mxc_uri} to {room_id_or_alias}."
        ))],
    )
}

/// The four user-targeting membership tools differ only in the IPC method they
/// call and the sentence they report, so they share one handler body.
fn handle_membership_action(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
    method: &str,
    summary: fn(&str, &str) -> String,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &["roomIdOrAlias", "userId", "reason"])?;
    let room_id_or_alias = require_non_empty_string(arguments, "roomIdOrAlias")?;
    let user_id = require_non_empty_string(arguments, "userId")?;
    let reason = optional_string(arguments, "reason")?;

    let mut params = Map::new();
    params.insert(
        "roomIdOrAlias".to_owned(),
        Value::String(room_id_or_alias.clone()),
    );
    params.insert("userId".to_owned(), Value::String(user_id.clone()));
    if let Some(reason) = reason {
        params.insert("reason".to_owned(), Value::String(reason));
    }

    backend_bool_true(backend, method, Value::Object(params))?;

    success(
        json!({
            "ok": true,
            "roomIdOrAlias": room_id_or_alias,
            "userId": user_id,
        }),
        vec![results::text_content(summary(&user_id, &room_id_or_alias))],
    )
}

fn handle_rooms_invite(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    handle_membership_action(backend, arguments, "rooms.invite", |user, room| {
        format!("Invited {user} to {room}.")
    })
}

fn handle_rooms_kick(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    handle_membership_action(backend, arguments, "rooms.kick", |user, room| {
        format!("Removed {user} from {room}.")
    })
}

fn handle_rooms_ban(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    handle_membership_action(backend, arguments, "rooms.ban", |user, room| {
        format!("Banned {user} from {room}.")
    })
}

fn handle_rooms_unban(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    handle_membership_action(backend, arguments, "rooms.unban", |user, room| {
        format!("Lifted the ban on {user} in {room}.")
    })
}

fn handle_rooms_leave(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &["roomIdOrAlias", "reason"])?;
    let room_id_or_alias = require_non_empty_string(arguments, "roomIdOrAlias")?;
    let reason = optional_string(arguments, "reason")?;

    let mut params = Map::new();
    params.insert(
        "roomIdOrAlias".to_owned(),
        Value::String(room_id_or_alias.clone()),
    );
    if let Some(reason) = reason {
        params.insert("reason".to_owned(), Value::String(reason));
    }

    backend_bool_true(backend, "rooms.leave", Value::Object(params))?;

    success(
        json!({
            "ok": true,
            "roomIdOrAlias": room_id_or_alias,
        }),
        vec![results::text_content(format!("Left {room_id_or_alias}."))],
    )
}

fn handle_user_get_id(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &[])?;
    let user_id = backend_string(backend, "user.userId", Value::Null)?;
    success(
        json!({ "userId": user_id }),
        vec![results::text_content(format!("Current user ID: {user_id}."))],
    )
}

fn handle_user_get_homeserver_url(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &[])?;
    let homeserver_url = backend_string(backend, "user.homeserverUrl", Value::Null)?;
    success(
        json!({ "homeserverUrl": homeserver_url }),
        vec![results::text_content(format!(
            "Homeserver URL: {homeserver_url}."
        ))],
    )
}

fn handle_user_get_device_id(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &[])?;
    let device_id = backend_string(backend, "user.deviceId", Value::Null)?;
    success(
        json!({ "deviceId": device_id }),
        vec![results::text_content(format!("Device ID: {device_id}."))],
    )
}

fn handle_user_get_status_message(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &[])?;
    let status_message = backend_string(backend, "user.statusMessage", Value::Null)?;
    success(
        json!({ "statusMessage": status_message }),
        vec![results::text_content(format!(
            "Current status message: {status_message}"
        ))],
    )
}

fn handle_user_set_status_message(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &["message"])?;
    let message = require_string(arguments, "message")?;
    backend_bool_true(
        backend,
        "user.setStatusMessage",
        json!({ "message": message }),
    )?;

    success(
        json!({
            "ok": true,
            "statusMessage": message,
        }),
        vec![results::text_content("Updated the status message.")],
    )
}

fn handle_settings_ui_get_theme(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &[])?;
    let theme = backend_string(backend, "settings.ui.theme", Value::Null)?;
    success(
        json!({ "theme": theme }),
        vec![results::text_content(format!("Current theme: {theme}."))],
    )
}

fn handle_settings_ui_set_theme(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &["theme"])?;
    let theme = require_non_empty_string(arguments, "theme")?;
    backend_bool_true(
        backend,
        "settings.ui.setTheme",
        json!({ "theme": theme }),
    )?;

    success(
        json!({
            "ok": true,
            "theme": theme,
        }),
        vec![results::text_content(format!("Set the active theme to {theme}."))],
    )
}

fn handle_media_fetch_image(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &["mxcUri"])?;
    let mxc_uri = require_non_empty_string(arguments, "mxcUri")?;
    let data = backend_string(backend, "media.fetch", json!({ "mxcUri": mxc_uri }))?;
    if data.is_empty() {
        return Err(ToolFailure::ToolError(
            "Komai returned an empty image response.".to_owned(),
        ));
    }

    success(
        json!({
            "mxcUri": mxc_uri,
            "mimeType": "image/png",
        }),
        vec![
            results::text_content("Fetched an image from the running Komai profile."),
            results::image_content("image/png", data),
        ],
    )
}

fn handle_media_upload_file(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &["path", "filename", "contentType"])?;
    let path = require_non_empty_string(arguments, "path")?;
    let filename = optional_string(arguments, "filename")?;
    let content_type = optional_string(arguments, "contentType")?;

    let mut params = Map::new();
    params.insert("path".to_owned(), Value::String(path.clone()));
    if let Some(filename) = filename {
        params.insert("filename".to_owned(), Value::String(filename));
    }
    if let Some(content_type) = content_type {
        params.insert("contentType".to_owned(), Value::String(content_type));
    }

    let result = backend_object(backend, "media.upload", Value::Object(params))?;
    let summary = result
        .get("mxcUri")
        .and_then(Value::as_str)
        .map(|mxc_uri| format!("Uploaded the file as {mxc_uri}."))
        .unwrap_or_else(|| "Uploaded the file.".to_owned());

    success(Value::Object(result), vec![results::text_content(summary)])
}

const TOOLS: &[ToolDefinition] = &[
    ToolDefinition {
        name: "app_get_version",
        title: "Get Komai Version",
        description: "Get the version string of the running Komai instance.",
        access: ToolAccess::Read,
        destructive: false,
        idempotent: true,
        open_world: false,
        input_schema: || object_schema(vec![], &[]),
        output_schema: || getter_output_schema("version", "Komai version string."),
        handler: handle_app_get_version,
    },
    ToolDefinition {
        name: "app_get_api_version",
        title: "Get Automation API Version",
        description: "Get the Komai automation API version reported by the running instance.",
        access: ToolAccess::Read,
        destructive: false,
        idempotent: true,
        open_world: false,
        input_schema: || object_schema(vec![], &[]),
        output_schema: || getter_output_schema("apiVersion", "Komai automation API version."),
        handler: handle_app_get_api_version,
    },
    ToolDefinition {
        name: "rooms_list",
        title: "List Rooms",
        description: "List joined rooms from the target Komai profile.",
        access: ToolAccess::Read,
        destructive: false,
        idempotent: true,
        open_world: false,
        input_schema: || object_schema(vec![], &[]),
        output_schema: || {
            object_schema(
                vec![(
                    "rooms",
                    json!({
                        "type": "array",
                        "items": room_info_schema(),
                    }),
                )],
                &["rooms"],
            )
        },
        handler: handle_rooms_list,
    },
    ToolDefinition {
        name: "rooms_get_timeline",
        title: "Get Room Timeline",
        description: "Read visible timeline events from a room, newest first.",
        access: ToolAccess::Read,
        destructive: false,
        idempotent: true,
        open_world: false,
        input_schema: || {
            object_schema(
                vec![
                    ("roomIdOrAlias", string_schema("Room ID or room alias.")),
                    (
                        "limit",
                        json!({
                            "type": "integer",
                            "description": "Maximum number of events to return.",
                            "minimum": 1,
                            "maximum": MAX_TIMELINE_LIMIT,
                            "default": DEFAULT_TIMELINE_LIMIT,
                        }),
                    ),
                    (
                        "beforeEventId",
                        string_schema("Exclusive pagination anchor. Returns events older than this event ID."),
                    ),
                    (
                        "includeUnsignedFields",
                        boolean_schema("Whether to include Matrix unsigned event fields."),
                    ),
                    (
                        "fetchMode",
                        enum_string_schema(
                            "Whether to read only from the local cache or fetch older history from the server when needed.",
                            &[
                                TIMELINE_FETCH_MODE_CACHED_ONLY,
                                TIMELINE_FETCH_MODE_SERVER_IF_NEEDED,
                            ],
                        ),
                    ),
                ],
                &["roomIdOrAlias"],
            )
        },
        output_schema: || {
            object_schema(
                vec![
                    ("roomId", string_schema("Resolved Matrix room ID.")),
                    (
                        "events",
                        json!({
                            "type": "array",
                            "items": timeline_event_schema(),
                        }),
                    ),
                    (
                        "hasMore",
                        boolean_schema("Whether more older events are available with the selected fetch mode."),
                    ),
                    (
                        "nextBeforeEventId",
                        nullable_string_schema("Use this value as beforeEventId to retrieve the next older page."),
                    ),
                ],
                &["roomId", "events", "hasMore", "nextBeforeEventId"],
            )
        },
        handler: handle_rooms_get_timeline,
    },
    ToolDefinition {
        name: "rooms_join",
        title: "Join Room",
        description: "Join a room by room ID or alias using the running Komai profile.",
        access: ToolAccess::Write,
        destructive: false,
        idempotent: false,
        open_world: true,
        input_schema: || {
            object_schema(
                vec![("roomIdOrAlias", string_schema("Room ID or room alias."))],
                &["roomIdOrAlias"],
            )
        },
        output_schema: || ok_with_key_output_schema("roomIdOrAlias", "Room ID or alias."),
        handler: handle_rooms_join,
    },
    ToolDefinition {
        name: "rooms_new_direct_chat",
        title: "Start Direct Chat",
        description: "Start or open a direct chat with a Matrix user.",
        access: ToolAccess::Write,
        destructive: false,
        idempotent: false,
        open_world: true,
        input_schema: || {
            object_schema(vec![("userId", string_schema("Matrix user ID."))], &["userId"])
        },
        output_schema: || ok_with_key_output_schema("userId", "Matrix user ID."),
        handler: handle_rooms_new_direct_chat,
    },
    ToolDefinition {
        name: "rooms_invite",
        title: "Invite User To Room",
        description: "Invite a Matrix user to a room. The user has to accept before they join.",
        access: ToolAccess::Write,
        destructive: false,
        idempotent: true,
        open_world: true,
        input_schema: || membership_input_schema("Optional reason shown to the invitee."),
        output_schema: membership_output_schema,
        handler: handle_rooms_invite,
    },
    ToolDefinition {
        name: "rooms_kick",
        title: "Remove User From Room",
        description: "Remove a Matrix user from a room. They can rejoin unless they are also banned.",
        access: ToolAccess::Write,
        destructive: true,
        idempotent: false,
        open_world: true,
        input_schema: || membership_input_schema("Optional reason recorded on the kick."),
        output_schema: membership_output_schema,
        handler: handle_rooms_kick,
    },
    ToolDefinition {
        name: "rooms_ban",
        title: "Ban User From Room",
        description: "Ban a Matrix user from a room, removing them if they are currently joined.",
        access: ToolAccess::Write,
        destructive: true,
        idempotent: true,
        open_world: true,
        input_schema: || membership_input_schema("Optional reason recorded on the ban."),
        output_schema: membership_output_schema,
        handler: handle_rooms_ban,
    },
    ToolDefinition {
        name: "rooms_unban",
        title: "Unban User From Room",
        description: "Lift a Matrix user's ban from a room. This does not re-invite or rejoin them.",
        access: ToolAccess::Write,
        destructive: false,
        idempotent: true,
        open_world: true,
        input_schema: || membership_input_schema("Optional reason recorded on the unban."),
        output_schema: membership_output_schema,
        handler: handle_rooms_unban,
    },
    ToolDefinition {
        name: "rooms_leave",
        title: "Leave Room",
        description: "Leave a room with the active account, or reject a pending invite to it. The room is not forgotten.",
        access: ToolAccess::Write,
        destructive: true,
        idempotent: true,
        open_world: true,
        input_schema: || {
            object_schema(
                vec![
                    ("roomIdOrAlias", string_schema("Room ID or room alias.")),
                    ("reason", string_schema("Optional reason recorded on the leave.")),
                ],
                &["roomIdOrAlias"],
            )
        },
        output_schema: || ok_with_key_output_schema("roomIdOrAlias", "Room ID or alias that was left."),
        handler: handle_rooms_leave,
    },
    ToolDefinition {
        name: "rooms_send",
        title: "Send Room Message",
        description: "Send a text or notice message to a room through the running Komai profile.",
        access: ToolAccess::Write,
        destructive: false,
        idempotent: false,
        open_world: true,
        input_schema: || {
            object_schema(
                vec![
                    ("roomIdOrAlias", string_schema("Room ID or room alias.")),
                    ("body", string_schema("Message body to send.")),
                    (
                        "msgtype",
                        json!({
                            "type": "string",
                            "description": "Message type. Preferred values: text or notice. Matrix-style m.text and m.notice are also accepted for compatibility.",
                            "enum": ["text", "notice"],
                        }),
                    ),
                    (
                        "format",
                        enum_string_schema("Message formatting mode.", &["auto", "plain", "html"]),
                    ),
                ],
                &["roomIdOrAlias", "body"],
            )
        },
        output_schema: || getter_output_schema("eventId", "Event ID of the sent message."),
        handler: handle_rooms_send,
    },
    ToolDefinition {
        name: "rooms_send_image_file",
        title: "Send Image File",
        description: "Upload an image from disk and send it to a room.",
        access: ToolAccess::Write,
        destructive: false,
        idempotent: false,
        open_world: true,
        input_schema: || {
            object_schema(
                vec![
                    ("roomIdOrAlias", string_schema("Room ID or room alias.")),
                    ("path", string_schema("Absolute or relative path to the local image file.")),
                    ("body", string_schema("Optional image caption.")),
                ],
                &["roomIdOrAlias", "path"],
            )
        },
        output_schema: || getter_output_schema("eventId", "Event ID of the sent image message."),
        handler: handle_rooms_send_image_file,
    },
    ToolDefinition {
        name: "rooms_send_image",
        title: "Send Uploaded Image",
        description: "Send an already-uploaded MXC image into a room.",
        access: ToolAccess::Write,
        destructive: false,
        idempotent: false,
        open_world: true,
        input_schema: || {
            object_schema(
                vec![
                    ("roomIdOrAlias", string_schema("Room ID or room alias.")),
                    ("mxcUri", string_schema("MXC URI of the uploaded image.")),
                    ("body", string_schema("Optional image caption.")),
                    ("filename", string_schema("Original filename to present in the event.")),
                    ("info", generic_object_schema("Optional Matrix image info object.")),
                ],
                &["roomIdOrAlias", "mxcUri"],
            )
        },
        output_schema: || getter_output_schema("eventId", "Event ID of the sent image message."),
        handler: handle_rooms_send_image,
    },
    ToolDefinition {
        name: "user_get_id",
        title: "Get User ID",
        description: "Get the Matrix user ID of the active account in the target Komai profile.",
        access: ToolAccess::Read,
        destructive: false,
        idempotent: true,
        open_world: false,
        input_schema: || object_schema(vec![], &[]),
        output_schema: || getter_output_schema("userId", "Matrix user ID."),
        handler: handle_user_get_id,
    },
    ToolDefinition {
        name: "user_get_homeserver_url",
        title: "Get Homeserver URL",
        description: "Get the homeserver base URL of the active account.",
        access: ToolAccess::Read,
        destructive: false,
        idempotent: true,
        open_world: false,
        input_schema: || object_schema(vec![], &[]),
        output_schema: || getter_output_schema("homeserverUrl", "Homeserver base URL."),
        handler: handle_user_get_homeserver_url,
    },
    ToolDefinition {
        name: "user_get_device_id",
        title: "Get Device ID",
        description: "Get the current Matrix device ID for the active Komai session.",
        access: ToolAccess::Read,
        destructive: false,
        idempotent: true,
        open_world: false,
        input_schema: || object_schema(vec![], &[]),
        output_schema: || getter_output_schema("deviceId", "Current Matrix device ID."),
        handler: handle_user_get_device_id,
    },
    ToolDefinition {
        name: "user_get_status_message",
        title: "Get Status Message",
        description: "Get the current user status message from the running Komai profile.",
        access: ToolAccess::Read,
        destructive: false,
        idempotent: true,
        open_world: false,
        input_schema: || object_schema(vec![], &[]),
        output_schema: || getter_output_schema("statusMessage", "Current status message."),
        handler: handle_user_get_status_message,
    },
    ToolDefinition {
        name: "user_set_status_message",
        title: "Set Status Message",
        description: "Set the current user status message through the running Komai profile.",
        access: ToolAccess::Write,
        destructive: false,
        idempotent: true,
        open_world: true,
        input_schema: || {
            object_schema(vec![("message", string_schema("New status message."))], &["message"])
        },
        output_schema: || ok_with_key_output_schema("statusMessage", "Updated status message."),
        handler: handle_user_set_status_message,
    },
    ToolDefinition {
        name: "settings_ui_get_theme",
        title: "Get Active Theme",
        description: "Get the active theme slug from the target Komai profile.",
        access: ToolAccess::Read,
        destructive: false,
        idempotent: true,
        open_world: false,
        input_schema: || object_schema(vec![], &[]),
        output_schema: || getter_output_schema("theme", "Active theme slug."),
        handler: handle_settings_ui_get_theme,
    },
    ToolDefinition {
        name: "settings_ui_set_theme",
        title: "Set Active Theme",
        description: "Change the active theme in the target Komai profile.",
        access: ToolAccess::Write,
        destructive: false,
        idempotent: true,
        open_world: false,
        input_schema: || {
            object_schema(vec![("theme", string_schema("Theme slug to activate."))], &["theme"])
        },
        output_schema: || ok_with_key_output_schema("theme", "Updated theme slug."),
        handler: handle_settings_ui_set_theme,
    },
    ToolDefinition {
        name: "media_fetch_image",
        title: "Fetch Image",
        description: "Fetch an authenticated PNG image from an MXC URI through the running Komai profile.",
        access: ToolAccess::Read,
        destructive: false,
        idempotent: true,
        open_world: false,
        input_schema: || {
            object_schema(vec![("mxcUri", string_schema("Matrix content URI."))], &["mxcUri"])
        },
        output_schema: || {
            object_schema(
                vec![
                    ("mxcUri", string_schema("Matrix content URI.")),
                    ("mimeType", string_schema("Returned MIME type.")),
                ],
                &["mxcUri", "mimeType"],
            )
        },
        handler: handle_media_fetch_image,
    },
    ToolDefinition {
        name: "media_upload_file",
        title: "Upload File",
        description: "Upload a local file through the running Komai profile and return its MXC URI.",
        access: ToolAccess::Write,
        destructive: false,
        idempotent: false,
        open_world: true,
        input_schema: || {
            object_schema(
                vec![
                    ("path", string_schema("Local path to the file to upload.")),
                    ("filename", string_schema("Optional filename override.")),
                    ("contentType", string_schema("Optional MIME type override.")),
                ],
                &["path"],
            )
        },
        output_schema: || {
            object_schema(
                vec![
                    ("mxcUri", string_schema("Uploaded Matrix content URI.")),
                    ("contentType", string_schema("Detected or requested MIME type.")),
                    ("filename", string_schema("Filename used for the upload.")),
                    ("size", integer_schema("Uploaded file size in bytes.")),
                ],
                &["mxcUri", "contentType", "filename", "size"],
            )
        },
        handler: handle_media_upload_file,
    },
];

#[cfg(test)]
mod tests {
    use std::cell::RefCell;
    use std::collections::HashMap;

    use super::{call_tool, list_tools, AccessMode, Backend, CallToolError};
    use serde_json::{json, Value};

    #[derive(Default)]
    struct MockBackend {
        calls: RefCell<Vec<(String, Value)>>,
        responses: HashMap<String, Result<Value, String>>,
    }

    impl MockBackend {
        fn with_response(method: &str, response: Result<Value, String>) -> Self {
            let mut responses = HashMap::new();
            responses.insert(method.to_owned(), response);
            Self {
                calls: RefCell::new(Vec::new()),
                responses,
            }
        }
    }

    impl Backend for MockBackend {
        fn call(&self, method: &str, params: Value) -> Result<Value, String> {
            self.calls
                .borrow_mut()
                .push((method.to_owned(), params.clone()));

            self.responses
                .get(method)
                .cloned()
                .unwrap_or_else(|| Ok(Value::Null))
        }
    }

    #[test]
    fn read_only_mode_hides_write_tools() {
        let names: Vec<String> = list_tools(AccessMode::ReadOnly)
            .into_iter()
            .map(|tool| tool["name"].as_str().unwrap().to_owned())
            .collect();

        assert!(names.contains(&"rooms_list".to_owned()));
        assert!(names.contains(&"rooms_get_timeline".to_owned()));
        assert!(!names.contains(&"rooms_send".to_owned()));
    }

    #[test]
    fn read_write_mode_includes_write_tools() {
        let names: Vec<String> = list_tools(AccessMode::ReadWrite)
            .into_iter()
            .map(|tool| tool["name"].as_str().unwrap().to_owned())
            .collect();

        assert!(names.contains(&"rooms_send".to_owned()));
        assert!(names.contains(&"settings_ui_set_theme".to_owned()));
    }

    #[test]
    fn rooms_list_wraps_rooms_in_structured_content() {
        let backend = MockBackend::with_response(
            "rooms.list",
            Ok(json!([
                {
                    "id": "!room:example.org",
                    "alias": "#example:example.org",
                    "name": "Example",
                    "avatarUrl": "mxc://example.org/avatar",
                    "read": false,
                    "unreadCount": 3,
                    "memberCount": 2,
                    "mostRecentEventTimestampMs": 1742810400000_i64,
                    "highlighted": false,
                    "categories": ["direct", "person", "encrypted"],
                    "tags": ["m.favourite"],
                    "parentSpaces": ["!space:example.org"],
                    "dmUserId": "@alice:example.org",
                    "encrypted": true
                }
            ])),
        );

        let result = call_tool(&backend, AccessMode::ReadOnly, "rooms_list", None).unwrap();

        assert!(result["structuredContent"]["rooms"].is_array());
        assert_eq!(
            result["structuredContent"]["rooms"][0]["id"].as_str(),
            Some("!room:example.org")
        );
        assert_eq!(result["content"][0]["type"].as_str(), Some("text"));
    }

    #[test]
    fn rooms_get_timeline_wraps_events_in_structured_content() {
        let backend = MockBackend::with_response(
            "rooms.timeline",
            Ok(json!({
                "roomId": "!room:example.org",
                "events": [
                    {
                        "content": {"body": "Hello", "msgtype": "m.text"},
                        "event_id": "$event:example.org",
                        "origin_server_ts": 1742810400000_i64,
                        "sender": "@alice:example.org",
                        "type": "m.room.message"
                    }
                ],
                "hasMore": true,
                "nextBeforeEventId": "$event:example.org"
            })),
        );

        let result = call_tool(
            &backend,
            AccessMode::ReadOnly,
            "rooms_get_timeline",
            Some(json!({ "roomIdOrAlias": "!room:example.org" })),
        )
        .unwrap();

        assert_eq!(
            result["structuredContent"]["roomId"].as_str(),
            Some("!room:example.org")
        );
        assert!(result["structuredContent"]["events"].is_array());
        assert_eq!(
            result["structuredContent"]["events"][0]["event_id"].as_str(),
            Some("$event:example.org")
        );
    }

    #[test]
    fn media_fetch_image_returns_image_content() {
        let backend = MockBackend::with_response(
            "media.fetch",
            Ok(Value::String("iVBORw0KGgoAAAANSUhEUgAAAAEAAAAB".to_owned())),
        );

        let result = call_tool(
            &backend,
            AccessMode::ReadOnly,
            "media_fetch_image",
            Some(json!({ "mxcUri": "mxc://example.org/image" })),
        )
        .unwrap();

        assert_eq!(
            result["structuredContent"]["mimeType"].as_str(),
            Some("image/png")
        );
        assert_eq!(result["content"][1]["type"].as_str(), Some("image"));
        assert_eq!(
            result["content"][1]["mimeType"].as_str(),
            Some("image/png")
        );
    }

    #[test]
    fn backend_errors_become_tool_errors() {
        let backend = MockBackend::with_response(
            "user.userId",
            Err("no running Komai instance for profile 'default'".to_owned()),
        );

        let result = call_tool(&backend, AccessMode::ReadOnly, "user_get_id", None).unwrap();

        assert_eq!(result["isError"].as_bool(), Some(true));
        assert!(result["content"][0]["text"]
            .as_str()
            .unwrap()
            .contains("no running Komai instance"));
    }

    #[test]
    fn invalid_arguments_are_reported_as_protocol_errors() {
        let backend = MockBackend::default();

        let error = call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_send",
            Some(json!({
                "roomIdOrAlias": "!room:example.org",
                "body": "Hello",
                "msgtype": "invalid"
            })),
        )
        .unwrap_err();

        assert_eq!(
            error,
            CallToolError::InvalidParams(
                "Argument 'msgtype' must be one of: text, notice, m.text, m.notice."
                    .to_owned()
            )
        );
    }

    #[test]
    fn invalid_timeline_limit_is_reported_as_protocol_error() {
        let backend = MockBackend::default();

        let error = call_tool(
            &backend,
            AccessMode::ReadOnly,
            "rooms_get_timeline",
            Some(json!({
                "roomIdOrAlias": "!room:example.org",
                "limit": 0
            })),
        )
        .unwrap_err();

        assert_eq!(
            error,
            CallToolError::InvalidParams(
                "Argument 'limit' must be between 1 and 500.".to_owned()
            )
        );
    }

    #[test]
    fn rooms_get_timeline_maps_arguments_to_existing_ipc_protocol() {
        let backend = MockBackend::with_response(
            "rooms.timeline",
            Ok(json!({
                "roomId": "!room:example.org",
                "events": [],
                "hasMore": false,
                "nextBeforeEventId": null
            })),
        );

        call_tool(
            &backend,
            AccessMode::ReadOnly,
            "rooms_get_timeline",
            Some(json!({
                "roomIdOrAlias": "#example:example.org",
                "limit": 75,
                "beforeEventId": "$older:example.org",
                "includeUnsignedFields": true,
                "fetchMode": "server_fetch_if_needed"
            })),
        )
        .unwrap();

        let calls = backend.calls.borrow();
        let (method, params) = &calls[0];
        assert_eq!(method, "rooms.timeline");
        assert_eq!(params["roomIdOrAlias"].as_str(), Some("#example:example.org"));
        assert_eq!(params["limit"].as_i64(), Some(75));
        assert_eq!(params["beforeEventId"].as_str(), Some("$older:example.org"));
        assert_eq!(params["includeUnsignedFields"].as_bool(), Some(true));
        assert_eq!(
            params["fetchMode"].as_str(),
            Some("server_fetch_if_needed")
        );
    }

    #[test]
    fn empty_join_target_is_reported_as_protocol_error() {
        let backend = MockBackend::default();

        let error = call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_join",
            Some(json!({
                "roomIdOrAlias": ""
            })),
        )
        .unwrap_err();

        assert_eq!(
            error,
            CallToolError::InvalidParams(
                "Argument 'roomIdOrAlias' must not be empty.".to_owned()
            )
        );
    }

    #[test]
    fn empty_message_body_is_reported_as_protocol_error() {
        let backend = MockBackend::default();

        let error = call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_send",
            Some(json!({
                "roomIdOrAlias": "!room:example.org",
                "body": "   "
            })),
        )
        .unwrap_err();

        assert_eq!(
            error,
            CallToolError::InvalidParams("Argument 'body' must not be empty.".to_owned())
        );
    }

    #[test]
    fn empty_theme_slug_is_reported_as_protocol_error() {
        let backend = MockBackend::default();

        let error = call_tool(
            &backend,
            AccessMode::ReadWrite,
            "settings_ui_set_theme",
            Some(json!({
                "theme": "   "
            })),
        )
        .unwrap_err();

        assert_eq!(
            error,
            CallToolError::InvalidParams("Argument 'theme' must not be empty.".to_owned())
        );
    }

    #[test]
    fn empty_media_fetch_uri_is_reported_as_protocol_error() {
        let backend = MockBackend::default();

        let error = call_tool(
            &backend,
            AccessMode::ReadOnly,
            "media_fetch_image",
            Some(json!({
                "mxcUri": ""
            })),
        )
        .unwrap_err();

        assert_eq!(
            error,
            CallToolError::InvalidParams("Argument 'mxcUri' must not be empty.".to_owned())
        );
    }

    #[test]
    fn membership_tools_are_write_gated() {
        let read_only: Vec<String> = list_tools(AccessMode::ReadOnly)
            .into_iter()
            .map(|tool| tool["name"].as_str().unwrap().to_owned())
            .collect();
        let read_write: Vec<String> = list_tools(AccessMode::ReadWrite)
            .into_iter()
            .map(|tool| tool["name"].as_str().unwrap().to_owned())
            .collect();

        for name in [
            "rooms_invite",
            "rooms_kick",
            "rooms_ban",
            "rooms_unban",
            "rooms_leave",
        ] {
            assert!(!read_only.contains(&name.to_owned()), "{name} in read_only");
            assert!(read_write.contains(&name.to_owned()), "{name} missing");
        }
    }

    #[test]
    fn destructive_membership_tools_are_annotated_as_such() {
        let tools = list_tools(AccessMode::ReadWrite);
        let destructive = |name: &str| -> bool {
            tools
                .iter()
                .find(|tool| tool["name"].as_str() == Some(name))
                .expect("tool present")["annotations"]["destructiveHint"]
                .as_bool()
                .expect("destructiveHint present")
        };

        assert!(destructive("rooms_kick"));
        assert!(destructive("rooms_ban"));
        assert!(destructive("rooms_leave"));
        assert!(!destructive("rooms_invite"));
        assert!(!destructive("rooms_unban"));
    }

    #[test]
    fn rooms_ban_maps_arguments_to_the_ipc_protocol() {
        let backend = MockBackend::with_response("rooms.ban", Ok(Value::Bool(true)));

        let result = call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_ban",
            Some(json!({
                "roomIdOrAlias": "#example:example.org",
                "userId": "@spammer:example.org",
                "reason": "spam"
            })),
        )
        .unwrap();

        let calls = backend.calls.borrow();
        let (method, params) = &calls[0];
        assert_eq!(method, "rooms.ban");
        assert_eq!(params["roomIdOrAlias"].as_str(), Some("#example:example.org"));
        assert_eq!(params["userId"].as_str(), Some("@spammer:example.org"));
        assert_eq!(params["reason"].as_str(), Some("spam"));
        assert_eq!(
            result["structuredContent"]["userId"].as_str(),
            Some("@spammer:example.org")
        );
    }

    #[test]
    fn membership_reason_is_omitted_when_not_given() {
        let backend = MockBackend::with_response("rooms.invite", Ok(Value::Bool(true)));

        call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_invite",
            Some(json!({
                "roomIdOrAlias": "!room:example.org",
                "userId": "@alice:example.org"
            })),
        )
        .unwrap();

        let calls = backend.calls.borrow();
        let (_, params) = &calls[0];
        assert!(params.get("reason").is_none());
    }

    #[test]
    fn membership_tools_require_a_user_id() {
        let backend = MockBackend::default();

        let error = call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_kick",
            Some(json!({ "roomIdOrAlias": "!room:example.org" })),
        )
        .unwrap_err();

        assert_eq!(
            error,
            CallToolError::InvalidParams("Missing required argument 'userId'.".to_owned())
        );
    }

    #[test]
    fn rooms_leave_does_not_accept_a_user_id() {
        let backend = MockBackend::default();

        let error = call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_leave",
            Some(json!({
                "roomIdOrAlias": "!room:example.org",
                "userId": "@alice:example.org"
            })),
        )
        .unwrap_err();

        assert_eq!(
            error,
            CallToolError::InvalidParams("Unexpected arguments: userId.".to_owned())
        );
    }

    #[test]
    fn rooms_send_maps_arguments_to_existing_ipc_protocol() {
        let backend = MockBackend::with_response(
            "rooms.send",
            Ok(json!({ "eventId": "$event:example.org" })),
        );

        let result = call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_send",
            Some(json!({
                "roomIdOrAlias": "#example:example.org",
                "body": "Hello",
                "msgtype": "notice",
                "format": "plain"
            })),
        )
        .unwrap();

        let calls = backend.calls.borrow();
        let (method, params) = &calls[0];
        assert_eq!(method, "rooms.send");
        assert_eq!(params["roomIdOrAlias"].as_str(), Some("#example:example.org"));
        assert_eq!(params["body"].as_str(), Some("Hello"));
        assert_eq!(params["msgtype"].as_str(), Some("m.notice"));
        assert_eq!(params["format"].as_str(), Some("plain"));
        assert_eq!(result["structuredContent"]["eventId"].as_str(), Some("$event:example.org"));
    }

    #[test]
    fn rooms_send_accepts_matrix_style_msgtype_values() {
        let backend = MockBackend::with_response(
            "rooms.send",
            Ok(json!({ "eventId": "$event:example.org" })),
        );

        call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_send",
            Some(json!({
                "roomIdOrAlias": "#example:example.org",
                "body": "Hello",
                "msgtype": "m.text"
            })),
        )
        .unwrap();

        let calls = backend.calls.borrow();
        let (method, params) = &calls[0];
        assert_eq!(method, "rooms.send");
        assert_eq!(params["msgtype"].as_str(), Some("m.text"));
    }
}
