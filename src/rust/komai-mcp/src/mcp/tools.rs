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

fn optional_string_array(
    arguments: &Map<String, Value>,
    key: &str,
) -> Result<Option<Vec<String>>, ToolFailure> {
    match arguments.get(key) {
        Some(Value::Array(values)) => values
            .iter()
            .map(|value| match value {
                Value::String(value) => Ok(value.clone()),
                _ => Err(ToolFailure::InvalidParams(format!(
                    "Argument '{key}' must contain only strings."
                ))),
            })
            .collect::<Result<Vec<String>, ToolFailure>>()
            .map(Some),
        Some(Value::Null) => Ok(None),
        Some(_) => Err(ToolFailure::InvalidParams(format!(
            "Argument '{key}' must be an array when provided."
        ))),
        None => Ok(None),
    }
}

fn optional_array(arguments: &Map<String, Value>, key: &str) -> Result<Option<Vec<Value>>, ToolFailure> {
    match arguments.get(key) {
        Some(Value::Array(values)) => Ok(Some(values.clone())),
        Some(Value::Null) => Ok(None),
        Some(_) => Err(ToolFailure::InvalidParams(format!(
            "Argument '{key}' must be an array when provided."
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

const ROOM_FIELDS: &[&str] = &[
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
];
const DEFAULT_ROOM_LIST_LIMIT: i64 = 50;
const MAX_ROOM_LIST_LIMIT: i64 = 1000;

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
        // The IPC layer defaults to cached_only, which suits the app. A tool
        // caller usually wants the room it just asked about, including one the
        // running profile has not opened this session, so the default flips
        // here rather than there.
        None => Ok(TIMELINE_FETCH_MODE_SERVER_IF_NEEDED.to_owned()),
        Some(TIMELINE_FETCH_MODE_CACHED_ONLY) => {
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

const CREATE_ROOM_PRESETS: &[&str] = &["private_chat", "public_chat", "trusted_private_chat"];

fn parse_create_room_preset(arguments: &Map<String, Value>) -> Result<String, ToolFailure> {
    match optional_string(arguments, "preset")?.as_deref() {
        None => Ok("private_chat".to_owned()),
        Some(preset) if CREATE_ROOM_PRESETS.contains(&preset) => Ok(preset.to_owned()),
        Some(_) => Err(ToolFailure::InvalidParams(format!(
            "Argument 'preset' must be one of: {}.",
            CREATE_ROOM_PRESETS.join(", ")
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

fn parse_room_list_fields(arguments: &Map<String, Value>) -> Result<Option<Vec<String>>, ToolFailure> {
    let Some(fields) = optional_string_array(arguments, "fields")? else {
        return Ok(None);
    };

    for field in &fields {
        if !ROOM_FIELDS.contains(&field.as_str()) {
            return Err(ToolFailure::InvalidParams(format!(
                "Argument 'fields' has an unknown key '{field}'. Known keys: {}.",
                ROOM_FIELDS.join(", ")
            )));
        }
    }

    Ok(Some(fields))
}

fn handle_rooms_list(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(
        arguments,
        &[
            "ids",
            "query",
            "isDm",
            "encrypted",
            "tag",
            "parentSpace",
            "minMemberCount",
            "limit",
            "offset",
            "fields",
        ],
    )?;

    let mut params = Map::new();

    for key in ["query", "tag", "parentSpace"] {
        if let Some(value) = optional_string(arguments, key)? {
            params.insert(key.to_owned(), Value::String(value));
        }
    }

    for key in ["isDm", "encrypted"] {
        if let Some(value) = optional_bool(arguments, key)? {
            params.insert(key.to_owned(), Value::Bool(value));
        }
    }

    if let Some(ids) = optional_string_array(arguments, "ids")? {
        params.insert(
            "ids".to_owned(),
            Value::Array(ids.into_iter().map(Value::String).collect()),
        );
    }

    if let Some(fields) = parse_room_list_fields(arguments)? {
        params.insert(
            "fields".to_owned(),
            Value::Array(fields.into_iter().map(Value::String).collect()),
        );
    }

    for key in ["minMemberCount", "offset"] {
        if let Some(value) = optional_integer(arguments, key)? {
            if value < 0 {
                return Err(ToolFailure::InvalidParams(format!(
                    "Argument '{key}' must not be negative."
                )));
            }
            params.insert(key.to_owned(), Value::Number(value.into()));
        }
    }

    // Unlike the IPC layer, the tool defaults to a page. An unbounded list is
    // large enough on a real account to blow an MCP host's tool-result cap,
    // and matchCount tells the caller what it is missing.
    let limit = match optional_integer(arguments, "limit")? {
        None => DEFAULT_ROOM_LIST_LIMIT,
        Some(limit) if (1..=MAX_ROOM_LIST_LIMIT).contains(&limit) => limit,
        Some(_) => {
            return Err(ToolFailure::InvalidParams(format!(
                "Argument 'limit' must be between 1 and {MAX_ROOM_LIST_LIMIT}."
            )))
        }
    };
    params.insert("limit".to_owned(), Value::Number(limit.into()));

    let result = backend_object(backend, "rooms.list", Value::Object(params))?;
    let returned = result
        .get("rooms")
        .and_then(Value::as_array)
        .map(Vec::len)
        .unwrap_or(0);
    let total = result
        .get("matchCount")
        .and_then(Value::as_i64)
        .unwrap_or(returned as i64);

    let summary = if (returned as i64) < total {
        format!("Listed {returned} of {total} matching rooms. Use offset to page through the rest.")
    } else {
        format!("Listed {returned} rooms.")
    };

    success(Value::Object(result), vec![results::text_content(summary)])
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

fn handle_rooms_create(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(
        arguments,
        &[
            "name",
            "topic",
            "aliasLocalpart",
            "preset",
            "invite",
            "isDirect",
            "isEncrypted",
            "isSpace",
            "isPublic",
            "roomVersion",
            "powerLevelContentOverride",
            "initialState",
            "creationContent",
        ],
    )?;

    let mut params = Map::new();
    params.insert(
        "preset".to_owned(),
        Value::String(parse_create_room_preset(arguments)?),
    );

    for key in ["name", "topic", "aliasLocalpart", "roomVersion"] {
        if let Some(value) = optional_string(arguments, key)? {
            params.insert(key.to_owned(), Value::String(value));
        }
    }

    for key in ["isDirect", "isEncrypted", "isSpace", "isPublic"] {
        if let Some(value) = optional_bool(arguments, key)? {
            params.insert(key.to_owned(), Value::Bool(value));
        }
    }

    if let Some(invite) = optional_string_array(arguments, "invite")? {
        params.insert(
            "invite".to_owned(),
            Value::Array(invite.into_iter().map(Value::String).collect()),
        );
    }

    for key in ["powerLevelContentOverride", "creationContent"] {
        if let Some(value) = optional_object(arguments, key)? {
            params.insert(key.to_owned(), Value::Object(value));
        }
    }

    if let Some(initial_state) = optional_array(arguments, "initialState")? {
        params.insert("initialState".to_owned(), Value::Array(initial_state));
    }

    let result = backend_object(backend, "rooms.create", Value::Object(params))?;
    let summary = result
        .get("roomId")
        .and_then(Value::as_str)
        .map(|room_id| format!("Created {room_id}."))
        .unwrap_or_else(|| "Created the room.".to_owned());

    success(Value::Object(result), vec![results::text_content(summary)])
}

/// Both state tools take the same three locating arguments.
fn state_params(
    arguments: &Map<String, Value>,
) -> Result<(String, String, String), ToolFailure> {
    let room_id_or_alias = require_non_empty_string(arguments, "roomIdOrAlias")?;
    let event_type = require_non_empty_string(arguments, "eventType")?;
    // Most state events key off the empty string, so an absent stateKey means
    // "" rather than "any state key".
    let state_key = optional_string(arguments, "stateKey")?.unwrap_or_default();
    Ok((room_id_or_alias, event_type, state_key))
}

fn handle_rooms_get_state(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &["roomIdOrAlias", "eventType", "stateKey"])?;
    let (room_id_or_alias, event_type, state_key) = state_params(arguments)?;

    let result = backend_object(
        backend,
        "rooms.getState",
        json!({
            "roomIdOrAlias": room_id_or_alias,
            "eventType": event_type,
            "stateKey": state_key,
        }),
    )?;

    let exists = result
        .get("exists")
        .and_then(Value::as_bool)
        .unwrap_or(false);
    let summary = if exists {
        format!("Read {event_type} from {room_id_or_alias}.")
    } else {
        format!("{room_id_or_alias} has no {event_type} state event.")
    };

    success(Value::Object(result), vec![results::text_content(summary)])
}

fn handle_rooms_set_state(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &["roomIdOrAlias", "eventType", "stateKey", "content"])?;
    let (room_id_or_alias, event_type, state_key) = state_params(arguments)?;
    let content = match arguments.get("content") {
        Some(Value::Object(content)) => content.clone(),
        Some(_) => {
            return Err(ToolFailure::InvalidParams(
                "Argument 'content' must be an object.".to_owned(),
            ))
        }
        None => {
            return Err(ToolFailure::InvalidParams(
                "Missing required argument 'content'.".to_owned(),
            ))
        }
    };

    let result = backend_object(
        backend,
        "rooms.setState",
        json!({
            "roomIdOrAlias": room_id_or_alias,
            "eventType": event_type,
            "stateKey": state_key,
            "content": content,
        }),
    )?;

    success(
        Value::Object(result),
        vec![results::text_content(format!(
            "Set {event_type} in {room_id_or_alias}."
        ))],
    )
}

/// rooms_set_name and rooms_set_topic differ only in the method and the key
/// carrying the value.
fn handle_room_text_setting(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
    method: &str,
    key: &'static str,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &["roomIdOrAlias", key])?;
    let room_id_or_alias = require_non_empty_string(arguments, "roomIdOrAlias")?;
    // An empty value is meaningful: it clears the field.
    let value = optional_string(arguments, key)?.unwrap_or_default();

    backend_bool_true(
        backend,
        method,
        json!({ "roomIdOrAlias": room_id_or_alias, key: value }),
    )?;

    let summary = if value.is_empty() {
        format!("Cleared the {key} of {room_id_or_alias}.")
    } else {
        format!("Set the {key} of {room_id_or_alias} to \"{value}\".")
    };

    success(
        json!({ "ok": true, "roomIdOrAlias": room_id_or_alias, key: value }),
        vec![results::text_content(summary)],
    )
}

fn handle_rooms_set_name(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    handle_room_text_setting(backend, arguments, "rooms.setName", "name")
}

fn handle_rooms_set_topic(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    handle_room_text_setting(backend, arguments, "rooms.setTopic", "topic")
}

fn handle_rooms_set_power_level(
    backend: &dyn Backend,
    arguments: &Map<String, Value>,
) -> Result<ToolSuccess, ToolFailure> {
    reject_unknown_keys(arguments, &["roomIdOrAlias", "userId", "powerLevel"])?;
    let room_id_or_alias = require_non_empty_string(arguments, "roomIdOrAlias")?;
    let user_id = require_non_empty_string(arguments, "userId")?;
    let power_level = optional_integer(arguments, "powerLevel")?.ok_or_else(|| {
        ToolFailure::InvalidParams("Missing required argument 'powerLevel'.".to_owned())
    })?;

    backend_bool_true(
        backend,
        "rooms.setPowerLevel",
        json!({
            "roomIdOrAlias": room_id_or_alias,
            "userId": user_id,
            "powerLevel": power_level,
        }),
    )?;

    success(
        json!({
            "ok": true,
            "roomIdOrAlias": room_id_or_alias,
            "userId": user_id,
            "powerLevel": power_level,
        }),
        vec![results::text_content(format!(
            "Set {user_id} to power level {power_level} in {room_id_or_alias}."
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
        description: "List joined rooms from the target Komai profile. Filters combine with AND, \
and matchCount reports how many rooms matched before paging -- so a caller can tell it is seeing \
a subset. Prefer 'ids' or 'query' plus 'fields' over listing everything.",
        access: ToolAccess::Read,
        destructive: false,
        idempotent: true,
        open_world: false,
        input_schema: || {
            object_schema(
                vec![
                    (
                        "ids",
                        string_array_schema("Room IDs or aliases to restrict the result to. The cheapest way to resolve known rooms to their names."),
                    ),
                    (
                        "query",
                        string_schema("Case-insensitive substring matched against the room name and alias."),
                    ),
                    ("isDm", boolean_schema("Keep only direct chats, or only non-direct chats.")),
                    (
                        "encrypted",
                        boolean_schema("Keep only encrypted rooms, or only unencrypted ones."),
                    ),
                    ("tag", string_schema("Keep only rooms carrying this Matrix room tag.")),
                    (
                        "parentSpace",
                        string_schema("Keep only rooms that are children of this space room ID."),
                    ),
                    (
                        "minMemberCount",
                        json!({
                            "type": "integer",
                            "description": "Keep only rooms with at least this many joined members.",
                            "minimum": 0,
                        }),
                    ),
                    (
                        "limit",
                        json!({
                            "type": "integer",
                            "description": "Maximum rooms to return.",
                            "minimum": 1,
                            "maximum": MAX_ROOM_LIST_LIMIT,
                            "default": DEFAULT_ROOM_LIST_LIMIT,
                        }),
                    ),
                    (
                        "offset",
                        json!({
                            "type": "integer",
                            "description": "Skip this many matching rooms before the returned page. Rooms are ordered by recent activity, so paging a busy account is a snapshot rather than a stable cursor.",
                            "minimum": 0,
                        }),
                    ),
                    (
                        "fields",
                        json!({
                            "type": "array",
                            "description": "Keys to keep on each returned room. Omit for all of them; most callers want just id and name.",
                            "items": {
                                "type": "string",
                                "enum": ROOM_FIELDS,
                            },
                        }),
                    ),
                ],
                &[],
            )
        },
        output_schema: || {
            object_schema(
                vec![
                    (
                        "rooms",
                        json!({
                            "type": "array",
                            "items": room_info_schema(),
                        }),
                    ),
                    (
                        "matchCount",
                        integer_schema("How many rooms matched the filters, counted before limit and offset were applied. Equals the number of joined rooms only when no filters were given."),
                    ),
                ],
                &["rooms", "matchCount"],
            )
        },
        handler: handle_rooms_list,
    },
    ToolDefinition {
        name: "rooms_get_timeline",
        title: "Get Room Timeline",
        description: "Read visible timeline events from a room, newest first. Reaches the \
homeserver for history the running Komai profile does not already hold, unless fetchMode says \
otherwise.",
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
                        json!({
                            "type": "string",
                            "description": "Where events may come from. server_fetch_if_needed (the default) reaches the homeserver for history the running profile does not already hold; cached_only stays local and returns nothing for a room Komai has not opened this session.",
                            "enum": [
                                TIMELINE_FETCH_MODE_CACHED_ONLY,
                                TIMELINE_FETCH_MODE_SERVER_IF_NEEDED,
                            ],
                            "default": TIMELINE_FETCH_MODE_SERVER_IF_NEEDED,
                        }),
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
        name: "rooms_create",
        title: "Create Room",
        description: "Create a Matrix room or space and return its room ID. \
powerLevelContentOverride, initialState and creationContent are passed to the homeserver \
untouched, so any Matrix-defined field works; the homeserver validates them.",
        access: ToolAccess::Write,
        destructive: false,
        idempotent: false,
        open_world: true,
        input_schema: || {
            object_schema(
                vec![
                    ("name", string_schema("Room name.")),
                    ("topic", string_schema("Room topic.")),
                    (
                        "aliasLocalpart",
                        string_schema("Local part of the room alias, without a leading '#' or a server name."),
                    ),
                    (
                        "preset",
                        json!({
                            "type": "string",
                            "description": "Matrix createRoom preset. trusted_private_chat grants invitees the creator's power level.",
                            "enum": CREATE_ROOM_PRESETS,
                            "default": "private_chat",
                        }),
                    ),
                    ("invite", string_array_schema("Matrix user IDs to invite on creation.")),
                    ("isDirect", boolean_schema("Whether to mark the room as a direct chat.")),
                    (
                        "isEncrypted",
                        boolean_schema("Whether to enable end-to-end encryption at creation."),
                    ),
                    ("isSpace", boolean_schema("Whether to create a space instead of a room.")),
                    (
                        "isPublic",
                        boolean_schema("Whether to publish the room in the server's room directory."),
                    ),
                    (
                        "roomVersion",
                        string_schema("Room version to request. Omit to accept the server's default."),
                    ),
                    (
                        "powerLevelContentOverride",
                        generic_object_schema(
                            "m.room.power_levels content to apply at creation. The only way to grant a co-moderator a power level atomically with the room.",
                        ),
                    ),
                    (
                        "initialState",
                        json!({
                            "type": "array",
                            "description": "State events to set at creation, each an object with 'type', optional 'state_key' and 'content'. Use this for state that must be right before anyone joins, such as history visibility.",
                            "items": {
                                "type": "object",
                            },
                        }),
                    ),
                    (
                        "creationContent",
                        generic_object_schema(
                            "Additional m.room.create content, such as {\"m.federate\": false}, which cannot be changed after creation.",
                        ),
                    ),
                ],
                &[],
            )
        },
        output_schema: || getter_output_schema("roomId", "Matrix room ID of the created room."),
        handler: handle_rooms_create,
    },
    ToolDefinition {
        name: "rooms_get_state",
        title: "Get Room State Event",
        description: "Read one room state event's content, fetched from the homeserver. Works for \
any event type, including custom ones. Returns exists:false rather than an error when the room \
has no such state.",
        access: ToolAccess::Read,
        destructive: false,
        idempotent: true,
        open_world: true,
        input_schema: || {
            object_schema(
                vec![
                    ("roomIdOrAlias", string_schema("Room ID or room alias.")),
                    (
                        "eventType",
                        string_schema("Matrix state event type, such as m.room.topic or a custom type."),
                    ),
                    (
                        "stateKey",
                        string_schema("State key. Defaults to the empty string, which is what most state events use."),
                    ),
                ],
                &["roomIdOrAlias", "eventType"],
            )
        },
        output_schema: || {
            object_schema(
                vec![
                    (
                        "exists",
                        boolean_schema("Whether the room has this state event at all."),
                    ),
                    (
                        "content",
                        generic_object_schema("The state event's content, empty when exists is false."),
                    ),
                ],
                &["exists", "content"],
            )
        },
        handler: handle_rooms_get_state,
    },
    ToolDefinition {
        name: "rooms_set_state",
        title: "Set Room State Event",
        description: "Send a room state event with arbitrary content, including custom event \
types. The content REPLACES the state event rather than merging into it, so read it first with \
rooms_get_state and send back a complete object. For m.room.power_levels prefer \
rooms_set_power_level, which merges; a partial power_levels object silently drops every level it \
omits.",
        access: ToolAccess::Write,
        destructive: true,
        idempotent: true,
        open_world: true,
        input_schema: || {
            object_schema(
                vec![
                    ("roomIdOrAlias", string_schema("Room ID or room alias.")),
                    ("eventType", string_schema("Matrix state event type to send.")),
                    (
                        "stateKey",
                        string_schema("State key. Defaults to the empty string, which is what most state events use."),
                    ),
                    (
                        "content",
                        generic_object_schema("Complete content for the state event. Replaces what is there."),
                    ),
                ],
                &["roomIdOrAlias", "eventType", "content"],
            )
        },
        output_schema: || getter_output_schema("eventId", "Event ID of the state event that was sent."),
        handler: handle_rooms_set_state,
    },
    ToolDefinition {
        name: "rooms_set_name",
        title: "Set Room Name",
        description: "Set a room's name. Passing an empty name clears it.",
        access: ToolAccess::Write,
        destructive: false,
        idempotent: true,
        open_world: true,
        input_schema: || {
            object_schema(
                vec![
                    ("roomIdOrAlias", string_schema("Room ID or room alias.")),
                    ("name", string_schema("New room name. Empty clears the name.")),
                ],
                &["roomIdOrAlias", "name"],
            )
        },
        output_schema: || {
            object_schema(
                vec![
                    ("ok", json!({"type": "boolean"})),
                    ("roomIdOrAlias", string_schema("Room ID or alias that was changed.")),
                    ("name", string_schema("Name that was set.")),
                ],
                &["ok", "roomIdOrAlias", "name"],
            )
        },
        handler: handle_rooms_set_name,
    },
    ToolDefinition {
        name: "rooms_set_topic",
        title: "Set Room Topic",
        description: "Set a room's topic. Passing an empty topic clears it.",
        access: ToolAccess::Write,
        destructive: false,
        idempotent: true,
        open_world: true,
        input_schema: || {
            object_schema(
                vec![
                    ("roomIdOrAlias", string_schema("Room ID or room alias.")),
                    ("topic", string_schema("New room topic. Empty clears the topic.")),
                ],
                &["roomIdOrAlias", "topic"],
            )
        },
        output_schema: || {
            object_schema(
                vec![
                    ("ok", json!({"type": "boolean"})),
                    ("roomIdOrAlias", string_schema("Room ID or alias that was changed.")),
                    ("topic", string_schema("Topic that was set.")),
                ],
                &["ok", "roomIdOrAlias", "topic"],
            )
        },
        handler: handle_rooms_set_topic,
    },
    ToolDefinition {
        name: "rooms_set_power_level",
        title: "Set User Power Level",
        description: "Set one user's power level in a room. Reads m.room.power_levels, changes \
this one user and writes it back, so every other level in the room is preserved. Prefer this over \
rooms_set_state for power levels.",
        access: ToolAccess::Write,
        destructive: false,
        idempotent: true,
        open_world: true,
        input_schema: || {
            object_schema(
                vec![
                    ("roomIdOrAlias", string_schema("Room ID or room alias.")),
                    ("userId", string_schema("Matrix user ID whose power level changes.")),
                    (
                        "powerLevel",
                        integer_schema("New power level. 0 is default, 50 is usually moderator, 100 is usually admin."),
                    ),
                ],
                &["roomIdOrAlias", "userId", "powerLevel"],
            )
        },
        output_schema: || {
            object_schema(
                vec![
                    ("ok", json!({"type": "boolean"})),
                    ("roomIdOrAlias", string_schema("Room ID or alias that was changed.")),
                    ("userId", string_schema("Matrix user ID that was changed.")),
                    ("powerLevel", integer_schema("Power level that was set.")),
                ],
                &["ok", "roomIdOrAlias", "userId", "powerLevel"],
            )
        },
        handler: handle_rooms_set_power_level,
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
        description: "Send a text or notice message to a room through the running Komai profile. \
Returns the real event ID, so a reply can be matched to the message that prompted it.",
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
            Ok(json!({"matchCount": 1, "rooms": [
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
            ]})),
        );

        let result = call_tool(&backend, AccessMode::ReadOnly, "rooms_list", None).unwrap();

        assert!(result["structuredContent"]["rooms"].is_array());
        assert_eq!(
            result["structuredContent"]["rooms"][0]["id"].as_str(),
            Some("!room:example.org")
        );
        assert_eq!(result["structuredContent"]["matchCount"].as_i64(), Some(1));
        assert_eq!(result["content"][0]["type"].as_str(), Some("text"));

        // The tool pages by default even when the caller asks for nothing.
        let calls = backend.calls.borrow();
        assert_eq!(calls[0].1["limit"].as_i64(), Some(50));
    }

    #[test]
    fn rooms_list_maps_filters_to_the_ipc_protocol() {
        let backend = MockBackend::with_response(
            "rooms.list",
            Ok(json!({"rooms": [], "matchCount": 0})),
        );

        call_tool(
            &backend,
            AccessMode::ReadOnly,
            "rooms_list",
            Some(json!({
                "ids": ["!a:example.org", "#b:example.org"],
                "query": "team",
                "isDm": false,
                "encrypted": true,
                "tag": "m.favourite",
                "parentSpace": "!space:example.org",
                "minMemberCount": 3,
                "limit": 5,
                "offset": 10,
                "fields": ["id", "name"]
            })),
        )
        .unwrap();

        let calls = backend.calls.borrow();
        let (method, params) = &calls[0];
        assert_eq!(method, "rooms.list");
        assert_eq!(params["ids"], json!(["!a:example.org", "#b:example.org"]));
        assert_eq!(params["query"].as_str(), Some("team"));
        assert_eq!(params["isDm"].as_bool(), Some(false));
        assert_eq!(params["encrypted"].as_bool(), Some(true));
        assert_eq!(params["tag"].as_str(), Some("m.favourite"));
        assert_eq!(params["parentSpace"].as_str(), Some("!space:example.org"));
        assert_eq!(params["minMemberCount"].as_i64(), Some(3));
        assert_eq!(params["limit"].as_i64(), Some(5));
        assert_eq!(params["offset"].as_i64(), Some(10));
        assert_eq!(params["fields"], json!(["id", "name"]));
    }

    #[test]
    fn rooms_list_says_when_the_page_is_a_subset() {
        let backend = MockBackend::with_response(
            "rooms.list",
            Ok(json!({"rooms": [{"id": "!a:example.org"}], "matchCount": 312})),
        );

        let result = call_tool(&backend, AccessMode::ReadOnly, "rooms_list", None).unwrap();

        let text = result["content"][0]["text"].as_str().unwrap();
        assert!(text.contains("1 of 312"), "unexpected summary: {text}");
        assert!(text.contains("offset"), "unexpected summary: {text}");
    }

    #[test]
    fn rooms_list_rejects_an_unknown_field() {
        let backend = MockBackend::default();

        let error = call_tool(
            &backend,
            AccessMode::ReadOnly,
            "rooms_list",
            Some(json!({ "fields": ["id", "nmae"] })),
        )
        .unwrap_err();

        let CallToolError::InvalidParams(message) = error else {
            panic!("expected invalid params");
        };
        assert!(message.contains("unknown key 'nmae'"), "{message}");
        assert!(message.contains("memberCount"), "{message}");
    }

    #[test]
    fn rooms_list_rejects_a_negative_offset() {
        let backend = MockBackend::default();

        let error = call_tool(
            &backend,
            AccessMode::ReadOnly,
            "rooms_list",
            Some(json!({ "offset": -1 })),
        )
        .unwrap_err();

        assert_eq!(
            error,
            CallToolError::InvalidParams("Argument 'offset' must not be negative.".to_owned())
        );
    }

    #[test]
    fn rooms_get_timeline_defaults_to_fetching_from_the_server() {
        let backend = MockBackend::with_response(
            "rooms.timeline",
            Ok(json!({"roomId": "!r:example.org", "events": [], "hasMore": false,
                      "nextBeforeEventId": null})),
        );

        call_tool(
            &backend,
            AccessMode::ReadOnly,
            "rooms_get_timeline",
            Some(json!({ "roomIdOrAlias": "!r:example.org" })),
        )
        .unwrap();

        let calls = backend.calls.borrow();
        assert_eq!(
            calls[0].1["fetchMode"].as_str(),
            Some("server_fetch_if_needed")
        );
    }

    #[test]
    fn rooms_get_timeline_still_honours_an_explicit_cached_only() {
        let backend = MockBackend::with_response(
            "rooms.timeline",
            Ok(json!({"roomId": "!r:example.org", "events": [], "hasMore": false,
                      "nextBeforeEventId": null})),
        );

        call_tool(
            &backend,
            AccessMode::ReadOnly,
            "rooms_get_timeline",
            Some(json!({ "roomIdOrAlias": "!r:example.org", "fetchMode": "cached_only" })),
        )
        .unwrap();

        let calls = backend.calls.borrow();
        assert_eq!(calls[0].1["fetchMode"].as_str(), Some("cached_only"));
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
    fn rooms_create_defaults_the_preset_and_omits_unset_fields() {
        let backend = MockBackend::with_response(
            "rooms.create",
            Ok(json!({ "roomId": "!new:example.org" })),
        );

        let result = call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_create",
            Some(json!({ "name": "Moderators" })),
        )
        .unwrap();

        let calls = backend.calls.borrow();
        let (method, params) = &calls[0];
        assert_eq!(method, "rooms.create");
        assert_eq!(params["name"].as_str(), Some("Moderators"));
        assert_eq!(params["preset"].as_str(), Some("private_chat"));
        for absent in [
            "topic",
            "invite",
            "isSpace",
            "roomVersion",
            "powerLevelContentOverride",
            "initialState",
            "creationContent",
        ] {
            assert!(params.get(absent).is_none(), "{absent} should be omitted");
        }
        assert_eq!(
            result["structuredContent"]["roomId"].as_str(),
            Some("!new:example.org")
        );
    }

    #[test]
    fn rooms_create_passes_raw_json_fields_through_untouched() {
        let backend = MockBackend::with_response(
            "rooms.create",
            Ok(json!({ "roomId": "!new:example.org" })),
        );

        let power_levels = json!({"users": {"@mod:example.org": 100}, "events_default": 0});
        let initial_state = json!([
            {"type": "m.room.history_visibility", "content": {"history_visibility": "invited"}}
        ]);
        let creation_content = json!({"m.federate": false});

        call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_create",
            Some(json!({
                "preset": "trusted_private_chat",
                "invite": ["@mod:example.org"],
                "roomVersion": "12",
                "powerLevelContentOverride": power_levels,
                "initialState": initial_state,
                "creationContent": creation_content
            })),
        )
        .unwrap();

        let calls = backend.calls.borrow();
        let (_, params) = &calls[0];
        assert_eq!(params["preset"].as_str(), Some("trusted_private_chat"));
        assert_eq!(params["invite"], json!(["@mod:example.org"]));
        assert_eq!(params["roomVersion"].as_str(), Some("12"));
        assert_eq!(params["powerLevelContentOverride"], power_levels);
        assert_eq!(params["initialState"], initial_state);
        assert_eq!(params["creationContent"], creation_content);
    }

    #[test]
    fn rooms_create_rejects_an_unknown_preset() {
        let backend = MockBackend::default();

        let error = call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_create",
            Some(json!({ "preset": "secret_chat" })),
        )
        .unwrap_err();

        assert_eq!(
            error,
            CallToolError::InvalidParams(
                "Argument 'preset' must be one of: private_chat, public_chat, trusted_private_chat."
                    .to_owned()
            )
        );
    }

    #[test]
    fn rooms_create_rejects_a_non_string_invite_entry() {
        let backend = MockBackend::default();

        let error = call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_create",
            Some(json!({ "invite": ["@alice:example.org", 7] })),
        )
        .unwrap_err();

        assert_eq!(
            error,
            CallToolError::InvalidParams(
                "Argument 'invite' must contain only strings.".to_owned()
            )
        );
    }

    #[test]
    fn rooms_create_takes_no_arguments_at_all() {
        let backend = MockBackend::with_response(
            "rooms.create",
            Ok(json!({ "roomId": "!new:example.org" })),
        );

        call_tool(&backend, AccessMode::ReadWrite, "rooms_create", None).unwrap();

        let calls = backend.calls.borrow();
        assert_eq!(calls[0].1["preset"].as_str(), Some("private_chat"));
    }

    #[test]
    fn rooms_get_state_defaults_the_state_key_to_empty() {
        let backend = MockBackend::with_response(
            "rooms.getState",
            Ok(json!({"exists": true, "content": {"topic": "hi"}})),
        );

        let result = call_tool(
            &backend,
            AccessMode::ReadOnly,
            "rooms_get_state",
            Some(json!({"roomIdOrAlias": "!r:example.org", "eventType": "m.room.topic"})),
        )
        .unwrap();

        let calls = backend.calls.borrow();
        let (method, params) = &calls[0];
        assert_eq!(method, "rooms.getState");
        assert_eq!(params["stateKey"].as_str(), Some(""));
        assert_eq!(
            result["structuredContent"]["content"]["topic"].as_str(),
            Some("hi")
        );
    }

    #[test]
    fn rooms_get_state_reports_absent_state_without_erroring() {
        let backend = MockBackend::with_response(
            "rooms.getState",
            Ok(json!({"exists": false, "content": {}})),
        );

        let result = call_tool(
            &backend,
            AccessMode::ReadOnly,
            "rooms_get_state",
            Some(json!({"roomIdOrAlias": "!r:example.org", "eventType": "com.example.custom"})),
        )
        .unwrap();

        assert!(result.get("isError").is_none() || result["isError"] == json!(false));
        assert_eq!(result["structuredContent"]["exists"].as_bool(), Some(false));
        assert!(result["content"][0]["text"]
            .as_str()
            .unwrap()
            .contains("has no com.example.custom"));
    }

    #[test]
    fn rooms_get_state_is_available_in_read_only_mode() {
        let names: Vec<String> = list_tools(AccessMode::ReadOnly)
            .into_iter()
            .map(|tool| tool["name"].as_str().unwrap().to_owned())
            .collect();

        assert!(names.contains(&"rooms_get_state".to_owned()));
        assert!(!names.contains(&"rooms_set_state".to_owned()));
    }

    #[test]
    fn rooms_set_state_passes_content_through_and_returns_an_event_id() {
        let backend = MockBackend::with_response(
            "rooms.setState",
            Ok(json!({"eventId": "$state:example.org"})),
        );

        let content = json!({"protected_rooms": ["!a:example.org"]});
        let result = call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_set_state",
            Some(json!({
                "roomIdOrAlias": "!r:example.org",
                "eventType": "fi.mau.meowlnir.protected_rooms",
                "content": content
            })),
        )
        .unwrap();

        let calls = backend.calls.borrow();
        let (method, params) = &calls[0];
        assert_eq!(method, "rooms.setState");
        assert_eq!(params["content"], content);
        assert_eq!(params["stateKey"].as_str(), Some(""));
        assert_eq!(
            result["structuredContent"]["eventId"].as_str(),
            Some("$state:example.org")
        );
    }

    #[test]
    fn rooms_set_state_requires_content() {
        let backend = MockBackend::default();

        let error = call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_set_state",
            Some(json!({"roomIdOrAlias": "!r:example.org", "eventType": "m.room.topic"})),
        )
        .unwrap_err();

        assert_eq!(
            error,
            CallToolError::InvalidParams("Missing required argument 'content'.".to_owned())
        );
    }

    #[test]
    fn rooms_set_state_is_annotated_destructive_but_the_wrappers_are_not() {
        let tools = list_tools(AccessMode::ReadWrite);
        let destructive = |name: &str| -> bool {
            tools
                .iter()
                .find(|tool| tool["name"].as_str() == Some(name))
                .expect("tool present")["annotations"]["destructiveHint"]
                .as_bool()
                .expect("destructiveHint present")
        };

        assert!(destructive("rooms_set_state"));
        assert!(!destructive("rooms_set_name"));
        assert!(!destructive("rooms_set_topic"));
        assert!(!destructive("rooms_set_power_level"));
    }

    #[test]
    fn empty_room_text_settings_clear_rather_than_fail() {
        let backend = MockBackend::with_response("rooms.setTopic", Ok(Value::Bool(true)));

        let result = call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_set_topic",
            Some(json!({"roomIdOrAlias": "!r:example.org", "topic": ""})),
        )
        .unwrap();

        let calls = backend.calls.borrow();
        assert_eq!(calls[0].1["topic"].as_str(), Some(""));
        assert!(result["content"][0]["text"]
            .as_str()
            .unwrap()
            .contains("Cleared"));
    }

    #[test]
    fn rooms_set_power_level_requires_an_explicit_level() {
        let backend = MockBackend::default();

        let error = call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_set_power_level",
            Some(json!({"roomIdOrAlias": "!r:example.org", "userId": "@a:example.org"})),
        )
        .unwrap_err();

        assert_eq!(
            error,
            CallToolError::InvalidParams("Missing required argument 'powerLevel'.".to_owned())
        );
    }

    #[test]
    fn rooms_set_power_level_accepts_a_negative_level() {
        let backend = MockBackend::with_response("rooms.setPowerLevel", Ok(Value::Bool(true)));

        call_tool(
            &backend,
            AccessMode::ReadWrite,
            "rooms_set_power_level",
            Some(json!({
                "roomIdOrAlias": "!r:example.org",
                "userId": "@a:example.org",
                "powerLevel": -1
            })),
        )
        .unwrap();

        let calls = backend.calls.borrow();
        assert_eq!(calls[0].1["powerLevel"].as_i64(), Some(-1));
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
